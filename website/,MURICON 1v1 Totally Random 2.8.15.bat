@echo off
setlocal enabledelayedexpansion

REM Create the game_logs directory if it doesn't exist
if not exist "game_logs" mkdir "game_logs"

REM Start the game log processor in a new window
start "Game Log Processor MURICON 1v1 Totally Random 2.8.15" cmd /k python python_parsers/LogProcessorEightPlayer.py "dedcon_game_events/game_events_MURICON_1v1_Totally_Random_2_8_15.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for MURICON 1v1 Totally Random 2.8.15...
Dedcon8P.exe "dedcon_configs/MURICON 1v1 Totally Random 2.8.15.txt" 

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 10 seconds...
timeout /t 10

goto server_loop