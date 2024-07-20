@echo off
echo Starting Octocon...
cd /d "%~dp0"

:: Start the server in a new window
start "Octocon Server" cmd /k node server.js

:: Start the GUI in a new window
start "Octocon Monitor" cmd /k npm run monitor

echo Octocon server and monitor have been launched.
echo This window will close in 5 seconds.
timeout /t 5
exit