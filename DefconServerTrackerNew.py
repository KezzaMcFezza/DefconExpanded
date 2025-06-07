import sys
import re
import os
import logging
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Set, Tuple, Any, AsyncIterator
from dataclasses import dataclass, field
from contextlib import asynccontextmanager
from pathlib import Path
import asyncio
from enum import Enum

import discord
from discord.ext import commands, tasks
import aiofiles
from dotenv import load_dotenv

class ConfigurationError(Exception):
    pass

@dataclass
class BotConfig:
    discord_token: str
    alert_channel_id: int
    alert_role_id: int
    update_cooldown: float = 0.5
    timeout_minutes: int = 5
    status_rotation_seconds: int = 10
    cleanup_interval_minutes: int = 30
    file_check_interval: float = 1.0
    log_level: str = "INFO"
    
    @classmethod
    def from_environment(cls) -> 'BotConfig':
        load_dotenv()
        
        required_vars = {
            'DISCORD_BOT_TOKEN': 'discord_token',
            'ALERT_CHANNEL_ID': 'alert_channel_id', 
            'DISCORD_ALERT_ROLE_ID': 'alert_role_id'
        }
        
        config_data = {}
        
        for env_var, config_key in required_vars.items():
            value = os.getenv(env_var)
            if not value:
                raise ConfigurationError(f"Required environment variable {env_var} not found")
            
            if config_key in ['alert_channel_id', 'alert_role_id']:
                try:
                    if config_key == 'alert_channel_id' and ',' in value:
                        value = value.split(',')[0]
                    config_data[config_key] = int(value)
                except ValueError:
                    raise ConfigurationError(f"Invalid integer value for {env_var}: {value}")
            else:
                config_data[config_key] = value
                
        optional_vars = {
            'UPDATE_COOLDOWN': ('update_cooldown', float),
            'TIMEOUT_MINUTES': ('timeout_minutes', int),
            'LOG_LEVEL': ('log_level', str)
        }
        
        for env_var, (config_key, converter) in optional_vars.items():
            value = os.getenv(env_var)
            if value:
                try:
                    config_data[config_key] = converter(value)
                except ValueError:
                    raise ConfigurationError(f"Invalid {converter.__name__} value for {env_var}: {value}")
        
        return cls(**config_data)

@dataclass
class PlayerIdentifier:
    client_id: int
    key_id: Optional[str] = None
    ip: Optional[str] = None
    
    def matches(self, other: 'PlayerIdentifier') -> bool:
        if not isinstance(other, PlayerIdentifier):
            return False
        
        return (
            (self.key_id and other.key_id and self.key_id == other.key_id) or
            (self.ip and other.ip and self.ip == other.ip) or
            (self.client_id == other.client_id)
        )

@dataclass
class PlayerInfo:
    client_id: int
    name: Optional[str] = None
    key_id: Optional[str] = None
    ip: Optional[str] = None
    team: Optional[int] = None
    is_active: bool = False


class GamePhase(Enum):
    LOBBY = "lobby"
    ACTIVE = "active"
    ENDED = "ended"

@dataclass
class GameState:
    server_name: str
    phase: GamePhase = GamePhase.LOBBY
    start_time: Optional[datetime] = None
    players: Dict[int, PlayerInfo] = field(default_factory=dict)
    team_territories: Dict[int, int] = field(default_factory=dict)
    games_played: int = 0
    
    def get_duration_str(self) -> str:
        if not self.start_time:
            return "0:00"
        
        duration = datetime.now() - self.start_time
        minutes = int(duration.total_seconds() // 60)
        seconds = int(duration.total_seconds() % 60)
        return f"{minutes}:{seconds:02d}"
    
    def get_active_players(self) -> List[PlayerInfo]:
        return [player for player in self.players.values() if player.is_active and player.name]
    
    def get_lobby_players(self) -> List[PlayerInfo]:
        return [player for player in self.players.values() if player.name and self.phase == GamePhase.LOBBY]

class LoggingService:
    @staticmethod
    def setup_logging(level: str = "INFO") -> logging.Logger:
        logging.basicConfig(
            level=getattr(logging, level.upper()),
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.StreamHandler(),
                logging.FileHandler('defcon_tracker.log')
            ]
        )
        return logging.getLogger('DefconTracker')

