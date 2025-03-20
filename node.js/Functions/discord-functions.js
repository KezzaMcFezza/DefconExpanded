const { Client, GatewayIntentBits, EmbedBuilder } = require('discord.js');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const chokidar = require('chokidar');
const { pool } = require('./database-backup');
const utils = require('../Utils/shared-utils');

// Discord configuration
const DISCORD_CONFIG = {
  token: 'MTMwNTQxNTU1NzM2NTEwNDY4MA.G6fE2Q.YywmUxfaA368tePVwCysMI_WAPC4p9OTD69f10',
  channelIds: [
    '1117301291652227163',
    '1305455377508470804',
    '1305580168810725447'
  ]
};

// Global variables
let discordBotReady = false;
let pendingInitialization = null;
let completePendingInitialization = null;
const pendingDemos = new Map();
const pendingJsons = new Map();
const processedJsons = new Set();
const pendingVerifications = new Map();

// Directory paths - updated to go up two directory levels
const demoDir = path.join(__dirname, '..', '..', 'demo_recordings');
const gameLogsDir = path.join(__dirname, '..', '..', 'game_logs');

// Create Discord bot client
const discordBot = new Client({
  intents: [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMessages
  ]
});

// Bot ready event
discordBot.once('ready', () => {
  console.log(`Discord bot logged in as ${discordBot.user.tag}`);
  discordBotReady = true;
  if (pendingInitialization) {
    console.log('Discord bot ready, proceeding with pending initialization...');
    completePendingInitialization();
  }
});

// Initialize Discord bot
const initializeDiscordBot = () => {
  discordBot.login(DISCORD_CONFIG.token);
};

// Shutdown Discord bot
const shutdownDiscordBot = () => {
  discordBotReady = false;
  pendingInitialization = null;
  completePendingInitialization = null;
};

