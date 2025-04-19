import sys
import re
import os
from datetime import datetime, timedelta
import discord
from discord.ext import commands, tasks
import asyncio
import aiofiles
import traceback
from dotenv import load_dotenv

load_dotenv()

DISCORD_TOKEN = os.getenv('DISCORD_BOT_TOKEN')
ALERT_CHANNEL_ID = int(os.getenv('DISCORD_CHANNEL_IDS').split(',')[0])
ALERT_ROLE_ID = int(os.getenv('DISCORD_ALERT_ROLE_ID', '688701291832017047'))
UPDATE_COOLDOWN = float(os.getenv('UPDATE_COOLDOWN', '0.5'))
TIMEOUT_MINUTES = int(os.getenv('TIMEOUT_MINUTES', '5'))
TIMEOUT_DURATION = timedelta(minutes=TIMEOUT_MINUTES)

SERVER_NAME_MAPPING = {
    "1V1": "1v1 Random",
    "1V1BEST": "1v1 Best Setups",
    "1V1BEST2": "1v1 Best Setups",
    "1V1DEFAULT": "1v1 Default",
    "NOOB": "New Player Server",
    "TOURNAMENT": "2v2 Tournament",
    "2V2": "2v2 Random",
    "6PLAYERFFA": "FFA Server",
    "3V3_FFA": "3v3 Random",
    "509CG1V1": "509 CG 1v1",
    "509CG2V2": "509 CG 2v2",
    "1V1MURICON": "Muricon 1v1",
    "8PLAYER4V4": "8 Player 4v4",
    "8PLAYERDIPLO": "8 Player Diplo",
    "10PLAYER5V5": "10 Player 5v5",
    "10PLAYERDIPLO": "10 Player Diplo",
    "16PLAYER": "16 Player Server",
    "SONYHOOV": "Sony and Hoovs Hideout"
}

player_timeouts = {}
last_update_time = datetime.now()

intents = discord.Intents.default()
intents.message_content = True
intents.members = True
intents.presences = True
intents.guilds = True
bot = commands.Bot(command_prefix='!', intents=intents)

class PlayerIdentifier:
    def __init__(self, client_id, key_id=None, ip=None):
        self.client_id = client_id
        self.key_id = key_id
        self.ip = ip
        
    def matches(self, other):
        if not isinstance(other, PlayerIdentifier):
            return False
        return (
            (self.key_id and other.key_id and self.key_id == other.key_id) or
            (self.ip and other.ip and self.ip == other.ip) or
            (self.client_id == other.client_id)
        )

