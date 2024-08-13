@echo off
echo Starting Defcon Expanded File Listener...
cd /d "%~dp0"

start "Defcon Expanded" cmd /k node server.js

echo Defcon Expanded Demo server has been launched.
echo This window will close in 5 seconds.
timeout /t 5
exit