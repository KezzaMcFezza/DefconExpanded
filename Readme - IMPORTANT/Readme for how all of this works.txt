#### Kezza's Dedcon Servers Readme ####

I made a readme for you if you have no fucking clue how this all works and tbh even I struggle to remember so this readme is good for me also

#### How It All Works ####

1. The batch file starts the Dedcon server and a separate log processor which will have different print statements so when you check the console you know what server is running.
2. Dedcon runs the game server based on the configuration file that is chosen inside the batch file.
3. The log processor script continuously monitors these logs.
4. When a game ends the log processor will create a log file by reading the game_events and the server_output file and will parse it into json data.
5. When the demo file and log file are created the website will add the demo file to a waiting list, the website has a system that will read the 
   log file and demo file name which are timestamps. now this is important as the website will only link these two files together if the timestamps
   are within 10 minutes of each other which it has never been outside 10 minutes before but we will see if it ever needs changed.
5. The server automatically restarts for the next game and same with the log processor window they all work seamlessly.
6. The authkeys are now stored inside the config files to make it easier.
7. All the servers that run will output their json log files and their demo files into the same directory so the website only has to look inside 2 folders.

#### Running the Servers ####

To run one of the Dedcon servers:

1. Goto the website folder containing the batch files.
2. Click the appropriate batch file:
   # 1v1 server.bat for 1v1 games
   # 2v2 server.bat for 2v2 games
   # 3v3 server.bat for 3v3 FFA games

Each batch file will:
# Start the Dedcon server with the config file that is chosen for example configfile2.txt
# Launch a separate window for the game log processor python script
# The servers will automatically restart after each game and the same is also true with the game log processor
# To stop one of the dedcon servers close the command prompt windows for both the server and the log processor.

##### Adding New Game Servers ####

To add a new game server for example a 4v4 or a diplo

1. Duplicate an existing batch file such as 1v1 and rename it to whatever you want the name of the batch file doesn't matter
2. Duplicate a config file from one of the other dedcon servers and rename it to configfile4 or something and inside the config file
   change whatever you want to make the server different from the other ones. but remember and change the auth key or the metaserver
   will come to your house and prepare to breach your asshole.
3. Duplicate one of the python scripts and rename it to something similar to the batch file/server theme you are going for but it doesn't
   really matter its just for ease of use
4. Edit the new batch file:

   # Update the Python script name to whatever you named the python script

   # Update log file names for example... server_output_4v4.log, game_events_4v4.log
     this is really important so don't fuck that up or else the dedcon server will read from the same log file as another server and it will
     create games from the other server but the demo file wont match it will be a mess pls don't do it

5. Finally we get to the good stuff. I have created some examples for you to understand how to do this yourself.
   this is an example for a 4v4 for the batch file and the python script

Example of modified log processor file content:


##########################################################################################################################################

def process_game_logs(server_output_file, game_events_file, output_directory):
    print(f"Starting game log processor for 4v4")
    print(f"Server output file: {server_output_file}")
    print(f"Game events file: {game_events_file}")
    print(f"Output directory: {output_directory}")

	---------------------------------------- rest of the script remains the same ----------------------------------------

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python game_log_processor4v4.py <server_output_file> <game_events_file> <output_directory>")
        sys.exit(1)
    server_output_file = sys.argv[1]
    game_events_file = sys.argv[2]
    output_directory = sys.argv[3]
    process_game_logs(server_output_file, game_events_file, output_directory)

##########################################################################################################################################


Example of modified batch file content:


##########################################################################################################################################

@echo off
setlocal enabledelayedexpansion

REM Start the game log processor in a new window
start "Game Log Processor 4v4" cmd /k python game_log_processor4v4.py "server_output_4v4.log" "game_events_4v4.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for 4v4...
dedcon.exe ConfigFile4.txt endgame.txt

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 5 seconds...
timeout /t 5

goto server_loop

##########################################################################################################################################







