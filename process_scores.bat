@echo off
setlocal enabledelayedexpansion

:: Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Python is not installed or not in PATH. Please install Python and try again.
    pause
    exit /b 1
)

echo Processing game events and logging scores...
python log_player_scores.py game_events.log player_scores.log
if %errorlevel% neq 0 (
    echo Error occurred while processing game events and logging scores.
    pause
    exit /b 1
)

echo Done! Check player_scores.log for results.
pause