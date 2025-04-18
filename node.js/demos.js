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
//Last Edited 18-04-2025

const path = require('path');
const fs = require('fs');
const pendingDemos = new Map();
const pendingJsons = new Map();
const processedJsons = new Set();

const {
    pool
} = require('./constants');

const {
    sendDemoToDiscord,
    discordState
}   = require('./discord')

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


function cleanupOldPendingFiles() {
    const oneHourAgo = Date.now() - 60 * 60 * 1000; 

    for (const [fileName, fileInfo] of pendingDemos) {
        if (fileInfo.addedTime < oneHourAgo) {
            pendingDemos.delete(fileName);
            console.log(`Removed old pending demo file: ${fileName}`);
        }
    }

    for (const [fileName, fileInfo] of pendingJsons) {
        if (fileInfo.addedTime < oneHourAgo) {
            pendingJsons.delete(fileName);
            console.log(`Removed old pending JSON file: ${fileName}`);
        }
    }
}

setInterval(cleanupOldPendingFiles, 60 * 60 * 1000);

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

async function processDemoFile(demoFileName, fileSize, logData, jsonFileName) {
    if (!discordState.isReady) {
        console.log('Discord bot not ready, waiting...');
        await new Promise((resolve) => {
            const checkInterval = setInterval(() => {
                if (discordState.isReady) { 
                    clearInterval(checkInterval);
                    resolve();
                }
            }, 1000);
        });
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

function allPlayersHaveZeroScore(logData) {
    if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
        return false;
    }

    return logData.players.every(player => player.score === 0);
}

module.exports = {
    checkForMatch,
    cleanupOldPendingFiles,
    processDemoFile,
    allPlayersHaveZeroScore,
    demoExistsInDatabase,
    pendingDemos,
    pendingJsons,
    processedJsons 
};