class PlayerTimeoutService:
    def __init__(self, timeout_duration: timedelta):
        self._timeouts: Dict[str, datetime] = {}
        self._timeout_duration = timeout_duration
        self._logger = logging.getLogger(f"{__name__}.PlayerTimeoutService")
    
    def is_player_timed_out(self, player_name: str) -> bool:
        if player_name not in self._timeouts:
            return False
        
        if self._timeouts[player_name] <= datetime.now():
            del self._timeouts[player_name]
            return False
        
        return True
    
    def get_remaining_timeout(self, player_name: str) -> Optional[timedelta]:
        if player_name not in self._timeouts:
            return None
        
        remaining = self._timeouts[player_name] - datetime.now()
        return remaining if remaining > timedelta() else None
    
    def apply_timeout(self, player_name: str) -> None:
        self._timeouts[player_name] = datetime.now() + self._timeout_duration
        self._logger.info(f"Applied timeout to player {player_name}")
    
    def cleanup_expired_timeouts(self) -> int:
        current_time = datetime.now()
        expired = [player for player, timeout_time in self._timeouts.items() 
                  if timeout_time <= current_time]
        
        for player in expired:
            del self._timeouts[player]
        
        if expired:
            self._logger.info(f"Cleaned up {len(expired)} expired timeouts")
        
        return len(expired)


class GameEventParser:
    def __init__(self):
        self._logger = logging.getLogger(f"{__name__}.GameEventParser")
        
        self._patterns = {
            'client_new': re.compile(r'CLIENT_NEW (\d+) (\S+) (\S+)'),
            'client_name': re.compile(r'CLIENT_NAME (\d+) (.*)'),
            'team_enter': re.compile(r'TEAM_ENTER (\d+) (\d+)'),
            'team_reconnect': re.compile(r'TEAM_RECONNECT (\d+) (\d+)'),
            'team_territory': re.compile(r'TEAM_TERRITORY (\d+) (\d+)'),
            'client_quit': re.compile(r'CLIENT_(?:QUIT|DROP) (\d+)'),
            'team_abandon': re.compile(r'TEAM_ABANDON (\d+)'),
            'gamealert': re.compile(r'client \d+ \((.*?)\) : /gamealert(.*)')
        }
    
    def parse_client_new(self, line: str) -> Optional[PlayerIdentifier]:
        match = self._patterns['client_new'].search(line)
        if match:
            client_id = int(match.group(1))
            key_id = match.group(2)
            ip = match.group(3)
            return PlayerIdentifier(client_id=client_id, key_id=key_id, ip=ip)
        return None
    
    def parse_client_name(self, line: str) -> Optional[Tuple[int, str]]:
        match = self._patterns['client_name'].search(line)
        if match:
            client_id = int(match.group(1))
            name = match.group(2).strip()
            return client_id, name
        return None
    
    def parse_team_enter(self, line: str) -> Optional[Tuple[int, int]]:
        match = self._patterns['team_enter'].search(line)
        if match:
            team = int(match.group(1))
            client_id = int(match.group(2))
            return team, client_id
        return None
    
    def parse_gamealert(self, line: str) -> Optional[Tuple[str, str]]:
        if 'Unknown chat command' in line:
            return None
            
        match = self._patterns['gamealert'].search(line.strip())
        if match:
            player_name = match.group(1)
            alert_message = match.group(2).strip() or "Join now before you make him sad :)"
            return player_name, alert_message
        return None


