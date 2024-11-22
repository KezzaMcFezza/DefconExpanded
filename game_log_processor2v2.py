import sys
import re
import os
from datetime import datetime
import json
import time

def create_new_game_data():
    return {
        'players': [],
        'spectators': [],
        'notes': [],
        'settings': {},
        'game_id': None,
        'start_time': None,
        'end_time': None,
        'duration': None,
        'gameType': 'DefconExpanded | 2V2 | Totally Random'
    }

def process_game_logs(game_events_file, output_directory):
    print(f"Starting game log processor for 2v2 server")
    print(f"Game events file: {game_events_file}")
    print(f"Output directory: {output_directory}")

    game_start_time = None
    game_end_time = None
    current_game_data = create_new_game_data()
    player_info = {}  # Tracks all players
    player_names = {}  # Persistent storage for player names
    team_territories = {}  # Persistent storage for territories
    final_players = {}  # Only players with SCORE_TEAM entries
    team_alliances = {}  # Track current alliance for each team
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
                player_names = {}  # Reset player names on server start
                team_territories = {}  # Reset territories on server start
                final_players = {}
                team_alliances = {}
                game_start_time = None
                game_end_time = None
                processing_game = False
                print("Server started. Resetting game data.")
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

            elif 'SCORE_END' in line:
                if game_start_time is not None and game_end_time is not None:
                    update_final_player_data(current_game_data, final_players, team_alliances, team_territories)
                    write_game_report(current_game_data, output_directory)
                    
                    current_game_data = create_new_game_data()
                    player_info = {}
                    final_players = {}
                    team_alliances = {}
                    game_start_time = None
                    game_end_time = None
                    processing_game = False
                else:
                    print("Encountered SCORE_END without a complete game. Ignoring.")

            process_line(line, current_game_data, player_info, final_players, team_alliances, player_names, team_territories)

def process_line(line, game_data, player_info, final_players, team_alliances, player_names, team_territories):
    if 'TEAM_ALLIANCE' in line:
        match = re.search(r'TEAM_ALLIANCE (\d+) (\d+)(?: (\w+))?', line)
        if match:
            team_id, alliance_id, action = match.groups()
            team_id = int(team_id)
            alliance_id = int(alliance_id)
            
            # Update the team's current alliance
            team_alliances[team_id] = alliance_id

    elif 'CLIENT_NEW' in line:
        match = re.search(r'CLIENT_NEW (\d+) (\w+) ([\d.]+) ([\d.]+)(?: (\w+))?', line)
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
                'name': player_names.get(client_id, '')  # Use stored name if available
            }

    elif 'CLIENT_NAME' in line:
        match = re.search(r'CLIENT_NAME (\d+) (.*)', line)
        if match:
            client_id, name = match.groups()
            client_id = int(client_id)
            name = name.strip()
            player_names[client_id] = name  # Store name in persistent dictionary
            if client_id in player_info:
                player_info[client_id]['name'] = name

    elif 'TEAM_ENTER' in line:
        match = re.search(r'TEAM_ENTER (\d+) (\d+)', line)
        if match:
            team, client_id = match.groups()
            team = int(team)
            client_id = int(client_id)
            if client_id in player_info:
                player_info[client_id]['team'] = team
                # Restore territory if available
                if team in team_territories:
                    player_info[client_id]['territory'] = team_territories[team]

    elif 'TEAM_TERRITORY' in line:
        match = re.search(r'TEAM_TERRITORY (\d+) (\d+)', line)
        if match:
            team, territory = match.groups()
            team = int(team)
            territory_name = get_territory_name(int(territory))
            team_territories[team] = territory_name  # Store territory by team
            for player in player_info.values():
                if player.get('team') == team:
                    player['territory'] = territory_name

    elif 'SCORE_TEAM' in line:
        match = re.search(r'SCORE_TEAM (\d+) (-?\d+) (\d+) (.*)', line)
        if match:
            team, score, client_id, player_name = match.groups()
            team = int(team)
            client_id = int(client_id)
            
            # Use stored name if available and current name is empty
            stored_name = player_names.get(client_id, '')
            current_name = player_name.strip()
            final_name = current_name or stored_name
            
            if client_id in player_info:
                # Copy existing player info
                final_players[client_id] = player_info[client_id].copy()
                final_players[client_id].update({
                    'team': team,
                    'score': int(score),
                    'name': final_name
                })
                # Restore territory if available
                if team in team_territories:
                    final_players[client_id]['territory'] = team_territories[team]
            else:
                # Look for this client's most recent info
                final_players[client_id] = {
                    'client_id': client_id,
                    'key_id': player_info.get(client_id, {}).get('key_id', ''),
                    'ip': player_info.get(client_id, {}).get('ip', ''),
                    'version': player_info.get(client_id, {}).get('version', ''),
                    'platform': player_info.get(client_id, {}).get('platform', 'STEAM'),
                    'name': final_name,
                    'team': team,
                    'score': int(score)
                }
                # Add territory if available for the team
                if team in team_territories:
                    final_players[client_id]['territory'] = team_territories[team]

    elif 'SCORE_SIGNATURE_SPECTATOR' in line:
        match = re.search(r'SCORE_SIGNATURE_SPECTATOR (\d+) (\w+) (.*)', line)
        if match:
            client_id, key_id, name = match.groups()
            client_id = int(client_id)
            
            # Create spectator entry from player info
            if client_id in player_info:
                spectator_data = {
                    'client_id': client_id,
                    'key_id': key_id,
                    'ip': player_info[client_id].get('ip', ''),
                    'version': player_info[client_id].get('version', ''),
                    'platform': player_info[client_id].get('platform', 'STEAM'),
                    'name': name.strip()
                }
                # Check if this spectator is already in the list to avoid duplicates
                if not any(spec['client_id'] == client_id for spec in game_data['spectators']):
                    game_data['spectators'].append(spectator_data)

    elif 'SETTING' in line:
        match = re.search(r'SETTING (\w+) (.*)', line)
        if match:
            setting, value = match.groups()
            game_data['settings'][setting] = value
            # Update gameType if ServerName is set
            if setting == 'ServerName':
                game_data['gameType'] = value

def update_final_player_data(game_data, final_players, team_alliances, team_territories):
    # Update each player's data with their final alliance and territory
    for player in final_players.values():
        team = player.get('team')
        if team in team_alliances:
            player['alliance'] = team_alliances[team]
        
        # Ensure territory is set from team_territories
        if team in team_territories:
            player['territory'] = team_territories[team]
        elif 'territory' not in player:
            # Fallback to checking other players on same team
            for other_player in final_players.values():
                if other_player.get('team') == team and 'territory' in other_player:
                    player['territory'] = other_player['territory']
                    break
    
    # Sort players by team ID
    game_data['players'] = sorted(list(final_players.values()), key=lambda x: x.get('team', 0))
    
    # Sort spectators by name
    game_data['spectators'] = sorted(game_data['spectators'], key=lambda x: x.get('name', ''))

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
        print("Usage: python game_log_processor2v2.py <game_events_file> <output_directory>")
        sys.exit(1)
    game_events_file = sys.argv[1]
    output_directory = sys.argv[2]
    process_game_logs(game_events_file, output_directory)
