const path = require('path');
const fs = require('fs');
const {
    pool,
    pendingDemos,
    pendingJsons,
    processedJsons
} = require('../Utils/shared-utils');

const { sendDemoToDiscord } = require('./discord-functions');

let discordBotReady = false;

async function checkForMatch() {
    const processedPairs = new Set(); 

    function allPlayersHaveZeroScore(logData) {
        if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
            return false;
        }
        return logData.players.every(player => player.score === 0);
    }

    for (const [jsonFileName, jsonInfo] of pendingJsons) {
        try {
            const jsonContent = await fs.promises.readFile(jsonInfo.path, 'utf8');
            const logData = JSON.parse(jsonContent);
            const recordSetting = logData.settings && logData.settings.Record;
            if (!recordSetting) {
                console.log(`No Record setting found in JSON file: ${jsonFileName}`);
                continue;
            }

            const dcrecFileName = path.basename(recordSetting);
            const demoInfo = pendingDemos.get(dcrecFileName);
            const pairKey = `${dcrecFileName}-${jsonFileName}`;
            if (processedPairs.has(pairKey)) {
                continue;
            }

            if (demoInfo) {
                console.log('Processing game data:', {
                    fileName: jsonFileName,
                    gameType: logData.gameType,
                    isTournament: logData.gameType?.toLowerCase().includes('tournament'),
                    is1v1: logData.gameType?.toLowerCase().includes('1v1'),
                    playerCount: logData.players?.length
                });

                console.log(`Matching files found: ${dcrecFileName} and ${jsonFileName}`);

                try {
                    await pool.query('START TRANSACTION');

                    if (allPlayersHaveZeroScore(logData)) {
                        console.log(`Skipping game ${dcrecFileName} - all players have score 0, likely an incomplete game`);
                        processedPairs.add(pairKey);
                        pendingDemos.delete(dcrecFileName);
                        pendingJsons.delete(jsonFileName);
                        processedJsons.add(jsonFileName);
                        await pool.query('COMMIT');
                        continue;
                    }

                    await processDemoFile(dcrecFileName, demoInfo.stats.size, logData, jsonFileName);

                    processedPairs.add(pairKey);
                    pendingDemos.delete(dcrecFileName);
                    pendingJsons.delete(jsonFileName);
                    processedJsons.add(jsonFileName);

                    console.log(`Successfully processed and linked ${dcrecFileName} with ${jsonFileName}`);

                    if (logData.gameType && (
                        logData.gameType.toLowerCase().includes('tournament') ||
                        logData.gameType.toLowerCase().includes('1v1')
                    )) {
                        console.log(`Detected ${logData.gameType.toLowerCase().includes('tournament') ? 'tournament' : '1v1'} game, updating leaderboard`);
                        await updateLeaderboard(logData);
                    }

                    await pool.query('COMMIT');
                } catch (error) {
                    await pool.query('ROLLBACK');
                    console.error(`Error processing demo/json pair: ${error}`);
                    continue;
                }
            }
        } catch (error) {
            console.error(`Error processing JSON file ${jsonFileName}:`, error);
        }
    }

    for (const [demoFileName, demoInfo] of pendingDemos) {
        let expectedJsonPrefix;
        if (demoFileName.endsWith('.d8crec')) {
            expectedJsonPrefix = 'game8p_';
        } else if (demoFileName.endsWith('.d10crec')) {
            expectedJsonPrefix = 'game10p_';
        } else {
            expectedJsonPrefix = 'game_';
        }

        for (const [jsonFileName, jsonInfo] of pendingJsons) {
            if (!jsonFileName.startsWith(expectedJsonPrefix)) continue;

            const pairKey = `${demoFileName}-${jsonFileName}`;
            if (processedPairs.has(pairKey)) {
                continue;
            }

            try {
                const jsonContent = await fs.promises.readFile(jsonInfo.path, 'utf8');
                const logData = JSON.parse(jsonContent);

                const recordSetting = logData.settings && logData.settings.Record;
                if (recordSetting && path.basename(recordSetting) === demoFileName) {
                    try {
                        await pool.query('START TRANSACTION');

                        if (allPlayersHaveZeroScore(logData)) {
                            console.log(`Skipping game ${demoFileName} - all players have score 0, likely an incomplete game`);
                            processedPairs.add(pairKey);
                            pendingDemos.delete(demoFileName);
                            pendingJsons.delete(jsonFileName);
                            processedJsons.add(jsonFileName);
                            await pool.query('COMMIT');
                            continue;
                        }

                        console.log(`Matching files found: ${demoFileName} and ${jsonFileName}`);
                        await processDemoFile(demoFileName, demoInfo.stats.size, logData, jsonFileName);

                        processedPairs.add(pairKey);
                        pendingDemos.delete(demoFileName);
                        pendingJsons.delete(jsonFileName);
                        processedJsons.add(jsonFileName);

                        console.log(`Successfully processed and linked ${demoFileName} with ${jsonFileName}`);

                        if (logData.gameType && (
                            logData.gameType.toLowerCase().includes('tournament') ||
                            logData.gameType.toLowerCase().includes('1v1')
                        )) {
                            console.log(`Detected ${logData.gameType.toLowerCase().includes('tournament') ? 'tournament' : '1v1'} game, updating leaderboard`);
                            await updateLeaderboard(logData);
                        }

                        await pool.query('COMMIT');
                        break;
                    } catch (error) {
                        await pool.query('ROLLBACK');
                        console.error(`Error processing demo/json pair: ${error}`);
                        continue;
                    }
                }
            } catch (error) {
                console.error(`Error processing JSON file ${jsonFileName}:`, error);
            }
        }
    }
}

