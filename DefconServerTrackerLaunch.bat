@echo off

REM Start the Discord status bot with multiple servers
start "Defcon Status Bot" cmd /k python DefconServerTracker.py ^
game_events_1v1.log:"1v1 Random" server_output_1v1.log ^
game_events_1v1best.log:"1v1 Best Setups" server_output_1v1best.log ^
game_events_1v1best2.log:"1v1 Best Setups" server_output_1v1best2.log ^
game_events_1v1default.log:"1v1 Default" server_output_1v1default.log ^
game_events_noob.log:"New Player Server" server_output_noob.log ^
game_events_2v2.log:"2v2 Random" server_output_2v2.log ^
game_events_6playerffa.log:"FFA Server" server_output_6playerffa.log ^
game_events_3v3_FFA.log:"3v3 Random" server_output_3v3_FFA.log ^
game_events_509cg1v1.log:"509 CG 1v1" server_output_509cg1v1.log ^
game_events_509cg2v2.log:"509 CG 2v2" server_output_509cg2v2.log ^
game_events_1v1muricon.log:"Muricon 1v1" server_output_1v1muricon.log ^
game_events_8player4v4.log:"8 Player 4v4" server_output_8player4v4.log ^
game_events_8playerdiplo.log:"8 Player Diplo" server_output_8playerdiplo.log ^
game_events_10player5v5.log:"10 Player 5v5" server_output_10player5v5.log ^
game_events_10playerdiplo.log:"10 Player Diplo" server_output_10playerdiplo.log ^
game_events_16player.log:"16 Player Server" server_output_16player.log ^
game_events_tournament.log:"2v2 Tournament" server_output_tournament.log ^
game_events_sonyhoov.log:"Sony/Hoovs Hideout" server_output_sonyhoov.log