// Send demo to Discord
const sendDemoToDiscord = async (demo, logData) => {
  if (!discordBotReady) {
    console.log('Discord bot not ready, waiting...');
    await new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (discordBotReady) {
          clearInterval(checkInterval);
          resolve();
        }
      }, 1000);
    });
  }

  try {
    for (const channelId of DISCORD_CONFIG.channelIds) {
      try {
        const channel = await discordBot.channels.fetch(channelId);
        if (!channel) {
          console.log(`Could not find channel with ID: ${channelId}`);
          continue;
        }

        const vanillaAllianceColors = {
          0: { color: '#ff4949', name: 'Red', emoji: '🔴' },
          1: { color: '#00bf00', name: 'Green', emoji: '🟢' },
          2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
          3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
          4: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
          5: { color: '#00e5ff', name: 'Turq', emoji: '🔷' }
        };

        const expandedAllianceColors = {
          0: { color: '#00bf00', name: 'Green', emoji: '🟢' },
          1: { color: '#ff4949', name: 'Red', emoji: '🔴' },
          2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
          3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
          4: { color: '#00e5ff', name: 'Turq', emoji: '🔷' },
          5: { color: '#e72de0', name: 'Pink', emoji: '🟣' },
          6: { color: '#4c4c4c', name: 'Black', emoji: '⚫' },
          7: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
          8: { color: '#28660a', name: 'Olive', emoji: '🟢' },
          9: { color: '#660011', name: 'Scarlet', emoji: '🔴' },
          10: { color: '#2a00ff', name: 'Indigo', emoji: '🔵' },
          11: { color: '#4c4c00', name: 'Gold', emoji: '🟡' },
          12: { color: '#004c3e', name: 'Teal', emoji: '🔷' },
          13: { color: '#6a007f', name: 'Purple', emoji: '🟣' },
          14: { color: '#e5e5e5', name: 'White', emoji: '⚪' },
          15: { color: '#964B00', name: 'Brown', emoji: '🟤' }
        };

        const isExpandedServer = (logData.players && logData.players.length > 6) ||
          demo.game_type.includes('8 Player') ||
          demo.game_type.includes('4v4') ||
          demo.game_type.includes('10 Player') ||
          demo.game_type.includes('5v5') ||
          demo.game_type.includes('16 Player') ||
          demo.game_type.includes('8v8') ||
          demo.game_type.includes('509') ||
          demo.game_type.includes('CG') ||
          demo.game_type.includes('MURICON');

        const allianceColors = isExpandedServer ? expandedAllianceColors : vanillaAllianceColors;

        const territoryImages = {
          'North America': 'northamerica',
          'South America': 'southamerica',
          'Europe': 'europe',
          'Africa': 'africa',
          'Asia': 'asia',
          'Russia': 'russia',
          'North Africa': 'northafrica',
          'South Africa': 'southafrica',
          'East Asia': 'eastasia',
          'West Asia': 'westasia',
          'South Asia': 'southasia',
          'Australasia': 'australasia',
          'Antartica': 'antartica'
        };

        const embed = new EmbedBuilder()
          .setColor('#0099ff')
          .setTitle(demo.game_type || 'Defcon Game')
          .setDescription('---------------------------------------------------------------------');

        // Add fields to embed
        if (demo.duration) {
          embed.addFields({
            name: 'Game Duration',
            value: demo.duration.split('.')[0],
            inline: true
          });
        }

        if (demo.date) {
          embed.addFields({
            name: 'Date',
            value: new Date(demo.date).toLocaleString(),
            inline: true
          });
        }

        if (logData.players && Array.isArray(logData.players)) {
          let playerGroups = {};

          logData.players.forEach(player => {
            if (player.alliance !== undefined) {
              playerGroups[player.alliance] = playerGroups[player.alliance] || [];
              playerGroups[player.alliance].push(player);
            }
          });

          const sortedGroups = Object.values(playerGroups).sort((a, b) => {
            const scoreA = a.reduce((sum, p) => sum + (p.score || 0), 0);
            const scoreB = b.reduce((sum, p) => sum + (p.score || 0), 0);
            return scoreB - scoreA;
          });

          sortedGroups.forEach(group => {
            const sortedPlayers = group.sort((a, b) => (b.score || 0) - (a.score || 0));
            const teamColor = allianceColors[sortedPlayers[0].alliance] || allianceColors[0];

            const playersText = sortedPlayers.map(player =>
              `${player.name} | ${player.territory} = ${player.score || 0}`
            ).join('\n');

            embed.addFields({
              name: `${teamColor.emoji} ${teamColor.name}`,
              value: playersText,
              inline: false
            });
          });

          if (sortedGroups.length >= 2) {
            const winningGroup = sortedGroups[0];
            const runnerUpGroup = sortedGroups[1];
            const winningScore = winningGroup.reduce((sum, p) => sum + (p.score || 0), 0);
            const runnerUpScore = runnerUpGroup.reduce((sum, p) => sum + (p.score || 0), 0);
            const scoreDifference = winningScore - runnerUpScore;

            const winningTeamColor = allianceColors[winningGroup[0].alliance] || allianceColors[0];
            embed.addFields({
              name: '\u200b',
              value: `**${winningTeamColor.name}** won by ${scoreDifference} points`,
              inline: false
            });
          }
        }

        if (demo.name) {
          embed.addFields({
            name: 'Download',
            value: `[Click to download](https://defconexpanded.com/api/download/${demo.name})`,
            inline: true
          });
        }

        const spectatorsText = logData.spectators && logData.spectators.length > 0
          ? logData.spectators
            .filter(s => s.name)
            .map(s => s.name)
            .join(', ') || 'No spectators'
          : 'No spectators';

        embed.addFields({
          name: 'Spectators',
          value: spectatorsText,
          inline: true
        });

        try {
          const mapBuffer = await createTerritoryMap(logData.players, territoryImages, false, allianceColors, allianceColors);
          if (mapBuffer) {
            embed.setImage('attachment://map.png');
            await channel.send({
              embeds: [embed],
              files: [{
                attachment: mapBuffer,
                name: 'map.png'
              }]
            });
          } else {
            await channel.send({ embeds: [embed] });
          }
        } catch (mapError) {
          console.error('Error generating territory map:', mapError);
          await channel.send({ embeds: [embed] });
        }

      } catch (channelError) {
        console.error(`Error sending to channel ${channelId}:`, channelError);
        continue;
      }
    }
  } catch (error) {
    console.error('Error sending demo to Discord:', error);
  }
};

// Create territory map - updated image paths to go up two directory levels
const createTerritoryMap = async (players, territoryImages, usingAlliances, teamColors, allianceColors) => {
  try {
    const { createCanvas, loadImage } = require('canvas');
    const canvas = createCanvas(800, 400);
    const ctx = canvas.getContext('2d');

    const baseMap = await loadImage(path.join(__dirname, '..', '..', 'public', 'images', 'base_map.png'));
    ctx.drawImage(baseMap, 0, 0, 800, 400);

    ctx.globalAlpha = 0.7;

    for (const player of players) {
      const territory = territoryImages[player.territory];
      if (!territory) continue;

      const colorSystem = allianceColors;
      const color = colorSystem[player.alliance]?.color || '#00bf00';
      const colorHex = color.replace('#', '');

      try {
        const overlayPath = path.join(__dirname, '..', '..', 'public', 'images', `${territory}${colorHex}.png`);
        const overlay = await loadImage(overlayPath);
        ctx.drawImage(overlay, 0, 0, 800, 400);
      } catch (err) {
        console.error(`Error loading territory overlay for ${territory}:`, err);
      }
    }

    ctx.globalAlpha = 1.0;

    return canvas.toBuffer();
  } catch (error) {
    console.error('Error creating territory map:', error);
    return null;
  }
};

