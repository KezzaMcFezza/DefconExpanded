@echo off
setlocal enabledelayedexpansion

REM Create the game_logs directory if it doesn't exist
if not exist "game_logs" mkdir "game_logs"

REM Start the game log processor in a new window
start "Game Log Processor DefconExpanded 8 Player Diplomacy" cmd /k python python_parsers/LogProcessorEightPlayer.py "dedcon_game_events/game_events_DefconExpanded_8_Player_Diplomacy.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for DefconExpanded 8 Player Diplomacy...
Dedcon8P.exe "dedcon_configs/DefconExpanded 8 Player Diplomacy.txt" 

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 60 seconds...
timeout /t 60

goto server_loop