async function processDemoFile(demoFileName, fileSize, logData, jsonFileName) {
    if (!discordBotReady) {
        console.log('Waiting for Discord bot to be ready before processing demo...');
        return;
    }

    console.log(`Processing demo file: ${demoFileName}`);

    try {
        await pool.query('START TRANSACTION');

        const [existingDemo] = await pool.query('SELECT * FROM demos WHERE name = ? FOR UPDATE', [demoFileName]);

        if (existingDemo.length > 0) {
            console.log(`Demo ${demoFileName} already exists in the database. Skipping upload.`);
            await pool.query('COMMIT');
            return;
        }

        const playerCount = logData.players ? logData.players.length : 0;
        const gameType = logData.gameType || `DefconExpanded ${playerCount} Player`;
        const duration = logData.duration || null;
        const allianceMapping = new Map();

        if (logData.alliance_history) {
            logData.alliance_history.forEach(event => {
                if (event.action === 'JOIN') {
                    allianceMapping.set(event.team, event.alliance);
                }
            });
        }

        const processedPlayers = logData.players.map(player => {
            if (player.alliance !== undefined) {
                return { ...player };
            }

            if (allianceMapping.has(player.team)) {
                return {
                    ...player,
                    alliance: allianceMapping.get(player.team)
                };
            }

            return {
                ...player,
                alliance: player.team
            };
        });

        const processedSpectators = logData.spectators ? logData.spectators.map(spectator => ({
            name: spectator.name,
            version: spectator.version,
            platform: spectator.platform,
            key_id: spectator.key_id
        })) : [];

        const fullGameData = {
            players: processedPlayers,
            spectators: processedSpectators
        };

        const playerData = {};
        const playerColumns = ['player1', 'player2', 'player3', 'player4', 'player5', 'player6', 'player7', 'player8', 'player9', 'player10'];
        processedPlayers.forEach((player, index) => {
            if (index < 10) {
                const prefix = playerColumns[index];
                playerData[`${prefix}_name`] = player.name || null;
                playerData[`${prefix}_team`] = player.team !== undefined ? player.team : null;
                playerData[`${prefix}_score`] = player.score !== undefined ? player.score : null;
                playerData[`${prefix}_territory`] = player.territory || null;
                playerData[`${prefix}_key_id`] = player.key_id || null;
            }
        });

        for (let i = playerCount; i < 10; i++) {
            const prefix = playerColumns[i];
            playerData[`${prefix}_name`] = null;
            playerData[`${prefix}_team`] = null;
            playerData[`${prefix}_score`] = null;
            playerData[`${prefix}_territory`] = null;
            playerData[`${prefix}_key_id`] = null;
        }

        const columns = [
            'name',
            'size',
            'date',
            'game_type',
            'duration',
            'players',
            ...Object.keys(playerData),
            'log_file'
        ];

        const values = [
            demoFileName,
            fileSize,
            new Date(logData.start_time),
            gameType,
            duration,
            JSON.stringify(fullGameData),
            ...Object.values(playerData),
            jsonFileName
        ];

        const placeholders = columns.map(() => '?').join(', ');
        const query = `INSERT INTO demos (${columns.join(', ')}) VALUES (${placeholders})`;

        const [result] = await pool.query(query, values);

        await pool.query('COMMIT');

        console.log(`Demo ${demoFileName} processed and added to database with player and spectator data`);
        console.log(`Inserted row ID: ${result.insertId}`);

        console.log('Processed game data:', JSON.stringify(fullGameData, null, 2));

        try {
            const [newDemo] = await pool.query('SELECT * FROM demos WHERE id = ?', [result.insertId]);
            if (newDemo.length > 0) {
                await sendDemoToDiscord(newDemo[0], logData);
                console.log(`Demo ${demoFileName} successfully posted to Discord`);
            }
        } catch (discordError) {
            console.error(`Error posting demo ${demoFileName} to Discord:`, discordError);
        }

    } catch (error) {
        await pool.query('ROLLBACK');
        console.error(`Error processing demo ${demoFileName}:`, error);
        throw error;
    }
}