// Initialize server
const initializeServer = async () => {
  console.log("Starting server initialization...");

  // Create a promise that resolves when everything is ready
  const initializationPromise = new Promise((resolve, reject) => {
    if (discordBotReady) {
      resolve();
    } else {
      console.log('Waiting for Discord bot to be ready...');
      pendingInitialization = true;
      completePendingInitialization = resolve;
    }
  });

  // Wait for Discord bot to be ready
  await initializationPromise;

  console.log("Discord bot is ready, proceeding with demo processing...");

  pendingDemos.clear();
  pendingJsons.clear();
  processedJsons.clear();

  const demoFiles = await fs.promises.readdir(demoDir);
  console.log(`Found ${demoFiles.length} demo files in the directory.`);

  const jsonFiles = await fs.promises.readdir(gameLogsDir);
  console.log(`Found ${jsonFiles.length} JSON files in the directory.`);

  const [rows] = await pool.query('SELECT name, log_file FROM demos');
  const existingDemos = new Map(rows.map(row => [row.name, row.log_file]));
  const existingJsonFiles = new Set(rows.map(row => row.log_file));

  for (const demoFile of demoFiles) {
    if (demoFile.endsWith('.dcrec')) {
      if (!existingDemos.has(demoFile)) {
        const demoStats = await fs.promises.stat(path.join(demoDir, demoFile));
        pendingDemos.set(demoFile, { stats: demoStats, addedTime: Date.now() });
        console.log(`Existing demo ${demoFile} added to pending list.`);
      } else {
        console.log(`Demo ${demoFile} already exists in the database. Skipping.`);
      }
    }
  }

  for (const jsonFile of jsonFiles) {
    if (jsonFile.endsWith('_full.json')) {
      if (!existingJsonFiles.has(jsonFile)) {
        pendingJsons.set(jsonFile, { path: path.join(gameLogsDir, jsonFile), addedTime: Date.now() });
        console.log(`Existing JSON ${jsonFile} added to pending list.`);
      } else {
        processedJsons.add(jsonFile);
        console.log(`JSON ${jsonFile} is already linked to a demo in the database. Skipping.`);
      }
    }
  }

  console.log(`Loaded ${pendingDemos.size} pending demo files.`);
  console.log(`Loaded ${pendingJsons.size} pending JSON files.`);
  console.log(`${existingDemos.size} demos already exist in the database.`);

  await checkForMatch();
};

// Check if a demo exists in the database
const demoExistsInDatabase = async (demoFileName) => {
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
};

// Process demo file
const processDemoFile = async (demoFileName, fileSize, logData, jsonFileName) => {
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

    // Create an alliance mapping from alliance_history data
    const allianceMapping = new Map();

    if (logData.alliance_history) {
      // Process all alliance events to get final state
      logData.alliance_history.forEach(event => {
        if (event.action === 'JOIN') {
          allianceMapping.set(event.team, event.alliance);
        }
      });
    }

    // Update player objects with alliance info if not already present
    const processedPlayers = logData.players.map(player => {
      // If player already has alliance info, keep it
      if (player.alliance !== undefined) {
        return { ...player };
      }

      // If we have alliance info from alliance_history, use it
      if (allianceMapping.has(player.team)) {
        return {
          ...player,
          alliance: allianceMapping.get(player.team)
        };
      }

      // Fallback to using team as alliance
      return {
        ...player,
        alliance: player.team
      };
    });

    // Process spectators if they exist
    const processedSpectators = logData.spectators ? logData.spectators.map(spectator => ({
      name: spectator.name,
      version: spectator.version,
      platform: spectator.platform,
      key_id: spectator.key_id
    })) : [];

    // Combine players and spectators for JSON storage
    const fullGameData = {
      players: processedPlayers,
      spectators: processedSpectators
    };

    // Process player data for individual columns
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

    // Fill remaining player slots with null
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
      JSON.stringify(fullGameData), // Store both players and spectators in the players column
      ...Object.values(playerData),
      jsonFileName
    ];

    const placeholders = columns.map(() => '?').join(', ');
    const query = `INSERT INTO demos (${columns.join(', ')}) VALUES (${placeholders})`;

    const [result] = await pool.query(query, values);

    await pool.query('COMMIT');

    console.log(`Demo ${demoFileName} processed and added to database with player and spectator data`);
    console.log(`Inserted row ID: ${result.insertId}`);

    // Log the processed data for verification
    console.log('Processed game data:', JSON.stringify(fullGameData, null, 2));

    // Send to Discord after successful database insertion
    try {
      const [newDemo] = await pool.query('SELECT * FROM demos WHERE id = ?', [result.insertId]);
      if (newDemo.length > 0) {
        await sendDemoToDiscord(newDemo[0], logData);
        console.log(`Demo ${demoFileName} successfully posted to Discord`);
      }
    } catch (discordError) {
      console.error(`Error posting demo ${demoFileName} to Discord:`, discordError);
      // Don't throw here - we want to keep the demo even if Discord posting fails
    }

  } catch (error) {
    await pool.query('ROLLBACK');
    console.error(`Error processing demo ${demoFileName}:`, error);
    throw error;
  }
};

