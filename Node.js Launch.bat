@echo off
echo Restarting DefconExpanded Website Server...
cd /d "%~dp0"

:restart
echo Starting server...
node --expose-gc node.js/server.js
echo Server stopped. Restarting in 3 seconds...
timeout /t 3
goto restart 