async function demoExistsInDatabase(demoFileName) {
    try {
        const [rows] = await pool.query(
            'SELECT COUNT(*) as count FROM (SELECT name FROM demos WHERE name = ? UNION SELECT demo_name FROM deleted_demos WHERE demo_name = ?) as combined',
            [demoFileName, demoFileName]
        );
        return rows[0].count > 0;
    } catch (error) {
        console.error(`Error checking if demo exists in database: ${error}`);
        return false;
    }
}

async function updateLeaderboard(gameData) {
    console.log('Checking game type:', gameData.gameType);

    const isTournament = gameData.gameType && gameData.gameType.toLowerCase().includes('tournament');
    const is1v1 = gameData.gameType && gameData.gameType.toLowerCase().includes('1v1');

    if (!isTournament && !is1v1) {
        console.log('Not a tracked game type:', gameData.gameType);
        return;
    }

    try {
        await pool.query('START TRANSACTION');

        let players;
        if (gameData.players && Array.isArray(gameData.players)) {
            players = gameData.players;
        } else if (typeof gameData.players === 'object' && gameData.players.players) {
            players = gameData.players.players;
        } else {
            console.log('Invalid player data structure:', gameData);
            await pool.query('ROLLBACK');
            return;
        }

        if (isTournament) {
            await processTeamTournament(players);
        } else if (is1v1) {
            await process1v1Match(players);
        }

        await pool.query('COMMIT');
    } catch (error) {
        await pool.query('ROLLBACK');
        console.error('Error updating leaderboard:', error);
    }
}

async function processTeamTournament(players) {
    console.log('Tournament game detected, processing for tournament leaderboard');

    const allianceGroups = {};
    players.forEach(player => {
        if (!allianceGroups[player.alliance]) {
            allianceGroups[player.alliance] = [];
        }
        allianceGroups[player.alliance].push(player);
    });

    const allianceScores = Object.entries(allianceGroups).map(([alliance, alliancePlayers]) => ({
        alliance: parseInt(alliance),
        players: alliancePlayers,
        totalScore: alliancePlayers.reduce((sum, player) => sum + (player.score || 0), 0)
    }));

    const sortedAlliances = allianceScores.sort((a, b) => b.totalScore - a.totalScore);
    const winningAlliance = sortedAlliances[0].alliance;

    console.log('Alliance scores:', sortedAlliances.map(a =>
        `Alliance ${a.alliance}: ${a.totalScore} (${a.players.map(p => p.name).join(', ')})`
    ));
    console.log(`Winning alliance: ${winningAlliance} with score ${sortedAlliances[0].totalScore}`);

    for (const player of players) {
        const isWinner = player.alliance === winningAlliance;
        try {
            const [existingPlayerByName] = await pool.query(
                'SELECT * FROM tournament_leaderboard WHERE player_name = ?',
                [player.name]
            );

            if (existingPlayerByName.length > 0) {
                await pool.query(`
                    UPDATE tournament_leaderboard 
                    SET games_played = games_played + 1,
                        wins = wins + ?,
                        losses = losses + ?
                    WHERE player_name = ?
                `, [
                    isWinner ? 1 : 0,
                    isWinner ? 0 : 1,
                    player.name
                ]);
                console.log(`Updated tournament data for ${player.name} (Alliance ${player.alliance}). Added ${isWinner ? 'win' : 'loss'}.`);
            } else {
                await pool.query(`
                    INSERT INTO tournament_leaderboard (player_name, key_id, games_played, wins, losses)
                    VALUES (?, ?, 1, ?, ?)
                `, [
                    player.name,
                    player.key_id || 'DEMO',
                    isWinner ? 1 : 0,
                    isWinner ? 0 : 1
                ]);
                console.log(`New player ${player.name} added to tournament leaderboard with initial ${isWinner ? 'win' : 'loss'}.`);
            }
            console.log(`Tournament leaderboard update completed for player ${player.name} (Key ID: ${player.key_id})`);
        } catch (error) {
            console.error(`Error updating tournament leaderboard for player ${player.name}:`, error);
            throw error;
        }
    }
}