// Check for matching demo and JSON files
const checkForMatch = async () => {
  const processedPairs = new Set(); // Track processed demo-json pairs

  // First check all JSON files
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

      // Check if this pair has already been processed
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

          // Check if all players have zero score
          if (utils.allPlayersHaveZeroScore(logData)) {
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

          // Only process tournament or 1v1 games for leaderboard
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

  // Then check for demos looking for matching jsons with specific prefixes
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

            // Check if all players have zero score
            if (utils.allPlayersHaveZeroScore(logData)) {
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
};

// Update leaderboard
const updateLeaderboard = async (gameData) => {
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
      // Tournament leaderboard logic
      // ...implementation details...
    } else if (is1v1) {
      // 1v1 leaderboard logic
      // ...implementation details...
    }

    await pool.query('COMMIT');
  } catch (error) {
    await pool.query('ROLLBACK');
    console.error('Error updating leaderboard:', error);
  }
};

// Cleanup old pending files
const cleanupOldPendingFiles = () => {
  const oneHourAgo = Date.now() - 60 * 60 * 1000; // 1 hour in milliseconds

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
};

// Setup file watchers - updated paths
const setupFileWatchers = () => {
  console.log(`Watching demo directory: ${demoDir}`);
  const demoWatcher = chokidar.watch(`${demoDir}/*.{dcrec,d8crec,d10crec}`, {
    ignored: /(^|[\/\\])\../,
    persistent: true
  });

  console.log(`Watching log file directory: ${gameLogsDir}`);
  const jsonWatcher = chokidar.watch(`${gameLogsDir}/{game_*.json,game8p_*.json,game10p_*.json}`, {
    ignored: /(^|[\/\\])\../,
    persistent: true
  });

  demoWatcher
    .on('add', async (demoPath) => {
      console.log(`New demo detected: ${demoPath}`);
      const demoFileName = path.basename(demoPath);

      // checks if the dcrec already exists in the database to prevent duplicates
      const exists = await demoExistsInDatabase(demoFileName);
      if (exists) {
        console.log(`Demo ${demoFileName} already exists in the database. Skipping.`);
        return;
      }

      const demoStats = await fs.promises.stat(demoPath);
      pendingDemos.set(demoFileName, {
        stats: demoStats,
        path: demoPath,
        addedTime: Date.now()
      });
      console.log(`Demo ${demoFileName} added to pending list.`);

      // checks the json file for the record setting that matches the demo file name
      await checkForMatch();
    })
    .on('error', error => console.error(`Demo watcher error: ${error}`));

  jsonWatcher
    .on('add', async (jsonPath) => {
      console.log(`New JSON log file detected: ${jsonPath}`);

      // timeout since sometimes json files can be written to while being processed which can cause incomplete demo processing
      console.log("Waiting 10 seconds before processing the file...");
      await utils.delay(10000);

      const jsonFileName = path.basename(jsonPath);

      if (!processedJsons.has(jsonFileName)) {
        pendingJsons.set(jsonFileName, {
          path: jsonPath,
          addedTime: Date.now()
        });
        console.log(`JSON ${jsonFileName} added to pending list.`);

        await checkForMatch();
      } else {
        console.log(`JSON ${jsonFileName} has already been processed. Skipping.`);
      }
    })
    .on('error', error => console.error(`JSON watcher error: ${error}`));

  // Cleanup old pending files periodically
  setInterval(cleanupOldPendingFiles, 60 * 60 * 1000);
};

module.exports = {
  initializeDiscordBot,
  shutdownDiscordBot,
  initializeServer,
  setupFileWatchers,
  sendDemoToDiscord,
  updateLeaderboard,
  checkForMatch,
  processDemoFile,
  pendingDemos,
  pendingJsons,
  processedJsons
};