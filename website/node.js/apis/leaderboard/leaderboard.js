//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 23-08-2025

const express = require('express');
const router = express.Router();

const {
    pool
} = require('../../constants');


const debug = require('../../debug-helpers');

router.get('/api/most-active-players', async (req, res) => {
    const startTime = debug.enter('getMostActivePlayers', [req.query], 1);
    try {
        const { serverName, serverList, startDate, endDate, excludeNames = '', limit = 10 } = req.query;
        debug.level2('Active players query parameters:', { serverName, serverList, startDate, endDate, excludeNames, limit });

        const namesToExclude = excludeNames ? excludeNames.split(',') : [];

        let conditions = [];
        let params = [];

        if (serverList) {
            const servers = serverList.split(',');
            const serverConditions = servers.map(() => 'game_type LIKE ?');
            conditions.push(`(${serverConditions.join(' OR ')})`);
            servers.forEach(server => {
                params.push(`%${server}%`);
            });
            debug.level3('Server list filter applied:', servers);
        } else if (serverName) {
            conditions.push('game_type LIKE ?');
            params.push(`%${serverName}%`);
            debug.level3('Single server filter applied:', serverName);
        }

        if (startDate && endDate) {
            conditions.push('date BETWEEN ? AND ?');
            params.push(startDate, endDate);
            debug.level3('Date range filter applied:', { startDate, endDate });
        } else if (startDate) {
            conditions.push('date >= ?');
            params.push(startDate);
            debug.level3('Start date filter applied:', startDate);
        } else if (endDate) {
            conditions.push('date <= ?');
            params.push(endDate);
            debug.level3('End date filter applied:', endDate);
        }

        let query = 'SELECT * FROM demos';
        if (conditions.length > 0) {
            query += ' WHERE ' + conditions.join(' AND ');
        }
        query += ' ORDER BY date DESC';

        debug.dbQuery(query, params, 2);
        const [demos] = await pool.query(query, params);
        debug.dbResult(demos, 2);

        debug.level2('Fetching blacklisted players');
        const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
        const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));
        debug.level3('Blacklisted players count:', blacklistedPlayers.size);

        const playerGameCounts = {};

        debug.level2('Processing demo data for player counts');
        for (const demo of demos) {
            try {
                let playersData = [];
                if (demo.players) {
                    const parsedData = JSON.parse(demo.players);
                    if (typeof parsedData === 'object') {
                        if (Array.isArray(parsedData.players)) {
                            playersData = parsedData.players;
                        } else if (Array.isArray(parsedData)) {
                            playersData = parsedData;
                        }
                    }
                }

                for (const player of playersData) {
                    if (!player.name) continue;
                    if (blacklistedPlayers.has(player.name.toLowerCase())) continue;
                    if (namesToExclude.includes(player.name)) continue;

                    if (!playerGameCounts[player.name]) {
                        playerGameCounts[player.name] = {
                            player_name: player.name,
                            games_played: 0,
                            last_game_date: null
                        };
                    }

                    playerGameCounts[player.name].games_played++;

                    const gameDate = new Date(demo.date);
                    if (!playerGameCounts[player.name].last_game_date ||
                        gameDate > new Date(playerGameCounts[player.name].last_game_date)) {
                        playerGameCounts[player.name].last_game_date = demo.date;
                    }
                }
            } catch (error) {
                debug.error('getMostActivePlayers', error, 1);
            }
        }

        let activePlayers = Object.values(playerGameCounts);
        debug.level2('Found unique players:', activePlayers.length);

        debug.level2('Adding profile URLs for players');
        activePlayers = await Promise.all(activePlayers.map(async (player) => {
            if (player.player_name) {
                const [userProfile] = await pool.query(`
            SELECT u.username 
            FROM user_profiles up
            JOIN users u ON up.user_id = u.id
            WHERE up.defcon_username = ?
          `, [player.player_name]);

                if (userProfile.length > 0) {
                    player.profileUrl = `/profile/${userProfile[0].username}`;
                }
            }
            return player;
        }));

        activePlayers.sort((a, b) => b.games_played - a.games_played);
        const limitedResults = activePlayers.slice(0, parseInt(limit));

        debug.level2('Returning active players:', limitedResults.length);
        debug.exit('getMostActivePlayers', startTime, { count: limitedResults.length }, 1);
        res.json(limitedResults);
    } catch (error) {
        debug.error('getMostActivePlayers', error, 1);
        debug.exit('getMostActivePlayers', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch active players' });
    }
});