async function process1v1Match(players) {
    if (players.length !== 2) {
        console.log(`Invalid player count for 1v1: ${players.length}`);
        await pool.query('ROLLBACK');
        return;
    }

    const [whitelist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const whitelistedPlayers = new Set(whitelist.map(entry => entry.player_name.toLowerCase()));

    const winner = players.reduce((a, b) => a.score > b.score ? a : b);
    const loser = players.find(p => p !== winner);

    console.log(`Winner: ${winner.name} (Score: ${winner.score})`);
    console.log(`Loser: ${loser.name} (Score: ${loser.score})`);

    for (const player of players) {
        if (whitelistedPlayers.has(player.name.toLowerCase())) {
            console.log(`Player ${player.name} is whitelisted. Skipping leaderboard update.`);
            continue;
        }

        const isWinner = player === winner;
        const scoreToAdd = player.score > 0 ? player.score : 0;

        try {
            const [existingPlayerByName] = await pool.query('SELECT * FROM leaderboard WHERE player_name = ?', [player.name]);
            let existingPlayerByKeyId = [];

            if (player.key_id !== 'DEMO') {
                [existingPlayerByKeyId] = await pool.query('SELECT * FROM leaderboard WHERE key_id = ?', [player.key_id]);
            }

            if (existingPlayerByName.length > 0) {
                await updateExistingPlayer(player, existingPlayerByName[0], isWinner, scoreToAdd);
            } else if (existingPlayerByKeyId.length > 0) {
                await updateExistingPlayerByKeyId(player, existingPlayerByKeyId[0], isWinner, scoreToAdd);
            } else {
                await addNewPlayer(player, isWinner, scoreToAdd);
            }

        } catch (error) {
            console.error(`Error updating leaderboard for player ${player.name}:`, error);
            throw error;
        }
    }
}

async function updateExistingPlayer(player, existingPlayer, isWinner, scoreToAdd) {
    if (player.key_id === 'DEMO') {
        if (existingPlayer.key_id !== 'DEMO') {
            console.log(`Looks like ${player.name} has lost their authentication key :( Adding player data to the leaderboard assuming they will find their authentication key at some point.`);
        } else {
            console.log(`Demo player detected: ${player.name}. Ignoring key ID matching and using player name instead.`);
        }
    } else if (existingPlayer.key_id === 'DEMO' && player.key_id !== 'DEMO') {
        console.log(`Demo player ${player.name} has now bought the game! Replacing DEMO key ID with ${player.key_id}`);
        await pool.query('UPDATE leaderboard SET key_id = ? WHERE player_name = ?', [player.key_id, player.name]);
    } else if (existingPlayer.key_id !== player.key_id) {
        console.log(`${player.name} appears to have a new key ID. Replacing existing key ID ${existingPlayer.key_id} with new key ID ${player.key_id} in the database.`);
        await pool.query('UPDATE leaderboard SET key_id = ? WHERE player_name = ?', [player.key_id, player.name]);
    }

    await pool.query(`
        UPDATE leaderboard 
        SET games_played = games_played + 1,
            wins = wins + ?,
            losses = losses + ?,
            total_score = total_score + ?
        WHERE player_name = ?
    `, [
        isWinner ? 1 : 0,
        isWinner ? 0 : 1,
        scoreToAdd,
        player.name
    ]);
    console.log(`Updated player data for ${player.name} (Key ID: ${player.key_id}). Added ${isWinner ? 'win' : 'loss'} and ${scoreToAdd} to total score.`);
}

async function updateExistingPlayerByKeyId(player, existingPlayer, isWinner, scoreToAdd) {
    const existingName = existingPlayer.player_name;
    console.log(`Key ID ${player.key_id} matches existing player ${existingName}, but current name is ${player.name}. Updating stats for ${existingName}.`);

    await pool.query(`
        UPDATE leaderboard 
        SET games_played = games_played + 1,
            wins = wins + ?,
            losses = losses + ?,
            total_score = total_score + ?
        WHERE key_id = ?
    `, [
        isWinner ? 1 : 0,
        isWinner ? 0 : 1,
        scoreToAdd,
        player.key_id
    ]);
    console.log(`Updated player data for ${existingName} (Key ID: ${player.key_id}). Added ${isWinner ? 'win' : 'loss'} and ${scoreToAdd} to total score.`);
}

async function addNewPlayer(player, isWinner, scoreToAdd) {
    console.log(`Adding new player ${player.name} with Key ID ${player.key_id} to the leaderboard.`);
    await pool.query(`
        INSERT INTO leaderboard (player_name, key_id, games_played, wins, losses, total_score)
        VALUES (?, ?, 1, ?, ?, ?)
    `, [
        player.name,
        player.key_id,
        isWinner ? 1 : 0,
        isWinner ? 0 : 1,
        scoreToAdd
    ]);
    console.log(`New player ${player.name} added to leaderboard with initial ${isWinner ? 'win' : 'loss'} and score of ${scoreToAdd}.`);
}

module.exports = {
    checkForMatch,
    processDemoFile,
    demoExistsInDatabase,
    updateLeaderboard,
    processTeamTournament,
    process1v1Match,
    setDiscordBotReady: (ready) => { discordBotReady = ready; }
};