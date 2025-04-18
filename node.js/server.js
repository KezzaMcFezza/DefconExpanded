require('dotenv').config();

// first lets check if our env file was deleted
console.log("Environment variables loaded:", {
    DB_HOST: process.env.DB_HOST,
    DB_USER: process.env.DB_USER,
    DB_PASSWORD: process.env.DB_PASSWORD ? "********" + process.env.DB_PASSWORD.slice(-4) : "NOT SET",
    DB_NAME: process.env.DB_NAME,
    DB_PORT: process.env.DB_PORT,
    PORT: process.env.PORT,
    NODE_ENV: process.env.NODE_ENV,
    JWT_SECRET: process.env.JWT_SECRET ? "********" + process.env.JWT_SECRET.slice(-4) : "NOT SET",
    EMAIL_HOST: process.env.EMAIL_HOST,
    EMAIL_PORT: process.env.EMAIL_PORT,
    EMAIL_USER: process.env.EMAIL_USER,
    EMAIL_PASSWORD: process.env.EMAIL_PASSWORD ? "********" : "NOT SET",
    EMAIL_FROM: process.env.EMAIL_FROM,
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN ? "********" + process.env.DISCORD_BOT_TOKEN.slice(-4) : "NOT SET",
    DISCORD_CHANNEL_IDS: process.env.DISCORD_CHANNEL_IDS,
    BASE_URL: process.env.BASE_URL,
    ENV_FILE_LOADED: process.env.DB_PASSWORD ? "YES" : "NO"
});

// the deed
process.env.DB_PASSWORD ? console.log("Hell yea") : console.log("We are fucked");

const express = require('express');
const router = express.Router();
const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const { JSDOM } = require('jsdom');
const nodemailer = require('nodemailer');
const axios = require('axios');
const session = require('express-session');
const timeout = require('connect-timeout');
const startTime = Date.now();
const crypto = require('crypto');
const app = express();
const port = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET;
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const logStream = fs.createWriteStream(path.join(__dirname, 'server.log'), { flags: 'a' });
const bcrypt = require('bcrypt');
const { Client, GatewayIntentBits, EmbedBuilder } = require('discord.js');
const pendingDemos = new Map();
const pendingJsons = new Map();
const processedJsons = new Set();
const pendingVerifications = new Map();
const rootDir = path.join(__dirname, '..');
const publicDir = path.join(rootDir, 'public');
const demoDir = path.join(rootDir, 'demo_recordings');
const resourcesDir = path.join(publicDir, 'Files');
const dedconBuildsDir = path.join(publicDir, 'Files');
const uploadDir = publicDir;
const gameLogsDir = path.join(rootDir, 'game_logs');
const modlistDir = path.join(publicDir, 'modlist');
const modPreviewsDir = path.join(publicDir, 'modpreviews');

const {
    pool,
    adminPages,
} = require('./constants');

const {
    authenticateToken,
    checkAuthToken,
    checkRole
}   = require('./authentication')

// api modules to import
const adminPanelRoutes = require('./apis/admin/admin-panel');
const blacklistRoutes = require('./apis/admin/blacklist');
const dedconBuildsRoutes = require('./apis/admin/dedcon-builds');
const dedconConfigRoutes = require('./apis/admin/dedcon-config');
const demoAdminRoutes = require('./apis/admin/demo');
const modlistAdminRoutes = require('./apis/admin/modlist');
const reportingAdminRoutes = require('./apis/admin/reporting');
const resourcesAdminRoutes = require('./apis/admin/resources');
const usersAdminRoutes = require('./apis/admin/users');
const demosRoutes = require('./apis/demos/demos');
const demoFiltersRoutes = require('./apis/demos/filters');
const demoReportingRoutes = require('./apis/demos/reporting');
const discordRoutes = require('./apis/discord/discord');
const graphRoutes = require('./apis/graph/graph');
const leaderboardRoutes = require('./apis/leaderboard/leaderboard');
const loginRoutes = require('./apis/login-signup/login');
const logoutRoutes = require('./apis/login-signup/logout');
const resetPasswordRoutes = require('./apis/login-signup/reset-password');
const signupRoutes = require('./apis/login-signup/signup');
const modlistFiltersRoutes = require('./apis/modlist/filters');
const likesFavouritesRoutes = require('./apis/modlist/likes-favourites');
const modRoutes = require('./apis/modlist/mod');
const modReportingRoutes = require('./apis/modlist/reporting');
const profileRoutes = require('./apis/profile/profile');
const dedconRoutes = require('./apis/resources/dedcon');
const resourcesRoutes = require('./apis/resources/resources');
const requestRoutes = require('./apis/users/request');

