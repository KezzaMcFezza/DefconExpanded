import sys
import re
import os
from datetime import datetime
import discord
from discord.ext import commands, tasks
import asyncio
import aiofiles
import threading

DISCORD_TOKEN = 'MTMwNTQxNTU1NzM2NTEwNDY4MA.G6fE2Q.YywmUxfaA368tePVwCysMI_WAPC4p9OTD69f10'

last_update_time = datetime.now()
UPDATE_COOLDOWN = 0.5

intents = discord.Intents.default()
intents.message_content = True
intents.members = True
intents.presences = True
intents.guilds = True

bot = commands.Bot(command_prefix='!', intents=intents)

class GameState:
    def __init__(self, server_name):
        self.server_name = server_name
        self.active_game = False
        self.start_time = None
        self.player_info = {}
        self.games_played = 0
        self.start_timestamp = datetime.now()
        self.lobby_players = {}
        self.team_alliances = {}  # Store current alliance ID for each team
        self.player_teams = {}

    def get_duration_str(self):
        if not self.start_time:
            return "0:00"
        duration = datetime.now() - self.start_time
        minutes = int(duration.total_seconds() // 60)
        seconds = int(duration.total_seconds() % 60)
        return f"{minutes}:{seconds:02d}"

    def clear_game(self):
        self.active_game = False
        self.start_time = None
        self.player_info.clear()
        self.team_alliances.clear()
        self.player_teams.clear()

    def get_status_text(self):
        if not self.active_game and self.lobby_players:
            lobby_names = [name for name in self.lobby_players.values() if name]
            if lobby_names:
                if len(lobby_names) == 1:
                    return f"{lobby_names[0]} waiting in {self.server_name}"
                else:
                    players_text = ", ".join(lobby_names[:-1]) + f" and {lobby_names[-1]}"
                    return f"{players_text} waiting in {self.server_name}"

        if self.active_game:
            duration = self.get_duration_str()
            
            # First, group players by their alliance ID
            alliance_groups = {}
            for client_id, player in self.player_info.items():
                if 'name' in player and 'spectator' not in player and client_id in self.player_teams:
                    team = self.player_teams[client_id]
                    alliance_id = self.team_alliances.get(team)
                    if alliance_id is not None:
                        if alliance_id not in alliance_groups:
                            alliance_groups[alliance_id] = []
                        alliance_groups[alliance_id].append(player['name'])

            # Convert alliance groups to text
            teams_text = []
            for alliance_players in alliance_groups.values():
                if alliance_players:
                    teams_text.append(", ".join(sorted(alliance_players)))
            
            if teams_text:
                players_text = " vs ".join(teams_text)
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

def process_game_line(line, game_state):
    if 'SERVER_START' in line:
        game_state.lobby_players.clear()
        game_state.player_info.clear()
        
    elif 'CLIENT_NEW' in line and not game_state.active_game:
        match = re.search(r'CLIENT_NEW (\d+)', line)
        if match:
            client_id = int(match.group(1))
            game_state.player_info[client_id] = {'client_id': client_id}
            game_state.lobby_players[client_id] = None

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

    elif 'TEAM_ALLIANCE' in line:
        match = re.search(r'TEAM_ALLIANCE (\d+) (\d+)', line)
        if match:
            team1, team2 = map(int, match.groups())
            game_state.team_alliances[team1] = team2

    elif 'CLIENT_QUIT' in line or 'CLIENT_DROP' in line:
        match = re.search(r'CLIENT_(?:QUIT|DROP) (\d+)', line)
        if match:
            client_id = int(match.group(1))
            if client_id in game_state.lobby_players:
                del game_state.lobby_players[client_id]
            if client_id in game_state.player_info:
                del game_state.player_info[client_id]
            if client_id in game_state.player_teams:
                del game_state.player_teams[client_id]

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
                        game_state.lobby_players.clear()
                        print(f"Server started. Resetting game state for {game_state.server_name}")
                        await update_bot_activity()

                    elif 'GAME_START' in line:
                        game_state.lobby_players.clear()
                        game_state.active_game = True
                        game_state.start_time = datetime.now()
                        print(f"Game started on {game_state.server_name}")
                        await update_bot_activity()

                    elif 'GAME_END' in line:
                        game_state.clear_game()
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

@tasks.loop(seconds=10)
async def rotate_status():
    await update_bot_activity()

@bot.event
async def on_ready():
    print(f"Discord bot connected as {bot.user}")
    rotate_status.start()
    await update_bot_activity()

@bot.command(name='stats')
async def server_stats(ctx):
    """Display stats for all servers"""
    stats = server_manager.get_server_stats()
    embed = discord.Embed(title="Server Statistics", color=0x00ff00)
    
    for server_name, server_stats in stats.items():
        status = "🟢 Active" if server_stats['active'] else "⚪ Inactive"
        embed.add_field(
            name=server_name,
            value=f"Status: {status}\nUptime: {server_stats['uptime']}\nGames: {server_stats['games_played']}",
            inline=False
        )
    
    await ctx.send(embed=embed)

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
        print("Server config format: log_file:server_name")
        sys.exit(1)

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    try:
        tasks = []
        
        for server_config in sys.argv[1:]:
            try:
                log_file, server_name = server_config.split(':')
                server_manager.add_server(log_file, server_name)
                tasks.append(loop.create_task(
                    monitor_game_events(log_file, server_manager.servers[log_file])
                ))
            except Exception as e:
                print(f"Error setting up server {server_config}: {e}")

        print(f"Monitoring {len(tasks)} servers...")
        
        def run_bot():
            try:
                bot.run(DISCORD_TOKEN)
            except Exception as e:
                print(f"Discord bot error: {e}")

        bot_thread = threading.Thread(target=run_bot)
        bot_thread.daemon = True
        bot_thread.start()

        loop.run_until_complete(asyncio.gather(*tasks, return_exceptions=True))
    except KeyboardInterrupt:
        print("Shutting down due to keyboard interrupt...")
    finally:
        loop.run_until_complete(shutdown())
        loop.close()