class FileMonitorService:
    def __init__(self, check_interval: float = 1.0):
        self._check_interval = check_interval
        self._logger = logging.getLogger(f"{__name__}.FileMonitorService")
        self._warned_files: Set[Path] = set()
    
    @asynccontextmanager
    async def monitor_file_changes(self, file_path: Path) -> AsyncIterator[AsyncIterator[str]]:
        # Debug: Log the absolute path and working directory
        abs_path = file_path.absolute()
        cwd = Path.cwd()
        self._logger.info(f"Monitoring file: {file_path} (absolute: {abs_path})")
        self._logger.info(f"Current working directory: {cwd}")
        
        last_position = file_path.stat().st_size if file_path.exists() else 0
        
        async def line_generator() -> AsyncIterator[str]:
            nonlocal last_position
            
            while True:
                try:
                    if not file_path.exists():
                        if file_path not in self._warned_files:
                            self._logger.warning(f"File {file_path} does not exist at {abs_path}")
                            self._logger.warning(f"Current working directory: {cwd}")
                            # List files in current directory for debugging
                            try:
                                files_in_cwd = list(cwd.glob("*.log"))
                                self._logger.warning(f"Log files in current directory: {files_in_cwd}")
                            except Exception as e:
                                self._logger.warning(f"Could not list files in current directory: {e}")
                            self._warned_files.add(file_path)
                        await asyncio.sleep(5)
                        continue
                    
                    current_size = file_path.stat().st_size
                    
                    if current_size < last_position:
                        last_position = 0
                    
                    if current_size > last_position:
                        async with aiofiles.open(file_path, 'r', encoding='latin1') as f:
                            await f.seek(last_position)
                            new_content = await f.read()
                            last_position = current_size
                            
                            for line in new_content.splitlines():
                                if line.strip():
                                    yield line
                    
                    await asyncio.sleep(self._check_interval)
                    
                except Exception as e:
                    self._logger.error(f"Error reading file {file_path}: {e}")
                    await asyncio.sleep(5)
        
        try:
            yield line_generator()
        finally:
            self._logger.info(f"Stopped monitoring {file_path}")


class DiscordService:
    def __init__(self, bot: commands.Bot, config: BotConfig):
        self._bot = bot
        self._config = config
        self._logger = logging.getLogger(f"{__name__}.DiscordService")
    
    async def send_game_alert(self, player_name: str, server_name: str, alert_message: str) -> bool:
        try:
            alert_channel = self._bot.get_channel(self._config.alert_channel_id)
            if not alert_channel:
                self._logger.error(f"Alert channel {self._config.alert_channel_id} not found")
                return False
            
            role_mention = f"<@&{self._config.alert_role_id}>"
            
            embed = discord.Embed(
                title="**Game Alert!**",
                color=discord.Color.blue(),
                timestamp=datetime.now(),
                description=alert_message
            )
            
            embed.add_field(name="**Player**", value=f"*{player_name}*", inline=True)
            embed.add_field(name="**Server**", value=f"*{server_name}*", inline=True)
            
            if self._bot.user and self._bot.user.avatar:
                embed.set_footer(text="DefconExpanded", icon_url=self._bot.user.avatar.url)
            else:
                embed.set_footer(text="DefconExpanded")
            
            await alert_channel.send(content=role_mention, embed=embed)
            self._logger.info(f"Sent alert for {player_name} on {server_name}")
            return True
            
        except discord.Forbidden:
            self._logger.error(f"No permission to send messages in channel {self._config.alert_channel_id}")
            return False
        except Exception as e:
            self._logger.error(f"Error sending alert: {e}")
            return False
    
    async def update_bot_presence(self, activity_text: Optional[str]) -> bool:
        try:
            if not activity_text:
                activity = discord.CustomActivity(name="Watching for Defcon games")
            elif "waiting" in activity_text:
                activity = discord.CustomActivity(name=activity_text)
            elif " | " in activity_text:
                server_info, player_info = activity_text.split(" | ", 1)
                activity = discord.Activity(
                    type=discord.ActivityType.competing,
                    name=server_info,
                    state=player_info,
                    details="DefconExpanded"
                )
            else:
                activity = discord.Activity(
                    type=discord.ActivityType.competing,
                    name=activity_text
                )
            
            await self._bot.change_presence(activity=activity)
            return True
            
        except Exception as e:
            self._logger.error(f"Error updating presence: {e}")
            return False

