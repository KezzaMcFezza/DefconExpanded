@echo off

REM Start the Discord status bot with multiple servers
start "Defcon Status Bot" cmd /k python liveservertracker.py ^
game_events_1v1.log:"1v1 Random" ^
game_events_1v1best.log:"1v1 Best Setups" ^
game_events_1v1best2.log:"1v1 Best Setups" ^
game_events_1v1default.log:"1v1 Default" ^
game_events_noob.log:"1v1 New Player Server" ^
game_events_2v2.log:"2v2 Random" ^
game_events_2v2noRUorAS.log:"2v2 Atlantic Warfare" ^
game_events_3v3_FFA.log:"3v3 Random" ^
game_events_8player4v4.log:"8 Player 4v4" ^
game_events_8playerdiplo.log:"8 Player Diplo" ^
game_events_10player5v5.log:"10 Player 5v5" ^
game_events_10playerdiplo.log:"10 Player Diplo" ^
game_events_16player.log:"16 Player Server"