// error handler for everything past this point
const errorHandler = (err, req, res, next) => {
    console.error('Unhandled error:', err);
    res.status(500).json({ error: 'Internal server error', details: err.message });
};

app.use(cookieParser());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use('/uploads', express.static(path.join(publicDir, 'uploads')));
app.use(session({
    secret: JWT_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { secure: process.env.NODE_ENV === 'production' }
}));

app.use((req, res, next) => {
    if (req.cookies.token) {
        try {
            const decoded = jwt.verify(req.cookies.token, JWT_SECRET);
            req.user = decoded;
            if (!req.session.loginTime) {
                req.session.loginTime = Date.now();
            }
        } catch (err) {
            res.clearCookie('token');
        }
    } else {
    }
    next();
});

// register the api modules for use
app.use(adminPanelRoutes);
app.use(blacklistRoutes);
app.use(dedconBuildsRoutes);
app.use(dedconConfigRoutes);
app.use(demoAdminRoutes);
app.use(modlistAdminRoutes);
app.use(reportingAdminRoutes);
app.use(resourcesAdminRoutes);
app.use(usersAdminRoutes);
app.use(demosRoutes);
app.use(demoFiltersRoutes);
app.use(demoReportingRoutes);
app.use(discordRoutes);
app.use(graphRoutes);
app.use(leaderboardRoutes);
app.use(loginRoutes);
app.use(logoutRoutes);
app.use(resetPasswordRoutes);
app.use(signupRoutes);
app.use(modlistFiltersRoutes);
app.use(likesFavouritesRoutes);
app.use(modRoutes);
app.use(modReportingRoutes);
app.use(profileRoutes);
app.use(dedconRoutes);
app.use(resourcesRoutes);
app.use(requestRoutes);


// i forgot what this does, most likely setting public as the static directory for multer and html pages to be served
app.use(express.static(publicDir, {
    setHeaders: (res, path, stat) => {
        if (path.endsWith('.html') && adminPages.includes(path.split('/').pop())) {
            res.set('Content-Type', 'text/plain');
        }
    }
}));

// site manifest for google console
app.use((req, res, next) => {
    if (req.url.endsWith('.webmanifest')) {
        res.setHeader('Content-Type', 'application/manifest+json');
    }
    next();
});

// https enforcement
app.use((req, res, next) => {
    if (req.headers['x-forwarded-proto'] === 'https') {
        res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
    }
    next();
});


// should fix the mod download issues!
app.use(timeout('1h'));

app.use((req, res, next) => {
    if (!req.timedout) next();
});

// claude made me a search algorithm and to be honest it makes my balls jingle looking at it
const levenshteinDistance = (a, b) => {
    if (a.length === 0) return b.length;
    if (b.length === 0) return a.length;

    const matrix = [];

    for (let i = 0; i <= b.length; i++) {
        matrix[i] = [i];
    }

    for (let j = 0; j <= a.length; j++) {
        matrix[0][j] = j;
    }

    for (let i = 1; i <= b.length; i++) {
        for (let j = 1; j <= a.length; j++) {
            if (b.charAt(i - 1) === a.charAt(j - 1)) {
                matrix[i][j] = matrix[i - 1][j - 1];
            } else {
                matrix[i][j] = Math.min(
                    matrix[i - 1][j - 1] + 1,
                    Math.min(
                        matrix[i][j - 1] + 1,
                        matrix[i - 1][j] + 1
                    )
                );
            }
        }
    }

    return matrix[b.length][a.length];
};

