import sys
import re
import os
from datetime import datetime
import json
import time

def process_game_logs(server_output_file, game_events_file, output_directory):
    print(f"Starting game log processor")
    print(f"Server output file: {server_output_file}")
    print(f"Game events file: {game_events_file}")
    print(f"Output directory: {output_directory}")

    os.makedirs(output_directory, exist_ok=True)

    game_start_time = None
    game_end_time = None
    current_game_data = {}
    game_id = None

    with open(server_output_file, 'r') as f:
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if not line:
                time.sleep(1)  # Wait for a short interval before checking for new lines
                continue

            if 'Game ID:' in line:
                game_id = line.split('Game ID:')[1].strip()
                print(f"Detected Game ID: {game_id}")
                current_game_data['game_id'] = game_id

            if 'Game started. Players:' in line and game_start_time is None:
                game_start_time = datetime.now()
                current_game_data['start_time'] = game_start_time.isoformat()
                current_game_data['game_id'] = game_id
                print(f"Game started at: {game_start_time}")

            elif 'Game Over !' in line and game_start_time is not None:
                game_end_time = datetime.now()
                current_game_data['end_time'] = game_end_time.isoformat()
                print(f"Game ended at: {game_end_time}")

                duration = game_end_time - game_start_time
                current_game_data['duration'] = str(duration)
                print(f"Game duration: {duration}")

                # Process player scores
                process_player_scores(game_events_file, output_directory, current_game_data)

                # Reset game start and end times for the next game
                game_start_time = None
                game_end_time = None
                current_game_data = {}
                game_id = None

def process_player_scores(input_file, output_directory, game_data):
    with open(input_file, 'r') as f:
        content = f.read()

    game_sections = content.split('SERVER_START')[1:]  # Split into game sections

    for game in game_sections:
        players = {}
        game_notes = []
        final_scores = {}
        last_alliance = None

        lines = game.split('\n')
        in_score_block = False
        for line in lines:
            if 'TEAM_ENTER' in line:
                match = re.search(r'TEAM_ENTER (\d+) (\d+)', line)
                if match:
                    team, client_id = match.groups()
                    players[client_id] = {'team': int(team), 'territory': 'Unknown', 'client_id': int(client_id)}
            elif 'TEAM_ALLIANCE' in line:
                match = re.search(r'TEAM_ALLIANCE (\d+) (\d+)', line)
                if match:
                    last_alliance = match.groups()
            elif 'TEAM_TERRITORY' in line:
                match = re.search(r'TEAM_TERRITORY (\d+) (\d+)', line)
                if match:
                    team, territory = match.groups()
                    for player in players.values():
                        if player['team'] == int(team):
                            player['territory'] = get_territory_name(int(territory))
            elif 'CLIENT_NAME' in line:
                match = re.search(r'CLIENT_NAME (\d+) (.*)', line)
                if match:
                    client_id, name = match.groups()
                    if client_id in players:
                        players[client_id]['name'] = name
            elif 'SCORE_BEGIN' in line:
                in_score_block = True
            elif 'SCORE_END' in line:
                in_score_block = False
            elif in_score_block and 'SCORE_TEAM' in line:
                match = re.search(r'SCORE_TEAM (\d+) (-?\d+) (\d+) (.*)', line)
                if match:
                    team, score, client_id, player_name = match.groups()
                    final_scores[client_id] = {'team': int(team), 'score': int(score), 'name': player_name}
            elif any(event in line for event in ['TEAM_READY', 'TEAM_ALLIANCE', 'TEAM_SPEED']):
                game_notes.append(line.strip())

        # Update player information with final scores
        final_players = []
        for client_id, score_data in final_scores.items():
            if client_id in players:
                player = players[client_id]
                player.update(score_data)
                final_players.append(player)

    if final_players:
        game_data['gameType'] = 'Multiplayer' if len(final_players) > 2 else '1v1'
        game_data['players'] = final_players
        game_data['notes'] = game_notes
        game_data['last_alliance'] = last_alliance  # Add the last alliance to the game data

        # Use the same naming convention as the demo file
        start_time = datetime.fromisoformat(game_data['start_time'])
        output_filename = f"game_{start_time.strftime('%Y-%m-%d-%H-%M')}_full.json"
        output_file = os.path.join(output_directory, output_filename)
        
        with open(output_file, 'w') as f:
            json.dump(game_data, f, indent=2)
        print(f"Game report written to {output_file}")

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
    if len(sys.argv) < 4:
        print("Usage: python game_log_processor.py <server_output_file> <game_events_file> <output_directory>")
        sys.exit(1)
    server_output_file = sys.argv[1]
    game_events_file = sys.argv[2]
    output_directory = sys.argv[3]
    process_game_logs(server_output_file, game_events_file, output_directory)