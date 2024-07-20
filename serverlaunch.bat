@echo off
echo Starting Octocon...
cd /d "%~dp0"

start "Octocon Server" cmd /k node server.js

start "Octocon Monitor" cmd /k npm run monitor

echo Octocon server and monitor have been launched.
echo This window will close in 5 seconds.
timeout /t 5
exit