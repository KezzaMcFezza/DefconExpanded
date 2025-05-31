import sys
import re
import os
from datetime import datetime
import json
import time

def create_new_game_data():
    return {
        'players': [],
        'notes': [],
        'settings': {},
        'game_id': None,
        'start_time': None,
        'end_time': None,
        'duration': None,
        'gameType': 'DefconExpanded | 1V1 | AF vs AS | Totally Random',
        'last_alliance': None
    }

def process_game_logs(game_events_file, output_directory):
    print(f"Starting game log processor for 1V1 AF vs AS server")
    print(f"Game events file: {game_events_file}")
    print(f"Output directory: {output_directory}")

    game_start_time = None
    game_end_time = None
    current_game_data = create_new_game_data()
    player_info = {}  # Tracks all players
    final_players = {}  # Only players with SCORE_TEAM entries
    processing_game = False

    with open(game_events_file, 'r') as f:
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if not line:
                time.sleep(1)
                f.seek(0, os.SEEK_CUR)
                continue

            if 'SERVER_START' in line:
                if processing_game:
                    write_game_report(current_game_data, output_directory)
                current_game_data = create_new_game_data()
                player_info = {}
                final_players = {}
                game_start_time = None
                game_end_time = None
                processing_game = False
                print("Server started. Resetting game data.")
                continue

            if 'SERVER_QUIT' in line:
                if processing_game:
                    write_game_report(current_game_data, output_directory)
                processing_game = False
                print("Server quit. Finalizing current game data.")
                continue

            if 'GAME_START' in line:
                processing_game = True
                game_start_time = datetime.now()
                current_game_data['start_time'] = game_start_time.isoformat()
                print(f"Game started at: {game_start_time}")

            elif 'GAME_END' in line:
                if game_start_time is not None:
                    game_end_time = datetime.now()
                    current_game_data['end_time'] = game_end_time.isoformat()
                    print(f"Game ended at: {game_end_time}")

                    duration = game_end_time - game_start_time
                    current_game_data['duration'] = str(duration)
                    print(f"Game duration: {duration}")

            elif 'SCORE_END' in line:
                if game_start_time is not None and game_end_time is not None:
                    update_final_player_data(current_game_data, final_players)
                    write_game_report(current_game_data, output_directory)
                    
                    current_game_data = create_new_game_data()
                    player_info = {}
                    final_players = {}
                    game_start_time = None
                    game_end_time = None
                    processing_game = False
                else:
                    print("Encountered SCORE_END without a complete game. Ignoring.")

            process_line(line, current_game_data, player_info, final_players)

def process_line(line, game_data, player_info, final_players):
    if 'CLIENT_NEW' in line:
        # match = re.search(r'CLIENT_NEW (\d+) (\w+) ([\d.]+) ([\d.]+ (\w+))', line)
        match = re.search(r'CLIENT_NEW (\d+) (\w+) ([\d.]+) ([\d.]+) ?(\w+)?', line)
        if match:
            client_id, key_id, ip, version, platform = match.groups()
            client_id = int(client_id)
            if platform is None:
                platform = 'undefined'
            player_info[client_id] = {
                'client_id': client_id,
                'key_id': key_id,
                'ip': ip,
                'version': version,
                'platform': platform,
                'name': ''
            }

    elif 'CLIENT_NAME' in line:
        match = re.search(r'CLIENT_NAME (\d+) (.*)', line)
        if match:
            client_id, name = match.groups()
            client_id = int(client_id)
            if client_id in player_info:
                player_info[client_id]['name'] = name

    elif 'TEAM_ENTER' in line:
        match = re.search(r'TEAM_ENTER (\d+) (\d+)', line)
        if match:
            team, client_id = match.groups()
            client_id = int(client_id)
            if client_id in player_info:
                player_info[client_id]['team'] = int(team)

    elif 'TEAM_TERRITORY' in line:
        match = re.search(r'TEAM_TERRITORY (\d+) (\d+)', line)
        if match:
            team, territory = match.groups()
            team = int(team)
            for player in player_info.values():
                if player.get('team') == team:
                    player['territory'] = get_territory_name(int(territory))

    elif 'SCORE_TEAM' in line:
        match = re.search(r'SCORE_TEAM (\d+) (-?\d+) (\d+) (.*)', line)
        if match:
            team, score, client_id, player_name = match.groups()
            client_id = int(client_id)
            if client_id in player_info:
                final_players[client_id] = player_info[client_id].copy()
                final_players[client_id].update({
                    'team': int(team),
                    'score': int(score),
                    'name': player_name or final_players[client_id]['name']
                })

    elif 'SETTING' in line:
        match = re.search(r'SETTING (\w+) (.*)', line)
        if match:
            setting, value = match.groups()
            game_data['settings'][setting] = value

    elif 'TEAM_ALLIANCE' in line:
        match = re.search(r'TEAM_ALLIANCE (\d+) (\d+)', line)
        if match:
            game_data['last_alliance'] = match.groups()

    elif any(event in line for event in ['GAME_START', 'GAME_END', 'SERVER_START', 'SERVER_QUIT']):
        game_data['notes'].append(line.strip())

def update_final_player_data(game_data, final_players):
    game_data['players'] = list(final_players.values())

def write_game_report(game_data, output_directory):
    if game_data['start_time'] and game_data['end_time']:
        start_time = datetime.fromisoformat(game_data['start_time'])
        filename = f"game_{start_time.strftime('%Y-%m-%d-%H-%M')}_full.json"
        output_file = os.path.join(output_directory, filename)
        
        with open(output_file, 'w') as f:
            json.dump(game_data, f, indent=2)
        print(f"Game report written to {output_file}")
    else:
        print("Incomplete game data. Skipping game report creation.")

def get_territory_name(territory_id):
    territories = {
        0: "North America",
        1: "South America",
        2: "Europe",
        3: "Russia",
        4: "Asia",
        5: "Africa"
    }
    return territories.get(territory_id, "Unknown")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python game_log_processor1v1afas.py <game_events_file> <output_directory>")
        sys.exit(1)
    game_events_file = sys.argv[1]
    output_directory = sys.argv[2]
    process_game_logs(game_events_file, output_directory)