// search algorithm for fetching data from the database
const fuzzyMatch = (needle, haystack, threshold = 0.3) => {
    const needleLower = needle.toLowerCase();
    const haystackLower = haystack.toLowerCase();

    if (haystackLower.includes(needleLower)) return true;

    const distance = levenshteinDistance(needleLower, haystackLower);
    const maxLength = Math.max(needleLower.length, haystackLower.length);
    return distance / maxLength <= threshold;
};

// function to check if we are on an admin page.
function serveAdminPage(pageName, minRole) {
    return (req, res) => {
        console.log(`Accessing /${pageName}. User:`, JSON.stringify(req.user, null, 2));
        fs.readFile(path.join(publicDir, `${pageName}.html`), 'utf8', (err, data) => {
            if (err) {
                console.error('Error reading file:', err);
                return res.status(500).send('Error loading page');
            }
            const modifiedHtml = data.replace('</head>', `<script>window.userRole = ${req.user.role};</script></head>`);
            res.send(modifiedHtml);
        });
    };
}

// general function for dates on the modlist and games
function formatTimestamp(date) {
    const pad = (num) => (num < 10 ? '0' + num : num);

    const year = date.getFullYear();
    const month = pad(date.getMonth() + 1);
    const day = pad(date.getDate());
    const hours = pad(date.getHours());
    const minutes = pad(date.getMinutes());
    const seconds = pad(date.getSeconds());
    const milliseconds = pad(Math.floor(date.getMilliseconds() / 10));

    return `${year}-${month}-${day}-${hours}:${minutes}:${seconds}.${milliseconds}`;
}

// this is meant to filter games with 0 score from being uploaded
function allPlayersHaveZeroScore(logData) {
    if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
        return false;
    }

    return logData.players.every(player => player.score === 0);
}

async function initializeServer() {
    console.log("Starting server initialization...");

    // creates a promise that resolves when everything is ready
    const initializationPromise = new Promise((resolve, reject) => {
        if (discordBotReady) {
            resolve();
        } else {
            console.log('Waiting for Discord bot to be ready...');
            pendingInitialization = true;
            completePendingInitialization = resolve;
        }
    });

    // wait for Discord bot to be ready
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

}

// oldest part of the code is here
console.log(`Watching demo directory: ${demoDir}`);
const demoWatcher = chokidar.watch(`${demoDir}/*.{dcrec,d8crec,d10crec}`, {
    ignored: /(^|[\/\\])\../,
    persistent: true
});

// here too
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
        await delay(10000);

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

async function checkForMatch() {
    const processedPairs = new Set(); // Track processed demo-json pairs

    // Helper function to check for zero scores
    function allPlayersHaveZeroScore(logData) {
        if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
            return false;
        }
        return logData.players.every(player => player.score === 0);
    }

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

// if i have deleted a demo from the database the website will remove the game from the pending list until i manually delte it
function cleanupOldPendingFiles() {
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
}

setInterval(cleanupOldPendingFiles, 60 * 60 * 1000);


// checks if a demo already exists in the database, if not proceed to processing
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
}

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// 404 handler
function sendHtml(res, fileName) {
    res.sendFile(path.join(publicDir, fileName), (err) => {
        if (err) {
            res.status(404).sendFile(path.join(publicDir, '404.html'));
        }
    });
}

// CORS middleware for all routes
app.use((req, res, next) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
    res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    next();
});

// my comfort function after the website used to randomly hang for no reason
pool.getConnection((err, connection) => {
    if (err) {
        console.error('Error connecting to the database:', err);
    } else {
        console.log('Successfully connected to the database');
        connection.release();
    }
});

