@echo off
setlocal enabledelayedexpansion

REM Start the game log processor in a new window
start "Game Log Processor 3v3 FFA" cmd /k python game_log_processor3v3_FFA.py "server_output_3v3_FFA.log" "game_events_3v3_FFA.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for 3v3 FFA...
dedcon.exe ConfigFile1.txt endgame.txt

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 5 seconds...
timeout /t 5

goto server_loop