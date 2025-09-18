@echo off
setlocal enabledelayedexpansion

REM Create the game_logs directory if it doesn't exist
if not exist "game_logs" mkdir "game_logs"

REM Start the game log processor in a new window
start "Game Log Processor DefconExpanded Free For All Random Cities" cmd /k python python_parsers/LogProcessorVanilla.py "dedcon_game_events/game_events_DefconExpanded_Free_For_All_Random_Cities.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for DefconExpanded Free For All Random Cities...
Dedcon.exe "dedcon_configs/DefconExpanded Free For All Random Cities.txt" 

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 60 seconds...
timeout /t 10

goto server_loop