router.get('/api/leaderboard', async (req, res) => {
    const startTime = debug.enter('getLeaderboard', [req.query], 1);
    try {
        const {
            serverName,
            serverList,
            playerName,
            sortBy = 'wins',
            startDate,
            endDate,
            territories,
            combineMode,
            scoreFilter,
            gameDuration,
            scoreDifference,
            gamesPlayed,
            minGames = 1,
            excludeNames = '',
            includeDetailedStats = 'false',
            limit
        } = req.query;

        debug.level2('Leaderboard query parameters:', { 
            serverName, serverList, playerName, sortBy, startDate, endDate, 
            territories, combineMode, scoreFilter, gameDuration, scoreDifference, 
            gamesPlayed, minGames, excludeNames, includeDetailedStats, limit 
        });

        const namesToExclude = excludeNames ? excludeNames.split(',') : [];
        let query = 'SELECT * FROM demos';
        let params = [];
        let conditions = [];


        if (serverList) {

            const servers = serverList.split(',');
            const serverConditions = servers.map(() => 'game_type LIKE ?');
            conditions.push(`(${serverConditions.join(' OR ')})`);
            servers.forEach(server => {
                params.push(`%${server}%`);
            });
        } else if (serverName) {

            conditions.push('game_type LIKE ?');
            params.push(`%${serverName}%`);
        }


        if (playerName) {
            conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                       OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                       OR player9_name LIKE ? OR player10_name LIKE ?)`);
            params = [...params, ...Array(10).fill(`%${playerName}%`)];
        }


        if (territories) {
            const territoryList = territories.split(',');

            if (combineMode === 'true') {
                const territoryChecks = territoryList.map(() => 'players LIKE ?').join(' AND ');
                conditions.push(`(${territoryChecks}) AND JSON_LENGTH(JSON_EXTRACT(players, '$.players')) = ?`);
                territoryList.forEach(territory => {
                    params.push(`%"territory":"${territory}"%`);
                });
                params.push(territoryList.length);
            } else {
                const territoryConditions = territoryList.map(() => 'players LIKE ?');
                conditions.push(`(${territoryConditions.join(' OR ')})`);
                territoryList.forEach(territory => {
                    params.push(`%"territory":"${territory}"%`);
                });
            }
        }


        if (startDate && endDate) {
            conditions.push('date BETWEEN ? AND ?');
            params.push(startDate, endDate);
        } else if (startDate) {
            conditions.push('date >= ?');
            params.push(startDate);
        } else if (endDate) {
            conditions.push('date <= ?');
            params.push(endDate);
        }

        if (conditions.length > 0) {
            query += ' WHERE ' + conditions.join(' AND ');
        }

        query += ' ORDER BY date ASC';


        const [demos] = await pool.query(query, params);
        const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
        const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));
        const playerStats = {};
        const alliances = {};
        const playerNemesis = {};


        for (const demo of demos) {
            try {
                let playersData = [];
                if (demo.players) {
                    const parsedData = JSON.parse(demo.players);
                    if (typeof parsedData === 'object') {

                        if (Array.isArray(parsedData.players)) {
                            playersData = parsedData.players;
                        } else if (Array.isArray(parsedData)) {
                            playersData = parsedData;
                        }
                    }
                }

                if (playersData.length < 2) continue;


                const usingAlliances = playersData.some(player => player.alliance !== undefined);


                const groupScores = {};
                playersData.forEach(player => {
                    const groupId = usingAlliances ? player.alliance : player.team;
                    if (groupId === undefined) return;

                    if (!groupScores[groupId]) {
                        groupScores[groupId] = 0;
                    }
                    groupScores[groupId] += player.score || 0;
                });


                const sortedGroups = Object.entries(groupScores)
                    .sort((a, b) => b[1] - a[1]);

                const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];
                const winningGroupId = isTie ? null : Number(sortedGroups[0][0]);

                for (const player of playersData) {
                    if (!player.name) continue;

                    if (blacklistedPlayers.has(player.name.toLowerCase())) continue;

                    const groupId = usingAlliances ? player.alliance : player.team;
                    if (groupId === undefined) continue;


                    const isWinner = !isTie && groupId === winningGroupId;
                    const isTieGame = isTie;


                    if (!playerStats[player.name]) {
                        playerStats[player.name] = {
                            player_name: player.name,
                            key_id: player.key_id || 'DEMO',
                            games_played: 0,
                            wins: 0,
                            losses: 0,
                            ties: 0,
                            total_score: 0,
                            highest_score: 0,
                            avg_score: 0,
                            games_by_server: {},
                            territories: {},
                            last_game_date: null,
                            nemesis_data: {}
                        };
                    }

                    const stats = playerStats[player.name];
                    stats.games_played++;

                    if (isTieGame) {
                        stats.ties++;
                    } else if (isWinner) {
                        stats.wins++;
                    } else {
                        stats.losses++;


                        if (!isTie && groupId !== winningGroupId) {
                            const winningPlayers = playersData.filter(p => {
                                const pGroupId = usingAlliances ? p.alliance : p.team;
                                return pGroupId === winningGroupId;
                            });


                            winningPlayers.forEach(winner => {
                                if (!winner.name || winner.name === player.name) return;

                                if (!stats.nemesis_data[winner.name]) {
                                    stats.nemesis_data[winner.name] = 0;
                                }
                                stats.nemesis_data[winner.name]++;
                            });
                        }
                    }

                    stats.total_score += player.score || 0;
                    stats.highest_score = Math.max(stats.highest_score, player.score || 0);
                    stats.avg_score = stats.total_score / stats.games_played;

                    const serverType = demo.game_type || 'Unknown';
                    stats.games_by_server[serverType] = (stats.games_by_server[serverType] || 0) + 1;

                    if (player.territory) {
                        if (!stats.territories[player.territory]) {
                            stats.territories[player.territory] = {
                                games: 0,
                                wins: 0,
                                losses: 0,
                                ties: 0,
                                score: 0
                            };
                        }

                        const terrStats = stats.territories[player.territory];
                        terrStats.games++;
                        terrStats.score += player.score || 0;

                        if (isTieGame) {
                            terrStats.ties++;
                        } else if (isWinner) {
                            terrStats.wins++;
                        } else {
                            terrStats.losses++;
                        }
                    }


                    const gameDate = new Date(demo.date);
                    if (!stats.last_game_date || gameDate > new Date(stats.last_game_date)) {
                        stats.last_game_date = demo.date;
                    }


                    if (usingAlliances) {
                        if (!alliances[groupId]) {
                            alliances[groupId] = {
                                id: groupId,
                                total_games: 0,
                                total_wins: 0,
                                players: new Set()
                            };
                        }

                        alliances[groupId].players.add(player.name);
                        alliances[groupId].total_games++;

                        if (isWinner) {
                            alliances[groupId].total_wins++;
                        }
                    }
                }
            } catch (error) {
                console.error('Error processing demo data for leaderboard:', error);
            }
        }


        Object.values(playerStats).forEach(player => {
            player.win_ratio = (player.wins / player.games_played) * 100 || 0;

            const gamesPlayed = player.games_played;
            const wins = player.wins;
            
            if (gamesPlayed === 0) {
                player.weighted_win_ratio = 0;
            } else {
                const n = gamesPlayed;
                const p = wins / n; 
                const z = 1.96; 
                const wilsonScore = (p + (z * z) / (2 * n) - z * Math.sqrt((p * (1 - p) + (z * z) / (4 * n)) / n)) / (1 + (z * z) / n);
                
                player.weighted_win_ratio = Math.max(0, wilsonScore * 100);
                
                if (gamesPlayed < 10) {
                    const lowGamePenalty = Math.pow(gamesPlayed / 10, 2);
                    player.weighted_win_ratio *= lowGamePenalty;
                }
            }

            const volumeBonus = Math.log(player.games_played + 1) / Math.log(101); 
            player.weighted_score = player.wins * (player.weighted_win_ratio / 100) * (1 + volumeBonus);

            if (Object.keys(player.territories).length > 0) {
                let bestTerritoryRatio = -1;
                let worstTerritoryRatio = 101;
                let bestTerritory = null;
                let worstTerritory = null;

                Object.entries(player.territories).forEach(([territory, stats]) => {
                    if (stats.games >= 3) {
                        const winRatio = (stats.wins / stats.games) * 100;

                        if (winRatio > bestTerritoryRatio) {
                            bestTerritoryRatio = winRatio;
                            bestTerritory = territory;
                        }

                        if (winRatio < worstTerritoryRatio) {
                            worstTerritoryRatio = winRatio;
                            worstTerritory = territory;
                        }
                    }
                });

                player.best_territory = bestTerritory;
                player.worst_territory = worstTerritory;
            }

            if (Object.keys(player.nemesis_data).length > 0) {
                let maxLosses = 0;
                let archNemesis = null;

                Object.entries(player.nemesis_data).forEach(([opponent, losses]) => {
                    if (losses > maxLosses) {
                        maxLosses = losses;
                        archNemesis = opponent;
                    }
                });

                player.arch_nemesis = archNemesis;
                player.nemesis_losses = maxLosses;
            }
        });

        let leaderboardData = Object.values(playerStats);


        if (minGames > 1) {
            leaderboardData = leaderboardData.filter(player => player.games_played >= minGames);
        }

        if (scoreFilter) {
            leaderboardData.sort((a, b) => scoreFilter === 'highest'
                ? b.highest_score - a.highest_score
                : a.highest_score - b.highest_score);
        } else if (gameDuration) {

            console.log('Game duration filter not implemented for leaderboard');
        } else {
            switch (sortBy) {
                case 'wins':
                    leaderboardData.sort((a, b) => b.wins - a.wins || b.total_score - a.total_score || a.player_name.localeCompare(b.player_name));
                    break;
                case 'gamesPlayed':
                    leaderboardData.sort((a, b) => b.games_played - a.games_played || b.wins - a.wins || a.player_name.localeCompare(b.player_name));
                    break;
                case 'totalScore':
                    leaderboardData.sort((a, b) => b.total_score - a.total_score || b.wins - a.wins || a.player_name.localeCompare(b.player_name));
                    break;
                case 'highestScore':
                    leaderboardData.sort((a, b) => b.highest_score - a.highest_score || a.player_name.localeCompare(b.player_name));
                    break;
                case 'avgScore':
                    leaderboardData.sort((a, b) => b.avg_score - a.avg_score || a.player_name.localeCompare(b.player_name));
                    break;
                case 'winRatio':
                    leaderboardData.sort((a, b) => {

                        const weightedRatioDiff = b.weighted_win_ratio - a.weighted_win_ratio;
                        if (Math.abs(weightedRatioDiff) > 0.001) {
                            return weightedRatioDiff;
                        }


                        const ratioDiff = b.win_ratio - a.win_ratio;
                        if (Math.abs(ratioDiff) > 0.001) {
                            return ratioDiff;
                        }


                        const gamesDiff = b.games_played - a.games_played;
                        if (gamesDiff !== 0) {
                            return gamesDiff;
                        }


                        return a.player_name.localeCompare(b.player_name);
                    });
                    break;
                case 'weightedScore':
                    leaderboardData.sort((a, b) => b.weighted_score - a.weighted_score || a.player_name.localeCompare(b.player_name));
                    break;
                case 'recent':
                    leaderboardData.sort((a, b) => new Date(b.last_game_date) - new Date(a.last_game_date));
                    break;
                default:
                    leaderboardData.sort((a, b) => b.wins - a.wins || b.total_score - a.total_score || a.player_name.localeCompare(b.player_name));
            }
        }

        leaderboardData.forEach((player, index) => {
            player.absolute_rank = index + 1;
        });

        const rankedLeaderboard = await Promise.all(leaderboardData.map(async (player) => {
            if (player.player_name) {
                const [userProfile] = await pool.query(`
            SELECT u.username 
            FROM user_profiles up
            JOIN users u ON up.user_id = u.id
            WHERE up.defcon_username = ?
          `, [player.player_name]);

                if (userProfile.length > 0) {
                    player.profileUrl = `/profile/${userProfile[0].username}`;
                }
            }
            return player;
        }));


        if (namesToExclude.length > 0) {
            leaderboardData = leaderboardData.filter(player =>
                !namesToExclude.includes(player.player_name)
            );
        }


        if (limit && !isNaN(parseInt(limit))) {
            leaderboardData = leaderboardData.slice(0, parseInt(limit));
        }


        if (includeDetailedStats !== 'true') {
            leaderboardData.forEach(player => {
                delete player.nemesis_data;
            });
        }


        debug.level2('Returning leaderboard data:', { totalPlayers: leaderboardData.length });
        debug.exit('getLeaderboard', startTime, { totalPlayers: leaderboardData.length }, 1);
        res.json({
            leaderboard: leaderboardData,
            totalPlayers: leaderboardData.length,
            filters: {
                serverName,
                serverList,
                playerName,
                territories,
                startDate,
                endDate,
                minGames
            }
        });
    } catch (error) {
        debug.error('getLeaderboard', error, 1);
        debug.exit('getLeaderboard', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to generate leaderboard' });
    }
});

router.get('/api/player-nemesis', async (req, res) => {
    const startTime = debug.enter('getPlayerNemesis', [req.query], 1);
    try {
        const { playerName, startDate, endDate } = req.query;
        debug.level2('Player nemesis query:', { playerName, startDate, endDate });

        if (!playerName) {
            debug.level1('Player nemesis request missing player name');
            debug.exit('getPlayerNemesis', startTime, 'missing_player_name', 1);
            return res.status(400).json({ error: 'Player name is required' });
        }


        let conditions = ['(player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ? OR ' +
            'player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ? OR ' +
            'player9_name = ? OR player10_name = ?)'];
        let params = Array(10).fill(playerName);


        if (startDate && endDate) {
            conditions.push('date BETWEEN ? AND ?');
            params.push(startDate, endDate);
        } else if (startDate) {
            conditions.push('date >= ?');
            params.push(startDate);
        } else if (endDate) {
            conditions.push('date <= ?');
            params.push(endDate);
        }


        const query = `SELECT * FROM demos WHERE ${conditions.join(' AND ')}`;
        const [demos] = await pool.query(query, params);


        const opponentLosses = {};

        for (const demo of demos) {
            try {

                let playersData = [];
                if (demo.players) {
                    const parsedData = JSON.parse(demo.players);
                    if (typeof parsedData === 'object') {

                        if (Array.isArray(parsedData.players)) {
                            playersData = parsedData.players;
                        } else if (Array.isArray(parsedData)) {
                            playersData = parsedData;
                        }
                    }
                }

                if (playersData.length < 2) continue;


                const playerInfo = playersData.find(p => p.name === playerName);
                if (!playerInfo) continue;

                const usingAlliances = playersData.some(player => player.alliance !== undefined);
                const playerGroupId = usingAlliances ? playerInfo.alliance : playerInfo.team;


                const groupScores = {};
                playersData.forEach(player => {
                    const groupId = usingAlliances ? player.alliance : player.team;
                    if (groupId === undefined) return;

                    if (!groupScores[groupId]) {
                        groupScores[groupId] = 0;
                    }
                    groupScores[groupId] += player.score || 0;
                });


                const sortedGroups = Object.entries(groupScores)
                    .sort((a, b) => b[1] - a[1]);

                const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];
                const winningGroupId = isTie ? null : Number(sortedGroups[0][0]);


                if (!isTie && playerGroupId !== winningGroupId) {

                    const winningPlayers = playersData.filter(p => {
                        const pGroupId = usingAlliances ? p.alliance : p.team;
                        return pGroupId === winningGroupId;
                    });


                    winningPlayers.forEach(winner => {
                        if (!winner.name || winner.name === playerName) return;

                        if (!opponentLosses[winner.name]) {
                            opponentLosses[winner.name] = 0;
                        }
                        opponentLosses[winner.name]++;
                    });
                }
            } catch (error) {
                debug.error('getPlayerNemesis', error, 1);
            }
        }

        let nemesis = null;
        let maxLosses = 0;

        Object.entries(opponentLosses).forEach(([opponent, losses]) => {
            if (losses > maxLosses) {
                nemesis = opponent;
                maxLosses = losses;
            }
        });

        debug.level2('Nemesis calculation complete:', { nemesis, maxLosses });
        debug.exit('getPlayerNemesis', startTime, { nemesis, maxLosses }, 1);
        res.json({
            playerName,
            nemesis: nemesis || 'None',
            lossCount: maxLosses,
            allOpponents: opponentLosses
        });

    } catch (error) {
        debug.error('getPlayerNemesis', error, 1);
        debug.exit('getPlayerNemesis', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch nemesis data' });
    }
});

module.exports = router;