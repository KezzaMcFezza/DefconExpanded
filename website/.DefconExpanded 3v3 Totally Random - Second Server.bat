@echo off
setlocal enabledelayedexpansion

REM Create the game_logs directory if it doesn't exist
if not exist "game_logs" mkdir "game_logs"

REM Start the game log processor in a new window
start "Game Log Processor DefconExpanded 3v3 Totally Random - Second Server" cmd /k python python_parsers/LogProcessorVanilla.py "dedcon_game_events/game_events_DefconExpanded_3v3_Totally_Random_Second_Server.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for DefconExpanded 3v3 Totally Random - Second Server...
Dedcon.exe "dedcon_configs/DefconExpanded 3v3 Totally Random Second Server.txt" 

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 60 seconds...
timeout /t 10

goto server_loop