class GameState:
    def __init__(self, server_name):
        self.server_name = server_name
        self.active_game = False
        self.start_time = None
        self.player_info = {}
        self.games_played = 0
        self.start_timestamp = datetime.now()
        self.lobby_players = {}
        self.team_alliances = {}  
        self.player_teams = {}    
        self.territories = {}     
        self.team_territories = {}
        self.active_players = {}
        self.player_identifiers = {}
        self.game_started = False

    def get_duration_str(self):
        if not self.start_time:
            return "0:00"
        duration = datetime.now() - self.start_time
        minutes = int(duration.total_seconds() // 60)
        seconds = int(duration.total_seconds() % 60)
        return f"{minutes}:{seconds:02d}"

    def clear_game(self):
        self.active_game = False
        self.game_started = False
        self.start_time = None
        self.player_info.clear()
        self.team_alliances.clear()
        self.player_teams.clear()
        self.territories.clear()
        self.team_territories.clear()
        self.active_players.clear()
        self.player_identifiers.clear()
        self.lobby_players.clear()

    def get_status_text(self):
        if not self.game_started and self.lobby_players:
            lobby_names = [name for name in self.lobby_players.values() if name]
            if lobby_names:
                if len(lobby_names) == 1:
                    return f"{lobby_names[0]} waiting in {self.server_name}"
                else:
                    players_text = ", ".join(lobby_names[:-1]) + f" and {lobby_names[-1]}"
                    return f"{players_text} waiting in {self.server_name}"

        if self.game_started:
            duration = self.get_duration_str()
            active_players = []
            
            for client_id, team in self.player_teams.items():
                if team in self.team_territories and client_id in self.player_info:
                    player = self.player_info[client_id]
                    if 'name' in player:
                        active_players.append(player['name'])
            
            if active_players:
                if len(active_players) == 2:
                    players_text = " vs ".join(active_players)
                else:
                    players_text = ", ".join(active_players)
                return f"{self.server_name} {duration} | {players_text}"

        return "Watching for Defcon games"

class ServerManager:
    def __init__(self):
        self.servers = {}
        self.status_index = 0
        self.last_update = {}
        
    def add_server(self, log_file, server_name):
        self.servers[log_file] = GameState(server_name)
        self.last_update[log_file] = 0
        
    def get_active_servers(self):
        return [server for server in self.servers.values() if server.active_game]
        
    def get_next_status(self):
        active_servers = self.get_active_servers()

        for server in self.servers.values():
            if not server.active_game and server.lobby_players:
                status = server.get_status_text()
                if status:
                    return status
        
        if not active_servers:
            return None
            
        if len(active_servers) == 1:
            return active_servers[0].get_status_text()
        
        grouped_servers = {}
        for server in active_servers:
            server_type = server.server_name.split()[0]
            if server_type not in grouped_servers:
                grouped_servers[server_type] = []
            grouped_servers[server_type].append(server)
        
        total_games = len(active_servers)
        server_counts = [f"{len(servers)}x {type}" for type, servers in grouped_servers.items()]
        summary = f"{total_games} Active Games: {' | '.join(server_counts)}"
        
        self.status_index = (self.status_index + 1) % (len(active_servers) + 1)
        if self.status_index == 0:
            return summary
        return active_servers[self.status_index - 1].get_status_text()

    def get_server_stats(self):
        return {
            server.server_name: {
                'uptime': str(datetime.now() - server.start_timestamp).split('.')[0],
                'games_played': server.games_played,
                'active': server.active_game
            }
            for server in self.servers.values()
        }

server_manager = ServerManager()

def process_game_line(line, game_state):
    if 'SERVER_START' in line:
        game_state.clear_game()
        
    elif 'CLIENT_NEW' in line:
        match = re.search(r'CLIENT_NEW (\d+) (\S+) (\S+)', line)
        if match:
            client_id = int(match.group(1))
            key_id = match.group(2)
            ip = match.group(3)
            
            game_state.player_identifiers[key_id] = client_id
            
            if not game_state.game_started:
                game_state.player_info[client_id] = {
                    'client_id': client_id,
                    'key_id': key_id,
                    'ip': ip
                }
                game_state.lobby_players[client_id] = None
            else:
                for _, active_player in game_state.active_players.items():
                    if active_player.get('key_id') == key_id:
                        game_state.player_info[client_id] = {
                            'client_id': client_id,
                            'key_id': key_id,
                            'ip': ip,
                            'name': active_player.get('name')
                        }
                        break

    elif 'CLIENT_NAME' in line:
        match = re.search(r'CLIENT_NAME (\d+) (.*)', line)
        if match:
            client_id, name = int(match.group(1)), match.group(2).strip()
            if client_id in game_state.player_info:
                game_state.player_info[client_id]['name'] = name
            if client_id in game_state.lobby_players:
                game_state.lobby_players[client_id] = name

    elif 'TEAM_ENTER' in line:
        match = re.search(r'TEAM_ENTER (\d+) (\d+)', line)
        if match:
            team, client_id = map(int, match.groups())
            if client_id in game_state.player_info:
                game_state.player_teams[client_id] = team

    elif 'TEAM_RECONNECT' in line:
        match = re.search(r'TEAM_RECONNECT (\d+) (\d+)', line)
        if match:
            team, client_id = map(int, match.groups())
            if client_id in game_state.player_info:
                game_state.player_teams[client_id] = team
                game_state.active_game = True  

    elif 'TEAM_TERRITORY' in line:
        match = re.search(r'TEAM_TERRITORY (\d+) (\d+)', line)
        if match:
            team, territory = map(int, match.groups())
            game_state.team_territories[team] = territory
            game_state.territories[territory] = team

    elif 'CLIENT_QUIT' in line or 'CLIENT_DROP' in line:
        match = re.search(r'CLIENT_(?:QUIT|DROP) (\d+)', line)
        if match:
            client_id = int(match.group(1))
            if client_id in game_state.lobby_players:
                del game_state.lobby_players[client_id]
            if client_id in game_state.player_info:
                if game_state.game_started:
                    game_state.active_players[client_id] = game_state.player_info[client_id].copy()
                del game_state.player_info[client_id]

    elif 'GAME_START' in line:
        game_state.game_started = True
        game_state.active_game = True
        game_state.start_time = datetime.now()
        game_state.active_players = game_state.player_info.copy()
        game_state.lobby_players.clear()

    elif 'TEAM_ABANDON' in line:
        match = re.search(r'TEAM_ABANDON (\d+)', line)
        if match:
            team = int(match.group(1))
            pass

async def monitor_game_events(log_file, game_state):
    print(f"Monitoring: {log_file} for {game_state.server_name}")
    
    while True:
        try:
            while not os.path.exists(log_file):
                print(f"Waiting for log file: {log_file}")
                await asyncio.sleep(5)
                continue

            async with aiofiles.open(log_file, 'r') as f:
                await f.seek(0, os.SEEK_END)
                
                while True:
                    line = await f.readline()
                    if not line:
                        await asyncio.sleep(1)
                        continue

                    if 'SERVER_START' in line:
                        game_state.clear_game()
                        print(f"Server started. Resetting game state for {game_state.server_name}")
                        await update_bot_activity()

                    elif 'GAME_START' in line:
                        game_state.lobby_players.clear()
                        game_state.active_game = True
                        game_state.start_time = datetime.now()
                        print(f"Game started on {game_state.server_name}")
                        await update_bot_activity()

                    elif 'GAME_END' in line:
                        game_state.games_played += 1
                        print(f"Game ended on {game_state.server_name}")
                        await update_bot_activity()
                    
                    process_game_line(line, game_state)
                    
                    if game_state.active_game or game_state.lobby_players:
                        await update_bot_activity()

        except FileNotFoundError:
            print(f"Log file disappeared: {log_file}, waiting for it to return...")
            await asyncio.sleep(5)
        except Exception as e:
            print(f"Error monitoring {log_file}: {e}")
            await asyncio.sleep(5)

async def monitor_server_output(output_file, bot):
    print(f"Starting monitor for output file: {output_file}")
    internal_name = output_file.replace('server_output_', '').replace('.log', '').upper()
    friendly_name = SERVER_NAME_MAPPING.get(internal_name, internal_name)
    print(f"Monitoring server: {friendly_name}")
    
    while not bot.is_ready():
        await asyncio.sleep(1)
    
    last_position = os.path.getsize(output_file) if os.path.exists(output_file) else 0

    while True:
        try:
            if not os.path.exists(output_file):
                print(f"Waiting for output file: {output_file}")
                await asyncio.sleep(5)
                continue

            current_size = os.path.getsize(output_file)
            if current_size < last_position:
                last_position = 0

            if current_size > last_position:
                try:
                    with open(output_file, 'rb') as f:
                        f.seek(last_position)
                        new_data = f.read()
                        new_lines = new_data.decode('latin1').splitlines()
                        
                        for line in new_lines:
                            if '/gamealert' in line:
                                print(f"Found gamealert in line: {line.strip()}")
                                match = re.search(r'client \d+ \((.*?)\) : /gamealert(.*)', line.strip())
                                if match and 'Unknown chat command' not in line:
                                    player_name = match.group(1)
                                    
                                    if player_name in player_timeouts:
                                        time_remaining = player_timeouts[player_name] - datetime.now()
                                        if time_remaining > timedelta():
                                            print(f"Player {player_name} is on timeout. {time_remaining.seconds} seconds remaining.")
                                            continue
                                        else:
                                            del player_timeouts[player_name]
                                    
                                    player_timeouts[player_name] = datetime.now() + TIMEOUT_DURATION
                                    
                                    alert_message = match.group(2).strip() or "Join now before you make him sad :)"
                                    print(f"Matched gamealert command from player: {player_name} with message: {alert_message}")
                                    asyncio.create_task(process_game_alert(bot, player_name, friendly_name, alert_message))
                
                    last_position = current_size
                except Exception as e:
                    print(f"Error processing file: {e}")
                    traceback.print_exc()
            
            await asyncio.sleep(0.1)
                
        except Exception as e:
            print(f"Monitor error: {e}")
            traceback.print_exc()
            await asyncio.sleep(5)

async def process_game_alert(bot, player_name, server_name, alert_message):
    try:
        alert_channel = bot.get_channel(ALERT_CHANNEL_ID)
        if alert_channel:
            print(f"Found alert channel: {alert_channel.name}")
            
            role_mention = f"<@&{ALERT_ROLE_ID}>"
            
            embed = discord.Embed(
                title="**Game Alert!**",
                color=discord.Color.blue(),
                timestamp=datetime.now()
            )
            embed.add_field(
                name="**Player**",
                value=f"*{player_name}*",
                inline=True
            )
            embed.add_field(
                name="**Server**",
                value=f"*{server_name}*",
                inline=True
            )
            embed.set_footer(
                text="DefconExpanded",
                icon_url=bot.user.avatar.url if bot.user.avatar else None
            )
            embed.description = alert_message
            
            await alert_channel.send(content=role_mention, embed=embed)
            print(f"Successfully sent alert for {player_name} on {server_name} with message: {alert_message}")
            
    except discord.errors.Forbidden:
        print(f"Bot does not have permission to send messages in channel")
    except Exception as e:
        print(f"Error in send_alert_message: {e}")
        traceback.print_exc()

async def update_bot_activity():
    global last_update_time
    
    if (datetime.now() - last_update_time).total_seconds() < UPDATE_COOLDOWN:
        return
        
    try:
        status_text = server_manager.get_next_status()
        
        if not status_text:
            activity = discord.CustomActivity(name="Watching for Defcon games")
        elif "waiting" in status_text:
            activity = discord.CustomActivity(name=status_text)
        else:
            if " | " in status_text:
                server_info, player_info = status_text.split(" | ", 1)
                activity = discord.Activity(
                    type=discord.ActivityType.competing,
                    name=server_info,
                    state=player_info,
                    details="DefconExpanded"
                )
            else:
                activity = discord.Activity(
                    type=discord.ActivityType.competing,
                    name=status_text
                )
                
        await bot.change_presence(activity=activity)
        last_update_time = datetime.now()
    except Exception as e:
        print(f"Error updating presence: {e}")

@tasks.loop(seconds=10)
async def rotate_status():
    await update_bot_activity()

@bot.event
async def on_ready():
    print(f"Discord bot connected as {bot.user}")
    rotate_status.start()
    await update_bot_activity()

@tasks.loop(minutes=30)
async def cleanup_timeouts():
    current_time = datetime.now()
    expired = [player for player, timeout_time in player_timeouts.items() 
               if timeout_time <= current_time]
    
    for player in expired:
        del player_timeouts[player]
    
    if expired:
        print(f"Cleaned up {len(expired)} expired player timeouts")

async def shutdown():
    print("Shutting down gracefully...")
    
    for game_state in server_manager.servers.values():
        game_state.clear_game()
    
    try:
        await bot.change_presence(activity=discord.Activity(
            type=discord.ActivityType.watching,
            name="Server restarting..."
        ))
    except:
        pass
    
    await bot.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python liveservertracker.py <server_config>")
        print("Server config format: game_events_file:server_name [server_output_file]")
        sys.exit(1)

    try:
        tasks = []
        
        for arg in sys.argv[1:]:
            try:
                if ':' in arg:  
                    log_file, server_name = arg.split(':')
                    server_manager.add_server(log_file, server_name)
                    tasks.append(monitor_game_events(log_file, server_manager.servers[log_file]))
                elif arg.startswith('server_output_'): 
                    print(f"Setting up output file monitoring for: {arg}")
                    tasks.append(monitor_server_output(arg, bot))
            except Exception as e:
                print(f"Error processing argument {arg}: {e}")

        num_game_events = len([t for t in tasks if 'monitor_game_events' in str(t)])
        num_outputs = len([t for t in tasks if 'monitor_server_output' in str(t)])
        print(f"Monitoring {num_game_events} game event files and {num_outputs} server output files...")

        @bot.event
        async def on_ready():
            print(f"Discord bot connected as {bot.user}")
            rotate_status.start()
            cleanup_timeouts.start()
            await update_bot_activity()

        async def main():
            await asyncio.gather(bot.start(DISCORD_TOKEN), *tasks)

        asyncio.run(main())

    except KeyboardInterrupt:
        print("Shutting down due to keyboard interrupt...")
        asyncio.run(shutdown())