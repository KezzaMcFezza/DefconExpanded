@echo off
setlocal enabledelayedexpansion

REM Start the game log processor in a new window
start "Game Log Processor Sony and Hoov's Hideout" cmd /k python python_parsers/LogProcessorVanilla.py "dedcon_game_events/game_events_Sony_and_Hoovs_Hideout.log" "game_logs"

:server_loop
set "game_start_time=%date:~10,4%-%date:~4,2%-%date:~7,2%T%time:~0,2%-%time:~3,2%-%time:~6,2%"
set "game_start_time=!game_start_time: =0!"

echo Starting Dedcon server for Sony and Hoov's Hideout...
Dedcon.exe "dedcon_configs/Sony and Hoov's Hideout.txt" 

echo Server closed. Processing game logs...

echo Done processing. Restarting server in 60 seconds...
timeout /t 10

goto server_loop