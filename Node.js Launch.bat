@echo off
echo Starting DefconExpanded Website Server...
cd /d "%~dp0"

start "Defcon Expanded" cmd /k node node.js/server.js

echo DefconExpanded backend sucessfully launched.
echo Window will close in 5 seconds.
timeout /t 5
exit