// pretty sure claude added this since it looks the same as the other error handler
app.use((err, req, res, next) => {
    console.error('Error details:', err);
    res.status(500).json({ error: 'Internal server error', details: err.message });
});



// http headers 0__0
app.use((err, req, res, next) => {
    console.error('Unhandled error:', err);
    console.error('Stack trace:', err.stack);
    console.error('Request details:', {
        method: req.method,
        url: req.url,
        headers: req.headers,
        body: req.body,
        query: req.query,
        params: req.params
    });
    res.status(500).json({ error: 'Internal server error', details: err.message });
});

// interval to prevent pending verification tokens from forever being alone
setInterval(() => {
    const oneDayAgo = Date.now() - 24 * 60 * 60 * 1000;

    for (let [email, { verificationToken }] of pendingVerifications) {
        try {
            const decoded = jwt.verify(verificationToken, JWT_SECRET);
        } catch (error) {
            pendingVerifications.delete(email);
        }
    }
}, 3600000); // cleans up the map every hour


module.exports = router;

// routes for each page in the frontend
app.get('/about', checkAuthToken, (req, res) => sendHtml(res, 'about.html'));
app.get('/about/combined-servers', checkAuthToken, (req, res) => sendHtml(res, 'combinedservers.html'));
app.get('/about/hours-played', checkAuthToken, (req, res) => sendHtml(res, 'totalhoursgraph.html'));
app.get('/about/popular-territories', checkAuthToken, (req, res) => sendHtml(res, 'popularterritories.html'));
app.get('/about/1v1-setup-statistics', checkAuthToken, (req, res) => sendHtml(res, '1v1setupstatistics.html'));
app.get('/guides', checkAuthToken, (req, res) => sendHtml(res, 'guides.html'));
app.get('/resources', checkAuthToken, (req, res) => sendHtml(res, 'resources.html'));
app.get('/laikasdefcon', checkAuthToken, (req, res) => sendHtml(res, 'laikasdefcon.html'));
app.get('/homepage/matchroom', checkAuthToken, (req, res) => sendHtml(res, 'matchroom.html'));
app.get('/homepage', checkAuthToken, (req, res) => sendHtml(res, 'index.html'));
app.get('/dedcon-builds', checkAuthToken, (req, res) => sendHtml(res, 'dedconbuilds.html'));
app.get('/patchnotes', checkAuthToken, (req, res) => sendHtml(res, 'patchnotes.html'));
app.get('/issue-report', checkAuthToken, (req, res) => sendHtml(res, 'bugreport.html'));
app.get('/phpmyadmin', checkAuthToken, (req, res) => sendHtml(res, 'idiot.html')); // for the memes
app.get('/leaderboard', checkAuthToken, (req, res) => sendHtml(res, 'leaderboard.html'));
app.get('/modlist', checkAuthToken, (req, res) => sendHtml(res, 'modlist.html'));
app.get('/privacypolicy', checkAuthToken, (req, res) => sendHtml(res, 'privacypolicy.html'));
app.get('/modlist/maps', checkAuthToken, (req, res) => sendHtml(res, 'mapmods.html'));
app.get('/modlist/graphics', checkAuthToken, (req, res) => sendHtml(res, 'graphicmods.html'));
app.get('/modlist/overhauls', checkAuthToken, (req, res) => sendHtml(res, 'overhaulmods.html'));
app.get('/modlist/moddingtools', checkAuthToken, (req, res) => sendHtml(res, 'moddingtools.html'));
app.get('/modlist/ai', checkAuthToken, (req, res) => sendHtml(res, 'aimods.html'));
app.get('/signup', checkAuthToken, (req, res) => sendHtml(res, 'signuppage.html'));
app.get('/forgotpassword', checkAuthToken, (req, res) => sendHtml(res, 'forgotpasswordfor816788487.html'));
app.get('/changepassword', checkAuthToken, (req, res) => sendHtml(res, 'changepassword248723424.html'));
app.get('/adminpanel', authenticateToken, checkRole(5), serveAdminPage('admin-panel', 5));
app.get('/leaderboardblacklistmanage', authenticateToken, checkRole(5), serveAdminPage('blacklist', 5));
app.get('/demomanage', authenticateToken, checkRole(5), serveAdminPage('demo-manage', 5));
app.get('/playerlookup', authenticateToken, checkRole(2), serveAdminPage('playerlookup', 2));
app.get('/defconservers', authenticateToken, checkRole(1), serveAdminPage('servermanagment', 1));
app.get('/accountmanage', authenticateToken, checkRole(2), serveAdminPage('account-manage', 2));
app.get('/modlistmanage', authenticateToken, checkRole(5), serveAdminPage('modmanagment', 5));
app.get('/dedconmanagment', authenticateToken, checkRole(2), serveAdminPage('dedconmanagment', 2));
app.get('/resourcemanage', authenticateToken, checkRole(3), serveAdminPage('resourcemanagment', 3));
app.get('/serverconsole', authenticateToken, checkRole(1), serveAdminPage('server-console', 2));