class DefconServerTracker:
    SERVER_NAME_MAPPING = {
        "1V1": "1v1 Random",
        "1V1VARIABLE": "1v1 Lots of Units",
        "1V1BEST": "1v1 Best Setups",
        "1V1CURSED": "1v1 Cursed Setups",
        "1V1RAIZER": "1v1 Raizer's Server",
        "1V1BEST2": "1v1 Best Setups", 
        "1V1DEFAULT": "1v1 Default",
        "1V1UKBALANCED": "1v1 UK and Ireland",
        "2V2UKBALANCED": "2v2 UK and Ireland",
        "DIPLOUK": "Diplomacy UK Mod",
        "MURICONUK": "Muricon UK Mod",
        "NOOB": "New Player Server",
        "TOURNAMENT": "2v2 Tournament",
        "2V2": "2v2 Random",
        "6PLAYERFFA": "FFA Server",
        "3V3_FFA": "3v3 Random",
        "1V1MURICONRANDOM": "Muricon 1v1 Random",
        "509CG2V2": "509 CG 2v2",
        "1V1MURICON": "Muricon 1v1",
        "8PLAYER4V4": "8 Player 4v4",
        "8PLAYERDIPLO": "8 Player Diplo",
        "10PLAYER5V5": "10 Player 5v5",
        "10PLAYERDIPLO": "10 Player Diplo",
        "16PLAYER": "16 Player Server",
        "SONYHOOV": "Sony and Hoovs Hideout"
    }
    
    def __init__(self, config: BotConfig):
        self._config = config
        self._logger = LoggingService.setup_logging(config.log_level)
        self._timeout_service = PlayerTimeoutService(timedelta(minutes=config.timeout_minutes))
        self._parser = GameEventParser()
        self._file_monitor = FileMonitorService(config.file_check_interval)
        
        intents = discord.Intents.default()
        intents.message_content = True
        intents.members = True
        intents.presences = True
        intents.guilds = True
        
        self._bot = commands.Bot(command_prefix='!', intents=intents)
        self._discord_service = DiscordService(self._bot, config)
        self._game_states: Dict[str, GameState] = {}
        self._status_index = 0
        self._last_update_time = datetime.now()
        self._setup_bot_events()
    
    def _setup_bot_events(self) -> None:
        @self._bot.event
        async def on_ready():
            self._logger.info(f"Discord bot connected as {self._bot.user}")
            self._start_background_tasks()
        
        @self._bot.event
        async def on_error(event, *args, **kwargs):
            self._logger.error(f"Discord error in {event}: {args}", exc_info=True)
    
    def _start_background_tasks(self) -> None:
        
        @tasks.loop(seconds=self._config.status_rotation_seconds)
        async def rotate_status():
            await self._update_bot_presence()
        
        @tasks.loop(minutes=self._config.cleanup_interval_minutes)
        async def cleanup_timeouts():
            self._timeout_service.cleanup_expired_timeouts()
        
        rotate_status.start()
        cleanup_timeouts.start()
    
    def add_server(self, log_file_path: str, server_name: str) -> None:
        self._game_states[log_file_path] = GameState(server_name=server_name)
        self._logger.info(f"Added server: {server_name} ({log_file_path})")
    
    def get_friendly_server_name(self, internal_name: str) -> str:
        return self.SERVER_NAME_MAPPING.get(internal_name.upper(), internal_name)
    
    async def _update_bot_presence(self) -> None:
        if (datetime.now() - self._last_update_time).total_seconds() < self._config.update_cooldown:
            return
        
        status_text = self._get_next_status()
        success = await self._discord_service.update_bot_presence(status_text)
        
        if success:
            self._last_update_time = datetime.now()
    
    def _get_next_status(self) -> Optional[str]:
        active_servers = [gs for gs in self._game_states.values() if gs.phase == GamePhase.ACTIVE]
        lobby_servers = [gs for gs in self._game_states.values() if gs.get_lobby_players()]
        
        for server in lobby_servers:
            lobby_players = server.get_lobby_players()
            if lobby_players:
                names = [p.name for p in lobby_players if p.name]
                if len(names) == 1:
                    return f"{names[0]} waiting in {server.server_name}"
                elif len(names) > 1:
                    players_text = ", ".join(names[:-1]) + f" and {names[-1]}"
                    return f"{players_text} waiting in {server.server_name}"
        
        if not active_servers:
            return None
        
        if len(active_servers) > 1:
            grouped = {}
            for server in active_servers:
                server_type = server.server_name.split()[0]
                grouped.setdefault(server_type, []).append(server)
            
            counts = [f"{len(servers)}x {type}" for type, servers in grouped.items()]
            summary = f"{len(active_servers)} Active Games: {' | '.join(counts)}"
            
            self._status_index = (self._status_index + 1) % (len(active_servers) + 1)
            if self._status_index == 0:
                return summary
            return self._format_game_status(active_servers[self._status_index - 1])
        
        return self._format_game_status(active_servers[0])
    
    def _format_game_status(self, game_state: GameState) -> str:
        duration = game_state.get_duration_str()
        active_players = game_state.get_active_players()
        
        if not active_players:
            return f"{game_state.server_name} {duration}"
        
        names = [p.name for p in active_players if p.name]
        if len(names) == 2:
            players_text = " vs ".join(names)
        else:
            players_text = ", ".join(names)
        
        return f"{game_state.server_name} {duration} | {players_text}"
    
    async def monitor_game_events(self, log_file_path: str) -> None:
        file_path = Path(log_file_path)
        game_state = self._game_states[log_file_path]
        
        self._logger.info(f"Starting to monitor {file_path} for {game_state.server_name}")
        
        try:
            async with self._file_monitor.monitor_file_changes(file_path) as line_stream:
                async for line in line_stream:
                    await self._process_game_line(line, game_state)
        except Exception as e:
            self._logger.error(f"Error monitoring {file_path}: {e}")
    
    async def _process_game_line(self, line: str, game_state: GameState) -> None:
        try:
            if 'SERVER_START' in line:
                self._reset_game_state(game_state)
                await self._update_bot_presence()
            
            elif 'CLIENT_NEW' in line:
                player_id = self._parser.parse_client_new(line)
                if player_id:
                    self._handle_client_new(game_state, player_id)
            
            elif 'CLIENT_NAME' in line:
                name_data = self._parser.parse_client_name(line)
                if name_data:
                    client_id, name = name_data
                    self._handle_client_name(game_state, client_id, name)
            
            elif 'TEAM_ENTER' in line:
                team_data = self._parser.parse_team_enter(line)
                if team_data:
                    team, client_id = team_data
                    self._handle_team_enter(game_state, team, client_id)
            
            elif 'GAME_START' in line:
                self._handle_game_start(game_state)
                await self._update_bot_presence()
            
            elif 'GAME_END' in line:
                self._handle_game_end(game_state)
                await self._update_bot_presence()

        except Exception as e:
            self._logger.error(f"Error processing line '{line}': {e}")
    
    def _reset_game_state(self, game_state: GameState) -> None:
        new_state = GameState(
            server_name=game_state.server_name,
            games_played=game_state.games_played
        )

        for key, state in self._game_states.items():
            if state == game_state:
                self._game_states[key] = new_state
                break
    
    def _handle_client_new(self, game_state: GameState, player_id: PlayerIdentifier) -> None:
        player_info = PlayerInfo(
            client_id=player_id.client_id,
            key_id=player_id.key_id,
            ip=player_id.ip
        )
        game_state.players[player_id.client_id] = player_info
    
    def _handle_client_name(self, game_state: GameState, client_id: int, name: str) -> None:
        if client_id in game_state.players:
            game_state.players[client_id].name = name
    
    def _handle_team_enter(self, game_state: GameState, team: int, client_id: int) -> None:
        if client_id in game_state.players:
            game_state.players[client_id].team = team
    
    def _handle_game_start(self, game_state: GameState) -> None:
        game_state.phase = GamePhase.ACTIVE
        game_state.start_time = datetime.now()
        
        for player in game_state.players.values():
            player.is_active = True
        
        self._logger.info(f"Game started on {game_state.server_name}")
    
    def _handle_game_end(self, game_state: GameState) -> None:
        game_state.phase = GamePhase.ENDED
        game_state.games_played += 1
        self._logger.info(f"Game ended on {game_state.server_name} (total: {game_state.games_played})")
    
    async def monitor_server_output(self, output_file_path: str) -> None:
        file_path = Path(output_file_path)
        internal_name = file_path.stem.replace('server_output_', '').upper()
        friendly_name = self.get_friendly_server_name(internal_name)
        
        self._logger.info(f"Starting to monitor {file_path} for alerts from {friendly_name}")
        
        try:
            async with self._file_monitor.monitor_file_changes(file_path) as line_stream:
                async for line in line_stream:
                    await self._process_server_output_line(line, friendly_name)
        except Exception as e:
            self._logger.error(f"Error monitoring {file_path}: {e}")
    
    async def _process_server_output_line(self, line: str, server_name: str) -> None:
        if '/gamealert' not in line:
            return
        
        alert_data = self._parser.parse_gamealert(line)
        if not alert_data:
            return
        
        player_name, alert_message = alert_data

        if self._timeout_service.is_player_timed_out(player_name):
            remaining = self._timeout_service.get_remaining_timeout(player_name)
            if remaining:
                self._logger.info(f"Player {player_name} is timed out ({remaining.seconds}s remaining)")
                return

        self._timeout_service.apply_timeout(player_name)
        
        success = await self._discord_service.send_game_alert(player_name, server_name, alert_message)
        if success:
            self._logger.info(f"Sent alert for {player_name} on {server_name}: {alert_message}")
    
    async def run(self, server_configs: List[str], output_files: List[str]) -> None:
        tasks = []
        
        for config in server_configs:
            if ':' in config:
                log_file, server_name = config.split(':', 1)
                self.add_server(log_file, server_name)
                tasks.append(self.monitor_game_events(log_file))
        
        for output_file in output_files:
            if output_file.startswith('server_output_'):
                tasks.append(self.monitor_server_output(output_file))
        
        self._logger.info(f"Starting with {len(tasks)} monitoring tasks")
        
        try:
            await asyncio.gather(
                self._bot.start(self._config.discord_token),
                *tasks
            )
        except KeyboardInterrupt:
            self._logger.info("Received shutdown signal")
        finally:
            await self._shutdown()
    
    async def _shutdown(self) -> None:
        self._logger.info("Shutting down gracefully...")
        
        try:
            await self._discord_service.update_bot_presence("Server restarting...")
            await asyncio.sleep(1)
        except:
            pass
        
        await self._bot.close()
        self._logger.info("Shutdown complete")

async def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python defcon_tracker.py <server_config> [server_output_file] ...")
        print("Server config format: game_events_file:server_name")
        sys.exit(1)
    
    try:
        config = BotConfig.from_environment()
        tracker = DefconServerTracker(config)
        server_configs = []
        output_files = []
        
        for arg in sys.argv[1:]:
            if ':' in arg:
                server_configs.append(arg)
            elif arg.startswith('server_output_'):
                output_files.append(arg)
            else:
                print(f"Warning: Unrecognized argument format: {arg}")
        
        await tracker.run(server_configs, output_files)
        
    except ConfigurationError as e:
        print(f"Configuration error: {e}")
        sys.exit(1)
    except Exception as e:
        logging.error(f"Unexpected error: {e}", exc_info=True)
        sys.exit(1)

if __name__ == "__main__":
    asyncio.run(main())