// special routes to be handled
app.get('/', (req, res) => {
  res.sendFile(path.join(publicDir,  'index.html'));
});

// route for demos page numbers
app.get('/demos/page/:pageNumber', checkAuthToken, (req, res) => {
  res.sendFile(path.join(publicDir,  'index.html'));
});

app.get('/account-settings', authenticateToken, (req, res) => {
  res.sendFile(path.join(publicDir,  'profilesettings.html'));
});

app.get('/change-password', (req, res) => {
  res.sendFile(path.join(publicDir,  'profilesettings.html'));
});

app.get('/sitemap', (req, res) => {
  res.sendFile(path.join(publicDir,  'sitemap.xml'));
});

app.get('/site.webmanifest', (req, res) => {
  res.sendFile(path.join(publicDir,  'site.webmanifest'));
});

app.get('/browserconfig.xml', (req, res) => {
  res.sendFile(path.join(publicDir,  'browserconfig.xml'));
});

app.get('/login', (req, res) => {
  res.sendFile(path.join(publicDir, 'loginpage.html'));
});

app.get('/profile/:username', (req, res) => {
  res.sendFile(path.join(publicDir,  'profile.html'));
});

// 404 route handler
app.get('*', (req, res) => {
  const requestedPath = path.join(publicDir,  req.path);
  if (fs.existsSync(requestedPath) && fs.statSync(requestedPath).isFile()) {
    res.sendFile(requestedPath);
  } else {
    res.status(404).sendFile(path.join(publicDir,  '404.html'));
  }
});

app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).send('Something broke!');
});

// route for the server console output
io.on('connection', (socket) => {
  console.log('A user connected to console output');
});

const originalLog = console.log;
console.log = function () {
  const timestamp = formatTimestamp(new Date());
  const args = Array.from(arguments);
  const message = `[${timestamp}] ${args.join(' ')}`;

  logStream.write(message + '\n');

  io.emit('console_output', message);

  originalLog.apply(console, [timestamp, ...args]);
};

//Shut down properly so she dont shit herself
process.on('SIGINT', async () => {
  console.log('Shutting down...');
  discordBotReady = false;
  pendingInitialization = null;
  completePendingInitialization = null;

  logStream.end(() => {
    console.log('Log file stream closed.');
    process.exit(0);
  });
});

app.use(errorHandler);

// node.js server proxy
http.listen(port, async () => {
  console.log(`Defcon Expanded Demo and File Server Listening at http://localhost:${port}`);
  console.log(`Server started at: ${new Date().toISOString()}`);
  console.log(`Environment: ${process.env.NODE_ENV || 'development'}`);
  console.log(`Node.js version: ${process.version}`);
  console.log(`Platform: ${process.platform}`);
  try {
    await initializeServer();
    console.log("Server initialization completed successfully.");
  } catch (error) {
    console.error("Error during server initialization:", error);
  }
});