const express = require('express');
const router = express.Router();
const mysql = require('mysql2/promise');
const queryTimeout = 5000; 
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const { JSDOM } = require('jsdom');
const multer = require('multer');
const nodemailer = require('nodemailer');
const axios = require('axios');
const session = require('express-session');
const timeout = require('connect-timeout');
const startTime = Date.now();
const crypto = require('crypto');
const app = express();
const port = 3000;
const JWT_SECRET = 'a8001708554bfcb4bf707fece608fcce49c3ce0d88f5126b137d82b3c7aeab65';
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const logStream = fs.createWriteStream(path.join(__dirname, 'server.log'), { flags: 'a' });
const bcrypt = require('bcrypt');
const pendingDemos = new Map();
const pendingJsons = new Map();
const processedJsons = new Set();
const pendingVerifications = new Map();

const pool = mysql.createPool({
  host: 'localhost',
  user: 'root',
  password: 'cca3d38e2b',
  database: 'defcon_demos',
  connectionLimit: 20,
  queueLimit: 0,
  waitForConnections: true,
  enableKeepAlive: true,
  keepAliveInitialDelay: 10000,
  connectTimeout: 10000,
});

  // Create a transporter using SMTP
  let transporter = nodemailer.createTransport({
    host: "smtp.gmail.com",
    port: 587,
    secure: false, // Use TLS
    auth: {
      user: "keiron.mcphee1@gmail.com",
      pass: "ueiz tkqy uqwj lwht"
    }
  });

app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(router);

const demoDir = path.join(__dirname, 'demo_recordings');
const resourcesDir = path.join(__dirname, 'public', 'Files');
const uploadDir = path.join(__dirname, 'public');
const gameLogsDir = path.join(__dirname, 'game_logs');
const modlistDir = path.join(__dirname, 'public', 'modlist');
const modPreviewsDir = path.join(__dirname, 'public', 'modpreviews');

const errorHandler = (err, req, res, next) => {
  console.error('Unhandled error:', err);
  res.status(500).json({ error: 'Internal server error', details: err.message });
};

// Ensure directories exist
[demoDir, resourcesDir, uploadDir, gameLogsDir, modlistDir, modPreviewsDir].forEach(dir => {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
});

// Configure multer for handling both demo and JSON file uploads
const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    if (file.fieldname === 'demoFile') {
      cb(null, demoDir);
    } else if (file.fieldname === 'jsonFile') {
      cb(null, gameLogsDir);
    } else if (file.fieldname === 'resourceFile') {
      cb(null, resourcesDir);
    } else if (file.fieldname === 'modFile') {
      cb(null, modlistDir);
    } else if (file.fieldname === 'previewImage') {
      cb(null, modPreviewsDir);
    } else {
      cb(new Error('Invalid file type'));
    }
  },
  filename: function (req, file, cb) {
    cb(null, file.originalname);
  }
});

const adminPages = [
  'admin-panel.html',
  'blacklist.html',
  'demo-manage.html',
  'leaderboard-manage.html',
  'account-manage.html',
  'modmanagment.html',
  'resourcemanagment.html',
  'server-console.html'
];

// Add this middleware before your route definitions
app.use((req, res, next) => {
  const requestedFile = path.basename(req.url);
  if (adminPages.includes(requestedFile)) {
    return res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
  }
  next();
});

const checkRole = (requiredRole) => {
  return (req, res, next) => {
    if (!req.user) {
      return res.status(401).json({ error: 'Not authenticated' });
    }
    if (req.user.role <= requiredRole) {
      next();
    } else {
      return res.status(403).json({ error: 'Insufficient permissions' });
    }
  };
};

app.use(express.static('public', {
  setHeaders: (res, path, stat) => {
    if (path.endsWith('.html') && adminPages.includes(path.split('/').pop())) {
      res.set('Content-Type', 'text/plain');
    }
  }
}));

app.use(cookieParser());
app.use(session({
  secret: 'a8001708554bfcb4bf707fece608fcce49c3ce0d88f5126b137d82b3c7aeab65',
  resave: false,
  saveUninitialized: true,
  cookie: { secure: process.env.NODE_ENV === 'true' }
}));

// Set the correct MIME type for .webmanifest files
app.use((req, res, next) => {
  if (req.url.endsWith('.webmanifest')) {
    res.setHeader('Content-Type', 'application/manifest+json');
  }
  next();
});

app.use((req, res, next) => {
  if (req.headers['x-forwarded-proto'] === 'https') {
    res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
  }
  next();
});

app.use((req, res, next) => {
  if (req.cookies.token) {
    try {
      const decoded = jwt.verify(req.cookies.token, JWT_SECRET);
      req.user = decoded;
      if (!req.session.loginTime) {
        req.session.loginTime = Date.now();
      }
    } catch (err) {
      // Invalid token, clear it
      res.clearCookie('token');
    }
  }
  next();
});

app.use(timeout('120s')); // Set a 30-second timeout for all requests

app.use((req, res, next) => {
  if (!req.timedout) next();
});

// Example usage in a route
app.get('/api/data', async (req, res, next) => {
  try {
    const results = await executeQuery('SELECT * FROM your_table WHERE id = ?', [req.query.id]);
    res.json(results);
  } catch (error) {
    next(error); // Pass the error to the error handling middleware
  }
});

const levenshteinDistance = (a, b) => {
  if (a.length === 0) return b.length;
  if (b.length === 0) return a.length;

  const matrix = [];

  // Increment along the first column of each row
  for (let i = 0; i <= b.length; i++) {
    matrix[i] = [i];
  }

  // Increment each column in the first row
  for (let j = 0; j <= a.length; j++) {
    matrix[0][j] = j;
  }

  // Fill in the rest of the matrix
  for (let i = 1; i <= b.length; i++) {
    for (let j = 1; j <= a.length; j++) {
      if (b.charAt(i - 1) === a.charAt(j - 1)) {
        matrix[i][j] = matrix[i - 1][j - 1];
      } else {
        matrix[i][j] = Math.min(
          matrix[i - 1][j - 1] + 1, // substitution
          Math.min(
            matrix[i][j - 1] + 1, // insertion
            matrix[i - 1][j] + 1 // deletion
          )
        );
      }
    }
  }

  return matrix[b.length][a.length];
};

const fuzzyMatch = (needle, haystack, threshold = 0.3) => {
  const needleLower = needle.toLowerCase();
  const haystackLower = haystack.toLowerCase();
  
  if (haystackLower.includes(needleLower)) return true;
  
  const distance = levenshteinDistance(needleLower, haystackLower);
  const maxLength = Math.max(needleLower.length, haystackLower.length);
  return distance / maxLength <= threshold;
};

function serveAdminPage(pageName, minRole) {
  return (req, res) => {
      console.log(`Accessing /${pageName}. User:`, JSON.stringify(req.user, null, 2));
      fs.readFile(path.join(__dirname, 'public', `${pageName}.html`), 'utf8', (err, data) => {
          if (err) {
              console.error('Error reading file:', err);
              return res.status(500).send('Error loading page');
          }
          const modifiedHtml = data.replace('</head>', `<script>window.userRole = ${req.user.role};</script></head>`);
          res.send(modifiedHtml);
      });
  };
}

async function executeQuery(sql, params = []) {
  let connection;
  try {
    connection = await pool.getConnection();
    const [results] = await connection.query(sql, params);
    return results;
  } catch (error) {
    console.error('Database query error:', error);
    throw error; // Re-throw for handling in the route
  } finally {
    if (connection) connection.release();
  }
}

app.get('/api/data', async (req, res) => {
  try {
    const results = await executeQuery('SELECT * FROM your_table WHERE id = ?', [req.query.id]);
    res.json(results);
  } catch (error) {
    console.error('Error in /api/data route:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

const upload = multer({ storage: storage });

function formatTimestamp(date) {
  const pad = (num) => (num < 10 ? '0' + num : num);
  
  const year = date.getFullYear();
  const month = pad(date.getMonth() + 1);
  const day = pad(date.getDate());
  const hours = pad(date.getHours());
  const minutes = pad(date.getMinutes());
  const seconds = pad(date.getSeconds());
  const milliseconds = pad(Math.floor(date.getMilliseconds() / 10)); // Get only the first two digits

  return `${year}-${month}-${day}-${hours}:${minutes}:${seconds}.${milliseconds}`;
}

async function initializeServer() {
  console.log("Initializing server...");
  
  // Clear pending lists and processed JSONs
  pendingDemos.clear();
  pendingJsons.clear();
  processedJsons.clear();

  // Read existing demo files
  const demoFiles = await fs.promises.readdir(demoDir);
  console.log(`Found ${demoFiles.length} demo files in the directory.`);

  // Read existing JSON files
  const jsonFiles = await fs.promises.readdir(gameLogsDir);
  console.log(`Found ${jsonFiles.length} JSON files in the directory.`);

  // Get existing demos from database
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

  // Check for matches
  await checkForMatch();
}

// Watch for new demo files
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
    
    // Check if the demo already exists in the database
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
    
    // Check for matching JSON file
    await checkForMatch();
  })
  .on('error', error => console.error(`Demo watcher error: ${error}`));

  jsonWatcher
  .on('add', async (jsonPath) => {
    console.log(`New JSON log file detected: ${jsonPath}`);
    
    // Wait for 10 seconds
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
  // First, try to match JSON files to existing demo files
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

      if (demoInfo) {
        console.log(`Matching files found: ${dcrecFileName} and ${jsonFileName}`);
        await processDemoFile(dcrecFileName, demoInfo.stats.size, logData, jsonFileName);
        
        console.log(`Successfully processed and linked ${dcrecFileName} with ${jsonFileName}`);
    
        pendingDemos.delete(dcrecFileName);
        pendingJsons.delete(jsonFileName);
        processedJsons.add(jsonFileName);

        // Only update leaderboard after successful processing
        if (logData.gameType && logData.gameType.toLowerCase().includes('1v1')) {
          await updateLeaderboard(logData);
        }
      } else {
        console.log(`Corresponding .dcrec, .d8crec, or .d10crec file not found for JSON file: ${jsonFileName}`);
      }
    } catch (error) {
      console.error(`Error processing JSON file ${jsonFileName}:`, error);
    }
  }

  // Then, try to match demo files to existing JSON files
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
      try {
        const jsonContent = await fs.promises.readFile(jsonInfo.path, 'utf8');
        const logData = JSON.parse(jsonContent);

        const recordSetting = logData.settings && logData.settings.Record;
        if (recordSetting && path.basename(recordSetting) === demoFileName) {
          console.log(`Matching files found: ${demoFileName} and ${jsonFileName}`);
          await processDemoFile(demoFileName, demoInfo.stats.size, logData, jsonFileName);
          
          console.log(`Successfully processed and linked ${demoFileName} with ${jsonFileName}`);

          pendingDemos.delete(demoFileName);
          pendingJsons.delete(jsonFileName);
          processedJsons.add(jsonFileName);
          break;
        }
      } catch (error) {
        console.error(`Error processing JSON file ${jsonFileName}:`, error);
      }
    }
  }
}
  
// Function to periodically clean up old pending files
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

// Run cleanup every hour
setInterval(cleanupOldPendingFiles, 60 * 60 * 1000);

async function updateLeaderboard(gameData) {
  if (gameData.gameType.includes('1V1')) {
    const players = gameData.players;
    if (players.length !== 2) return; // Ensure it's a 1v1 game

    // Fetch the whitelist
    const [whitelist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const whitelistedPlayers = new Set(whitelist.map(entry => entry.player_name.toLowerCase()));

    // Determine the winner and loser
    const winner = players.reduce((a, b) => a.score > b.score ? a : b);
    const loser = players.find(p => p !== winner);

    for (const player of players) {
      // Check if the player is whitelisted
      if (whitelistedPlayers.has(player.name.toLowerCase())) {
        console.log(`Player ${player.name} is whitelisted. Skipping leaderboard update.`);
        continue; // Skip this player
      }

      const isWinner = player === winner;
      const scoreToAdd = player.score > 0 ? player.score : 0; // Only add positive scores

      try {
        // Check if the player already exists in the leaderboard by name
        const [existingPlayerByName] = await pool.query('SELECT * FROM leaderboard WHERE player_name = ?', [player.name]);

        // Check if the player exists in the leaderboard by key_id (only for non-DEMO players)
        let existingPlayerByKeyId = [];
        if (player.key_id !== 'DEMO') {
          [existingPlayerByKeyId] = await pool.query('SELECT * FROM leaderboard WHERE key_id = ?', [player.key_id]);
        }

        if (existingPlayerByName.length > 0) {
          // Player exists with the same name
          if (player.key_id === 'DEMO') {
            if (existingPlayerByName[0].key_id !== 'DEMO') {
              console.log(`Looks like ${player.name} has lost their authentication key :( Adding player data to the leaderboard assuming they will find their authentication key at some point.`);
            } else {
              console.log(`Demo player detected: ${player.name}. Ignoring key ID matching and using player name instead.`);
            }
          } else if (existingPlayerByName[0].key_id === 'DEMO' && player.key_id !== 'DEMO') {
            console.log(`Demo player ${player.name} has now bought the game! Replacing DEMO key ID with ${player.key_id}`);
            await pool.query('UPDATE leaderboard SET key_id = ? WHERE player_name = ?', [player.key_id, player.name]);
          } else if (existingPlayerByName[0].key_id !== player.key_id) {
            console.log(`${player.name} appears to have a new key ID. Replacing existing key ID ${existingPlayerByName[0].key_id} with new key ID ${player.key_id} in the database.`);
            await pool.query('UPDATE leaderboard SET key_id = ? WHERE player_name = ?', [player.key_id, player.name]);
          }
          
          // Update player stats
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
        } else if (existingPlayerByKeyId.length > 0) {
          // Player exists with the same key_id (this will only happen for non-DEMO players)
          const existingName = existingPlayerByKeyId[0].player_name;
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
        } else {
          // New player, insert into leaderboard
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

        console.log(`Leaderboard update completed for player ${player.name} (Key ID: ${player.key_id})`);
      } catch (error) {
        console.error(`Error updating leaderboard for player ${player.name}:`, error);
      }
    }
  }
}

async function demoExistsInDatabase(demoFileName) {
  try {
    const [rows] = await pool.query('SELECT COUNT(*) as count FROM demos WHERE name = ?', [demoFileName]);
    return rows[0].count > 0;
  } catch (error) {
    console.error(`Error checking if demo exists in database: ${error}`);
    return false;
  }
}

async function processDemoFile(demoFileName, fileSize, logData, jsonFileName) {
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

    // Filter and process players
    const playerData = {};
    const playerColumns = ['player1', 'player2', 'player3', 'player4', 'player5', 'player6', 'player7', 'player8', 'player9', 'player10'];
    logData.players.forEach((player, index) => {
      if (index < 10) {
        const prefix = playerColumns[index];
        playerData[`${prefix}_name`] = player.name || null;
        playerData[`${prefix}_team`] = player.team !== undefined ? player.team : null;
        playerData[`${prefix}_score`] = player.score !== undefined ? player.score : null;
        playerData[`${prefix}_territory`] = player.territory || null;
        playerData[`${prefix}_key_id`] = player.key_id || null;
      }
    });

    // Fill remaining player slots with null values
    for (let i = playerCount; i < 10; i++) {
      const prefix = playerColumns[i];
      playerData[`${prefix}_name`] = null;
      playerData[`${prefix}_team`] = null;
      playerData[`${prefix}_score`] = null;
      playerData[`${prefix}_territory`] = null;
      playerData[`${prefix}_key_id`] = null;
    }

    const columns = [
      'name', 'size', 'date', 'game_type', 'duration', 'players', 
      ...Object.keys(playerData), 
      'log_file'
    ];

    const values = [
      demoFileName, 
      fileSize, 
      new Date(logData.start_time), 
      gameType, 
      duration, 
      JSON.stringify(logData.players), 
      ...Object.values(playerData),
      jsonFileName
    ];

    const placeholders = columns.map(() => '?').join(', ');
    const query = `INSERT INTO demos (${columns.join(', ')}) VALUES (${placeholders})`;

    const [result] = await pool.query(query, values);

    await pool.query('COMMIT');

    console.log(`Demo ${demoFileName} processed and added to database with player data`);
    console.log(`Inserted row ID: ${result.insertId}`);

  } catch (error) {
    await pool.query('ROLLBACK');
    console.error(`Error processing demo ${demoFileName}:`, error);
  }
}

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// Middleware to verify JWT token
const authenticateToken = async (req, res, next) => {
  const token = req.cookies.token;
  if (!token) {
    return res.status(401).json({ error: 'Not authenticated' });
  }

  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    
    // Fetch the current user information from the database
    const [users] = await pool.query('SELECT id, username, role FROM users WHERE id = ?', [decoded.id]);
    if (users.length === 0) {
      console.log('Authentication failed: User not found in database');
      return res.status(403).json({ error: 'Invalid token' });
    }
    
    const user = users[0];
    
    req.user = user;
    next();
  } catch (err) {
    console.log('Authentication failed: Invalid token', err);
    return res.status(403).json({ error: 'Invalid token' });
  }
};

// Middleware to check for the presence of a valid token in the cookie on each request
const checkAuthToken = (req, res, next) => {
  const token = req.cookies.token;
  if (token) {
    jwt.verify(token, JWT_SECRET, (err, user) => {
      if (err) {
        res.locals.user = null;
      } else {
        res.locals.user = { id: user.id, username: user.username };
      }
      next();
    });
  } else {
    res.locals.user = null;
    next();
  }
};

// Helper function to send HTML file
function sendHtml(res, fileName) {
  res.sendFile(path.join(__dirname, '..', 'public', fileName), (err) => {
    if (err) {
      res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
    }
  });
}

// Routes
app.get('/api/monitoring-data', authenticateToken, async (req, res) => {
  try {
    const uptime = Math.floor((Date.now() - startTime) / 1000);
    
    const [totalDemosResult] = await pool.query('SELECT COUNT(*) as count FROM demos');
    const totalDemos = totalDemosResult[0].count;

    const [pendingRequestsResult] = await pool.query(`
      SELECT 
        (SELECT COUNT(*) FROM demo_reports WHERE status = 'pending') +
        (SELECT COUNT(*) FROM mod_reports WHERE status = 'pending') as total_pending
    `);
    const userRequests = pendingRequestsResult[0].total_pending;
    
    res.json({
      uptime,
      totalDemos,
      userRequests,
    });
  } catch (error) {
    console.error('Error fetching monitoring data:', error);
    res.status(500).json({ error: 'Unable to fetch monitoring data' });
  }
});

//Gets the ip from the client who is logging in
function getClientIp(req) {
  const forwardedFor = req.headers['x-forwarded-for'];
  if (forwardedFor) {
    return forwardedFor.split(',')[0].trim();
  }
  return (req.ip || req.connection.remoteAddress).replace(/^::ffff:/, '');
}

// Check authentication status
app.get('/api/checkAuth', (req, res) => {
  const token = req.cookies.token;
  if (token) {
    jwt.verify(token, JWT_SECRET, (err, user) => {
      if (err) {
        res.json({ isLoggedIn: false });
      } else {
        res.json({ isLoggedIn: true, username: user.username });
      }
    });
  } else {
    res.json({ isLoggedIn: false });
  }
});

app.get('/download-logs', checkAuthToken, (req, res) => {
  const logPath = path.join(__dirname, 'server.log');
  res.download(logPath, 'server.log', (err) => {
    if (err) {
      console.error('Error downloading log file:', err);
      res.status(500).send('Error downloading log file');
    }
  });
});

app.get('/api/discord-widget', async (req, res) => {
  try {
    const discordResponse = await axios.get('https://discord.com/api/guilds/1256842522215579668/widget.json');
    res.json(discordResponse.data);
  } catch (error) {
    console.error('Error fetching Discord widget:', error);
    res.status(500).json({ error: 'Failed to fetch Discord widget data' });
  }
});

// Admin panel route
app.get('/admin-panel', authenticateToken, (req, res) => {
  if (req.user) {
      res.sendFile(path.join(__dirname, '..', 'public', 'admin-panel.html'));
  } else {
      res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
  }
});

app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  next();
});

// Whitelist management routes
app.get('/api/whitelist', authenticateToken, async (req, res) => {
  try {
      const [rows] = await pool.query('SELECT * FROM leaderboard_whitelist');
      res.json(rows);
  } catch (error) {
      console.error('Error fetching whitelist:', error);
      res.status(500).json({ error: 'Unable to fetch whitelist' });
  }
});

app.post('/api/whitelist', authenticateToken, checkRole(5), async (req, res) => {
  const { playerName, reason } = req.body;
  
  try {
    const [result] = await pool.query(
      'INSERT INTO leaderboard_whitelist (player_name, reason) VALUES (?, ?)',
      [playerName, reason]
    );
    console.log(`${req.user.username} added player to whitelist: ${JSON.stringify({ playerName, reason }, null, 2)}`);
    res.json({ message: 'Player added to whitelist', id: result.insertId });
  } catch (error) {
    console.error('Error adding player to whitelist:', error.message);
    res.status(500).json({ error: 'Unable to add player to whitelist', details: error.message });
  }
});

// Test database connection
pool.getConnection((err, connection) => {
  if (err) {
    console.error('Error connecting to the database:', err);
  } else {
    console.log('Successfully connected to the database');
    connection.release();
  }
});

// Enhanced error logging
app.use((err, req, res, next) => {
  console.error('Error details:', err);
  res.status(500).json({ error: 'Internal server error', details: err.message });
});

app.delete('/api/whitelist/:playerName', authenticateToken, checkRole(5), async (req, res) => {
  const { playerName } = req.params;
  try {
    await pool.query('DELETE FROM leaderboard_whitelist WHERE player_name = ?', [playerName]);
    console.log(`${req.user.username} removed player from whitelist: ${playerName}`);
    res.json({ message: 'Player removed from whitelist' });
  } catch (error) {
    console.error('Error removing player from whitelist:', error);
    res.status(500).json({ error: 'Unable to remove player from whitelist' });
  }
});

// Demo management routes
app.post('/api/upload-demo', authenticateToken, upload.fields([
  { name: 'demoFile', maxCount: 1 },
  { name: 'jsonFile', maxCount: 1 }
]), checkRole(4), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Demo upload by ${req.user.username} from IP ${clientIp}`);
  
  if (!req.files || !req.files.demoFile || !req.files.jsonFile) {
    console.log(`Failed demo upload attempt by ${req.user.username} from IP ${clientIp}: Missing required files`);
    return res.status(400).json({ error: 'Both demo and JSON files are required' });
  }

  const demoFile = req.files.demoFile[0];
  const jsonFile = req.files.jsonFile[0];

  console.log(`Received files for upload by ${req.user.username} from IP ${clientIp}:`, 
              JSON.stringify({ demo: demoFile.originalname, json: jsonFile.originalname }, null, 2));

  try {
    // Check if a demo with the same name already exists
    const [existingDemos] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFile.originalname]);
    if (existingDemos.length > 0) {
      console.log('Demo already exists:', demoFile.originalname);
      return res.status(400).json({ error: 'A demo with this name already exists' });
    }

    // Read and parse the JSON file
    const jsonContent = await fs.promises.readFile(jsonFile.path, 'utf8');
    const logData = JSON.parse(jsonContent);

    // Process the demo file
    await processDemoFile(demoFile.originalname, demoFile.size, logData, jsonFile.originalname);

    console.log(`Demo successfully uploaded and processed by ${req.user.username} from IP ${clientIp}: ${demoFile.originalname}`);
    res.json({ message: 'Demo uploaded and processed successfully' });
  } catch (error) {
    console.error(`Error processing uploaded demo by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to process uploaded demo', details: error.message });
  }
});

app.get('/api/demo/:demoId', authenticateToken, async (req, res) => {
  try {
      const [rows] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
      if (rows.length === 0) {
          res.status(404).json({ error: 'Demo not found' });
      } else {
          res.json(rows[0]);
      }
  } catch (error) {
      console.error('Error fetching demo details:', error);
      res.status(500).json({ error: 'Unable to fetch demo details' });
  }
});

app.put('/api/demo/:demoId', authenticateToken, checkRole(5), async (req, res) => {
  const { demoId } = req.params;
  const { name, game_type, duration, players } = req.body;

  try {
    console.log('Starting database transaction');
    await pool.query('START TRANSACTION');

    const [oldData] = await pool.query('SELECT * FROM demos WHERE id = ?', [demoId]);
    console.log('Executing update query');
    const [updateResult] = await pool.query(
      'UPDATE demos SET name = ?, game_type = ?, duration = ?, players = ? WHERE id = ?', 
      [name, game_type, duration, players, demoId]
    );

    console.log('Update result:', JSON.stringify(updateResult, null, 2));

    if (updateResult.affectedRows === 0) {
      console.log('No rows affected. Rolling back transaction.');
      await pool.query('ROLLBACK');
      return res.status(404).json({ error: 'Demo not found or no changes made' });
    }

    console.log('Committing transaction');
    await pool.query('COMMIT');
    console.log(`${req.user.username} updated demo:
      Demo ID: ${demoId}
      Old data: ${JSON.stringify(oldData[0], null, 2)}
      New data: ${JSON.stringify({ name, game_type, duration, players }, null, 2)}`);
    res.json({ message: 'Demo updated successfully' });
  } catch (error) {
    console.error('Error updating demo:', error.message);
    await pool.query('ROLLBACK');
    res.status(500).json({ error: 'Unable to update demo', details: error.message });
  }
});

app.get('/api/most-active-players', async (req, res) => {
  try {
      const [rows] = await pool.query(`
          SELECT player_name, games_played
          FROM leaderboard
          ORDER BY games_played DESC
          LIMIT 5
      `);
      res.json(rows);
  } catch (error) {
      console.error('Error fetching most active players:', error);
      res.status(500).json({ error: 'Unable to fetch most active players', details: error.message });
  }
});

app.get('/api/leaderboard', async (req, res) => {
  const { sortBy = 'wins' } = req.query;
  try {
    let orderBy;
    switch (sortBy) {
      case 'wins':
        orderBy = 'wins DESC, total_score DESC, player_name ASC';
        break;
      case 'gamesPlayed':
        orderBy = 'games_played DESC, wins DESC, total_score DESC, player_name ASC';
        break;
      case 'totalScore':
        orderBy = 'total_score DESC, wins DESC, player_name ASC';
        break;
      default:
        orderBy = 'wins DESC, total_score DESC, player_name ASC';
    }

    const query = `
      SELECT 
        l.*,
        r.absolute_rank
      FROM 
        leaderboard l
      JOIN (
        SELECT 
          id, 
          ROW_NUMBER() OVER (ORDER BY wins DESC, total_score DESC, player_name ASC) as absolute_rank
        FROM 
          leaderboard
      ) r ON l.id = r.id
      ORDER BY ${orderBy}
    `;

    const [rows] = await pool.query(query);
    res.json(rows);
  } catch (error) {
    console.error('Error fetching leaderboard:', error);
    res.status(500).json({ error: 'Unable to fetch leaderboard' });
  }
});

app.put('/api/leaderboard/:playerId', authenticateToken, checkRole(5), async (req, res) => {
  const { playerId } = req.params;
  const { player_name, games_played, wins, losses, total_score } = req.body;

  try {
    const [oldData] = await pool.query('SELECT * FROM leaderboard WHERE id = ?', [playerId]);
    const [result] = await pool.query('UPDATE leaderboard SET player_name = ?, games_played = ?, wins = ?, losses = ?, total_score = ? WHERE id = ?',
      [player_name, games_played, wins, losses, total_score, playerId]);
    
    if (result.affectedRows === 0) {
      return res.status(404).json({ error: 'Player not found' });
    }
    
    console.log(`${req.user.username} edited leaderboard entry:
      Player ID: ${playerId}
      Old data: ${JSON.stringify(oldData[0], null, 2)}
      New data: ${JSON.stringify({ player_name, games_played, wins, losses, total_score }, null, 2)}`);
    
    res.json({ message: 'Player data updated successfully' });
  } catch (error) {
    console.error('Error updating player data:', error.message);
    res.status(500).json({ error: 'Unable to update player data' });
  }
});

app.get('/api/leaderboard/:playerId', async (req, res) => {
  const { playerId } = req.params;

  try {
      const [rows] = await pool.query('SELECT * FROM leaderboard WHERE id = ?', [playerId]);
      if (rows.length === 0) {
          return res.status(404).json({ error: 'Player not found' });
      }
      res.json(rows[0]);
  } catch (error) {
      console.error('Error fetching player data:', error);
      res.status(500).json({ error: 'Unable to fetch player data' });
  }
});

// Remove player from leaderboard
app.delete('/api/leaderboard/:playerId', authenticateToken, checkRole(1), async (req, res) => {
  const { playerId } = req.params;

  try {
    const [playerData] = await pool.query('SELECT player_name FROM leaderboard WHERE id = ?', [playerId]);
    await pool.query('DELETE FROM leaderboard WHERE id = ?', [playerId]);
    console.log(`${req.user.username} removed player from leaderboard: ${playerData[0].player_name} (ID: ${playerId})`);
    res.json({ message: 'Player removed from leaderboard successfully' });
  } catch (error) {
    console.error('Error removing player from leaderboard:', error);
    res.status(500).json({ error: 'Unable to remove player from leaderboard' });
  }
});

// Add player to leaderboard
app.post('/api/leaderboard', authenticateToken, checkRole(5), async (req, res) => {
  const { player_name, games_played, wins, losses, total_score } = req.body;

  if (!player_name || games_played == null || wins == null || losses == null || total_score == null) {
    return res.status(400).json({ error: 'All fields are required' });
  }

  try {
    const [result] = await pool.query(
      'INSERT INTO leaderboard (player_name, games_played, wins, losses, total_score) VALUES (?, ?, ?, ?, ?)',
      [player_name, games_played, wins, losses, total_score]
    );
    console.log(`${req.user.username} added player to leaderboard:
      ${JSON.stringify({ player_name, games_played, wins, losses, total_score }, null, 2)}`);
    res.json({ message: 'Player added to leaderboard successfully', id: result.insertId });
  } catch (error) {
    console.error('Error adding player to leaderboard:', error.message);
    res.status(500).json({ error: 'Unable to add player to leaderboard' });
  }
});

// List all demos with associated users
app.get('/api/demos', async (req, res) => {
  const { playerName } = req.query;
  
  try {
    let query = 'SELECT * FROM demos';
    let params = [];

    if (playerName) {
      query += ` WHERE player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                 OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? OR player10_name LIKE ?`;
      params = Array(10).fill(`%${playerName}%`);
    }

    query += ' ORDER BY date DESC';

    const [demos] = await pool.query(query, params);
    res.json(demos);
  } catch (error) {
    console.error('Error fetching demos:', error);
    res.status(500).json({ error: 'Unable to fetch demos' });
  }
});

app.get('/api/search-players', async (req, res) => {
  const { playerName } = req.query;
  
  if (!playerName) {
    return res.status(400).json({ error: 'Player name is required' });
  }

  try {
    const query = `
      SELECT * FROM demos
      WHERE player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
      OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
      OR player9_name LIKE ? OR player10_name LIKE ? ORDER BY date DESC
    `;
    const searchPattern = `%${playerName}%`;
    const [demos] = await pool.query(query, Array(10).fill(searchPattern));

    res.json(demos);
  } catch (error) {
    console.error('Error searching for players:', error);
    res.status(500).json({ error: 'Unable to search for players' });
  }
});

// Download a demo
app.get('/api/download/:demoName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    if (rows.length === 0) {
      res.status(404).send('Demo not found');
    } else {
      const demoPath = path.join(demoDir, rows[0].name);
      res.download(demoPath, (err) => {
        if (err) {
          res.status(404).send('Demo file not found');
        }
      });
    }
  } catch (error) {
    console.error('Error downloading demo:', error);
    res.status(500).send('Error downloading demo');
  }
});

// Delete a demo (protected route)
app.delete('/api/demo/:demoId', authenticateToken, checkRole(1), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Demo deletion by ${req.user.username} from IP ${clientIp}`);
  
  try {
    const [demoData] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
    const [result] = await pool.query('DELETE FROM demos WHERE id = ?', [req.params.demoId]);
    if (result.affectedRows === 0) {
      console.log(`Failed demo deletion attempt by ${req.user.username} from IP ${clientIp}: Demo not found (ID: ${req.params.demoId})`);
      res.status(404).json({ error: 'Demo not found' });
    } else {
      console.log(`Demo successfully deleted by ${req.user.username} from IP ${clientIp}:`);
      console.log(JSON.stringify(demoData[0], null, 2));
      res.json({ message: 'Demo deleted successfully' });
    }
  } catch (error) {
    console.error(`Error deleting demo by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to delete demo' });
  }
});

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

// New routes for resources
app.get('/api/resources', async (req, res) => {
  try {
    const [resources] = await pool.query('SELECT * FROM resources ORDER BY date DESC');
    res.json(resources);
  } catch (error) {
    console.error('Error fetching resources:', error);
    res.status(500).json({ error: 'Unable to fetch resources' });
  }
});

app.post('/api/upload-resource', authenticateToken, upload.single('resourceFile'), checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Resource upload by ${req.user.username} from IP ${clientIp}`);

  if (!req.file) {
    console.log(`Failed resource upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
    return res.status(400).json({ error: 'No file uploaded' });
  }

  try {
    const originalName = req.file.originalname;
    const filePath = path.join(resourcesDir, originalName);
    const { version, releaseDate, platform, playerCount } = req.body;

    if (!version || !platform || !playerCount) {
      console.log(`Failed resource upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
      return res.status(400).json({ error: 'Version, platform, and player count are required' });
    }

    console.log(`Processing resource upload by ${req.user.username} from IP ${clientIp}:`, 
                JSON.stringify({ name: originalName, version, releaseDate, platform, playerCount }, null, 2));

    // Move the uploaded file to the resources directory
    fs.renameSync(req.file.path, filePath);

    const stats = fs.statSync(filePath);

    const uploadDate = releaseDate ? new Date(releaseDate) : new Date();

    const [result] = await pool.query(
      'INSERT INTO resources (name, size, date, version, platform, player_count) VALUES (?, ?, ?, ?, ?, ?)',
      [originalName, stats.size, uploadDate, version, platform, playerCount]
    );

    console.log(`Resource successfully uploaded by ${req.user.username} from IP ${clientIp}: ${originalName} (Version: ${version}, Platform: ${platform}, Player Count: ${playerCount})`);
    res.json({ message: 'Resource uploaded successfully', resourceName: originalName, version: version, platform: platform, playerCount: playerCount });
  } catch (error) {
    console.error(`Error uploading resource by ${req.user.username} from IP ${clientIp}:`, error.message);
    
    // If the file was moved, try to delete it
    if (req.file && req.file.path) {
      try {
        fs.unlinkSync(req.file.path);
      } catch (unlinkError) {
        console.error('Error deleting uploaded file:', unlinkError.message);
      }
    }
    
    res.status(500).json({ error: 'Unable to upload resource', details: error.message });
  }
});

app.delete('/api/resource/:resourceId', authenticateToken, checkRole(1), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Resource deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    const [resourceData] = await pool.query('SELECT name, version FROM resources WHERE id = ?', [req.params.resourceId]);
    const [result] = await pool.query('DELETE FROM resources WHERE id = ?', [req.params.resourceId]);
    if (result.affectedRows === 0) {
      console.log(`Failed resource deletion attempt by ${req.user.username} from IP ${clientIp}: Resource not found (ID: ${req.params.resourceId})`);
      res.status(404).json({ error: 'Resource not found' });
    } else {
      console.log(`Resource successfully deleted by ${req.user.username} from IP ${clientIp}: ${resourceData[0].name} (Version: ${resourceData[0].version}, ID: ${req.params.resourceId})`);
      res.json({ message: 'Resource deleted successfully' });
    }
  } catch (error) {
    console.error(`Error deleting resource by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to delete resource' });
  }
});

app.put('/api/resource/:resourceId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  const { resourceId } = req.params;
  const { name, version, date, platform, playerCount } = req.body;

  console.log(`Admin action initiated: Resource edit by ${req.user.username} from IP ${clientIp}`);
  console.log(`Editing resource ID ${resourceId}:`, JSON.stringify({ name, version, date, platform, playerCount }, null, 2));

  try {
    const [oldData] = await pool.query('SELECT * FROM resources WHERE id = ?', [resourceId]);
    const [result] = await pool.query(
      'UPDATE resources SET name = ?, version = ?, date = ?, platform = ?, player_count = ? WHERE id = ?',
      [name, version, new Date(date), platform, playerCount, resourceId]
    );

    if (result.affectedRows === 0) {
      console.log(`Failed resource edit attempt by ${req.user.username} from IP ${clientIp}: Resource not found (ID: ${resourceId})`);
      res.status(404).json({ error: 'Resource not found' });
    } else {
      console.log(`Resource successfully edited by ${req.user.username} from IP ${clientIp}:`);
      console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
      console.log(`New data:`, JSON.stringify({ name, version, date, platform, playerCount }, null, 2));
      res.json({ message: 'Resource updated successfully' });
    }
  } catch (error) {
    console.error(`Error updating resource by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to update resource' });
  }
});

// Download a resource
app.get('/api/download-resource/:resourceName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [req.params.resourceName]);
    if (rows.length === 0) {
      return res.status(404).send('Resource not found');
    }

    const resourcePath = path.join(resourcesDir, rows[0].name);
    
    // Check if the file exists before attempting to download
    if (!fs.existsSync(resourcePath)) {
      return res.status(404).send('Resource file not found');
    }

    res.download(resourcePath, (err) => {
      if (err) {
        console.error('Error during file download:', err);
        // Don't try to send another response here
      }
    });
  } catch (error) {
    console.error('Error downloading resource:', error);
    res.status(500).send('Error downloading resource');
  }
});

app.get('/api/resource/:resourceId', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM resources WHERE id = ?', [req.params.resourceId]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Resource not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching resource details:', error);
    res.status(500).json({ error: 'Unable to fetch resource details' });
  }
});

app.get('/api/mods', async (req, res) => {
  try {
    const { type, sort, search } = req.query;
    let query = 'SELECT *, download_count';
    const params = [];
    const conditions = [];

    if (type) {
      conditions.push('type = ?');
      params.push(type);
    }

    query += ' FROM modlist';

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    if (sort) {
      switch (sort) {
        case 'most-downloaded':
          query += ' ORDER BY download_count DESC';
          break;
        case 'latest':
          query += ' ORDER BY release_date DESC';
          break;
        default:
          query += ` ORDER BY ${sort}`;
      }
    } else {
      query += ' ORDER BY release_date DESC';
    }

    const [rows] = await pool.query(query, params);

    // Apply fuzzy search on the results
    if (search) {
      const searchTerms = search.toLowerCase().split(' ');
      const fuzzyResults = rows.filter(mod => 
        searchTerms.every(term => 
          fuzzyMatch(term, mod.name || '') || 
          fuzzyMatch(term, mod.creator || '') || 
          fuzzyMatch(term, mod.description || '')
        )
      );
      console.log('Number of fuzzy results:', fuzzyResults.length);
      res.json(fuzzyResults);
    } else {
      res.json(rows);
    }
  } catch (error) {
    console.error('Error fetching mods:', error);
    res.status(500).json({ error: 'Unable to fetch mods', details: error.message });
  }
});

app.get('/api/download-mod/:id', async (req, res) => {
  try {
    const [mod] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
    if (mod.length === 0) {
      console.log(`Mod not found: ID ${req.params.id}`);
      return res.status(404).send('Mod not found');
    }

    const modPath = path.join(__dirname, mod[0].file_path);
    console.log(`Attempting to download mod: ${modPath}`);

    if (!fs.existsSync(modPath)) {
      console.error(`Mod file not found: ${modPath}`);
      return res.status(404).send('Mod file not found');
    }

    // Increment the download count
    await pool.query('UPDATE modlist SET download_count = download_count + 1 WHERE id = ?', [req.params.id]);

    // Use the original filename for download
    const downloadFilename = path.basename(mod[0].file_path);

    res.download(modPath, downloadFilename, (err) => {
      if (err) {
        console.error(`Error downloading mod: ${err.message}`);
        if (!res.headersSent) {
          res.status(500).send('Error downloading mod');
        }
      } else {
        console.log(`Mod downloaded successfully: ${downloadFilename}`);
      }
    });
  } catch (error) {
    console.error('Error in download-mod route:', error);
    res.status(500).send('Server error');
  }
});

app.get('/api/mods/:id', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Mod not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching mod details:', error);
    res.status(500).json({ error: 'Unable to fetch mod details' });
  }
});

// API route to add a new mod
app.post('/api/mods', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  try {
    const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
    const modFile = req.files['modFile'] ? req.files['modFile'][0] : null;
    const previewImage = req.files['previewImage'] ? req.files['previewImage'][0] : null;

    // Store relative paths
    const modFilePath = modFile ? path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/') : null;
    const previewImagePath = previewImage ? path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/') : null;

    // Get the file size if a mod file was uploaded
    const fileSize = modFile ? modFile.size : 0;

    // Insert the mod information into your database
    const [result] = await pool.query(
      'INSERT INTO modlist (name, type, creator, release_date, description, file_path, preview_image_path, compatibility, version, size) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
      [name, type, creator, releaseDate, description, modFilePath, previewImagePath, compatibility, version, fileSize]
    );

    res.json({ id: result.insertId, message: 'Mod added successfully' });
  } catch (error) {
    console.error('Error adding mod:', error);
    res.status(500).json({ error: 'Unable to add mod', details: error.message });
  }
});

// API route to update a mod
app.put('/api/mods/:id', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  const { id } = req.params;
  const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
  const clientIp = getClientIp(req);

  console.log(`Admin action initiated: Mod update by ${req.user.username} from IP ${clientIp}`);

  try {
    // Fetch the old data
    const [oldData] = await pool.query('SELECT * FROM modlist WHERE id = ?', [id]);
    if (oldData.length === 0) {
      console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: Mod not found (ID: ${id})`);
      return res.status(404).json({ error: 'Mod not found' });
    }

    let query = 'UPDATE modlist SET name = ?, type = ?, creator = ?, release_date = ?, description = ?, compatibility = ?, version = ?';
    let params = [name, type, creator, releaseDate, description, compatibility, version];

    let newFilePath = oldData[0].file_path;
    let newFileSize = oldData[0].size;
    let newPreviewPath = oldData[0].preview_image_path;

    if (req.files['modFile']) {
      const modFile = req.files['modFile'][0];
      newFilePath = path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/');
      newFileSize = modFile.size;
      query += ', file_path = ?, size = ?';
      params.push(newFilePath, newFileSize);
    }

    if (req.files['previewImage']) {
      const previewImage = req.files['previewImage'][0];
      newPreviewPath = path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/');
      query += ', preview_image_path = ?';
      params.push(newPreviewPath);
    }

    query += ' WHERE id = ?';
    params.push(id);

    const [result] = await pool.query(query, params);

    if (result.affectedRows === 0) {
      console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: No changes made (ID: ${id})`);
      return res.status(404).json({ error: 'Mod not found or no changes made' });
    }

    console.log(`Mod successfully updated by ${req.user.username} from IP ${clientIp}:`);
    console.log(`Mod ID: ${id}`);
    console.log('Old data:', JSON.stringify(oldData[0], null, 2));
    console.log('New data:', JSON.stringify({
      name, type, creator, release_date: releaseDate, description, compatibility, version,
      file_path: newFilePath,
      size: newFileSize,
      preview_image_path: newPreviewPath
    }, null, 2));

    res.json({ message: 'Mod updated successfully' });
  } catch (error) {
    console.error(`Error updating mod by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to update mod', details: error.message });
  }
});

// API route to delete a mod
app.delete('/api/mods/:id', checkRole(1), async (req, res) => {
  const { id } = req.params;

  try {
    await pool.query('DELETE FROM modlist WHERE id = ?', [id]);
    res.json({ message: 'Mod deleted successfully' });
  } catch (error) {
    console.error('Error deleting mod:', error);
    res.status(500).json({ error: 'Unable to delete mod' });
  }
});

app.get('/api/search-mods', async (req, res) => {
  const searchTerm = req.query.term;
  try {
    const [rows] = await pool.query('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`]);
    res.json(rows);
  } catch (error) {
    console.error('Error searching mods:', error);
    res.status(500).json({ error: 'Unable to search mods' });
  }
});

app.get('/api/sort-mods', async (req, res) => {
  const sortType = req.query.type;
  let query = 'SELECT * FROM modlist';
  
  if (sortType === 'most-downloaded') {
    query += ' ORDER BY download_count DESC';
  } else if (sortType === 'latest') {
    query += ' ORDER BY release_date DESC';
  }

  try {
    const [rows] = await pool.query(query);
    res.json(rows);
  } catch (error) {
    console.error('Error sorting mods:', error);
    res.status(500).json({ error: 'Unable to sort mods' });
  }
});

app.get('/api/current-user', checkAuthToken, (req, res) => {
  console.log('Current user request. User:', JSON.stringify(req.user, null, 2));
  if (req.user) {
    res.json({ user: { id: req.user.id, username: req.user.username, role: req.user.role } });
  } else {
    console.log('No authenticated user found');
    res.status(401).json({ error: 'Not authenticated' });
  }
});

app.get('/api/users', authenticateToken, checkRole(2), async (req, res) => {
  console.log('Fetching users. Authenticated user:', JSON.stringify(req.user, null, 2));
  try {
      const [rows] = await pool.query('SELECT id, username, email, role FROM users');
      console.log('Fetched users:', rows.length);
      res.json(rows);
  } catch (error) {
      console.error('Error fetching users:', error);
      res.status(500).json({ error: 'Unable to fetch users' });
  }
});

app.get('/api/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
  try {
      const [rows] = await pool.query('SELECT id, username, email, role FROM users WHERE id = ?', [req.params.userId]);
      if (rows.length === 0) {
          res.status(404).json({ error: 'User not found' });
      } else {
          res.json(rows[0]);
      }
  } catch (error) {
      console.error('Error fetching user details:', error);
      res.status(500).json({ error: 'Unable to fetch user details' });
  }
});

app.put('/api/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
  const { userId } = req.params;
  const { username, email, role } = req.body;

  // Only allow super admins to change roles
  if (req.user.role !== 1 && role !== undefined) {
      return res.status(403).json({ error: 'Only super admins can change user roles' });
  }

  try {
      const [result] = await pool.query(
          'UPDATE users SET username = ?, email = ?, role = ? WHERE id = ?',
          [username, email, role, userId]
      );
      
      if (result.affectedRows === 0) {
          res.status(404).json({ error: 'User not found' });
      } else {
          res.json({ message: 'User updated successfully' });
      }
  } catch (error) {
      console.error('Error updating user:', error);
      res.status(500).json({ error: 'Unable to update user' });
  }
});

app.delete('/api/users/:userId', authenticateToken, checkRole(1), async (req, res) => {
  const { userId } = req.params;

  try {
      const [result] = await pool.query('DELETE FROM users WHERE id = ?', [userId]);
      if (result.affectedRows === 0) {
          res.status(404).json({ error: 'User not found' });
      } else {
          res.json({ message: 'User deleted successfully' });
      }
  } catch (error) {
      console.error('Error deleting user:', error);
      res.status(500).json({ error: 'Unable to delete user' });
  }
});

app.post('/api/report-demo', authenticateToken, async (req, res) => {
  const { demoId, reportType } = req.body;
  const userId = req.user.id;

  try {
      await pool.query(
          'INSERT INTO demo_reports (demo_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
          [demoId, userId, reportType]
      );
      res.json({ message: 'Report submitted successfully' });
  } catch (error) {
      console.error('Error submitting report:', error);
      res.status(500).json({ error: 'Failed to submit report' });
  }
});

app.get('/api/pending-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
      const [reports] = await pool.query(`
          SELECT dr.*, d.game_type, d.players, u.username 
          FROM demo_reports dr 
          JOIN demos d ON dr.demo_id = d.id 
          JOIN users u ON dr.user_id = u.id 
          WHERE dr.status = 'pending'
      `);
      res.json(reports);
  } catch (error) {
      console.error('Error fetching pending reports:', error);
      res.status(500).json({ error: 'Failed to fetch pending reports' });
  }
});

app.put('/api/resolve-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
  const { reportId } = req.params;
  try {
      await pool.query('UPDATE demo_reports SET status = "resolved" WHERE id = ?', [reportId]);
      res.json({ message: 'Report resolved successfully' });
  } catch (error) {
      console.error('Error resolving report:', error);
      res.status(500).json({ error: 'Failed to resolve report' });
  }
});

app.get('/api/pending-reports-count', authenticateToken, async (req, res) => {
  try {
      const [result] = await pool.query('SELECT COUNT(*) as count FROM demo_reports WHERE status = "pending"');
      console.log('Pending reports count from database:', result[0].count);
      res.json({ count: result[0].count });
  } catch (error) {
      console.error('Error fetching pending reports count:', error);
      res.status(500).json({ error: 'Failed to fetch pending reports count' });
  }
});

app.post('/api/report-mod', authenticateToken, async (req, res) => {
  const { modId, reportType } = req.body;
  const userId = req.user.id;

  try {
      await pool.query(
          'INSERT INTO mod_reports (mod_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
          [modId, userId, reportType]
      );
      res.json({ message: 'Mod report submitted successfully' });
  } catch (error) {
      console.error('Error submitting mod report:', error);
      res.status(500).json({ error: 'Failed to submit mod report' });
  }
});

app.get('/api/pending-mod-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
      const [reports] = await pool.query(`
          SELECT mr.*, m.name as mod_name, u.username 
          FROM mod_reports mr 
          JOIN modlist m ON mr.mod_id = m.id 
          JOIN users u ON mr.user_id = u.id 
          WHERE mr.status = 'pending'
      `);
      res.json(reports);
  } catch (error) {
      console.error('Error fetching pending mod reports:', error);
      res.status(500).json({ error: 'Failed to fetch pending mod reports' });
  }
});

app.put('/api/resolve-mod-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
  const { reportId } = req.params;
  try {
      await pool.query('UPDATE mod_reports SET status = "resolved" WHERE id = ?', [reportId]);
      res.json({ message: 'Mod report resolved successfully' });
  } catch (error) {
      console.error('Error resolving mod report:', error);
      res.status(500).json({ error: 'Failed to resolve mod report' });
  }
});

// Signup route
router.post('/api/signup', async (req, res) => {
  try {
    const { username, email, password } = req.body;

    if (!username || !email || !password) {
      return res.status(400).json({ error: 'All fields are required' });
    }

    // Check if email is already in the database
    const [existingUser] = await pool.query('SELECT * FROM users WHERE email = ?', [email]);
    if (existingUser.length > 0) {
      return res.status(400).json({ error: 'User with this email already exists.' });
    }

    // Hash password
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(password, salt);

    // Generate verification token
    const verificationToken = jwt.sign({ email }, JWT_SECRET, { expiresIn: '1d' });

    // Store the user details temporarily in memory
    pendingVerifications.set(email, {
      username,
      email,
      password: hashedPassword,
      verificationToken,
    });

    // Send verification email
    const verificationLink = `https://defconexpanded.com/verify?token=${verificationToken}`;
    await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Verify Your Email',
      html: `Please click this link to verify your email: <a href="${verificationLink}">${verificationLink}</a>`,
    });

    res.status(200).json({ message: 'Please check your email to verify your account.' });
  } catch (error) {
    console.error('Signup error:', error);
    res.status(500).json({ error: 'Server error.' });
  }
});


// Email verification route
router.get('/verify', async (req, res) => {
  try {
    const { token } = req.query;
    const decoded = jwt.verify(token, JWT_SECRET);
    const email = decoded.email;

    // Check if the email exists in the pendingVerifications map
    if (!pendingVerifications.has(email)) {
      return res.sendFile(path.join(__dirname, 'public', 'verificationerror.html'));
    }

    // Get the user details from the pendingVerifications map
    const userDetails = pendingVerifications.get(email);

    // Create the user in the database
    await pool.query('INSERT INTO users (username, email, password, is_verified) VALUES (?, ?, ?, 1)', 
      [userDetails.username, userDetails.email, userDetails.password]);

    // Remove the user from the pendingVerifications map
    pendingVerifications.delete(email);

    // Serve the success page
    res.sendFile(path.join(__dirname, '..', 'public', 'verificationsuccess.html'));
  } catch (error) {
    console.error('Verification error:', error);
    res.sendFile(path.join(__dirname, '..', 'public', 'verificationerror.html'));
  }
});

setInterval(() => {
  const oneDayAgo = Date.now() - 24 * 60 * 60 * 1000;
  
  for (let [email, { verificationToken }] of pendingVerifications) {
    try {
      const decoded = jwt.verify(verificationToken, JWT_SECRET);
    } catch (error) {
      // If the token has expired, remove the pending verification
      pendingVerifications.delete(email);
    }
  }
}, 3600000); // Run cleanup every hour

// Login route
router.post('/api/login', async (req, res) => {
  const { username, password, rememberMe } = req.body;

  try {
    const [users] = await pool.query('SELECT * FROM users WHERE username = ?', [username]);
    if (users.length === 0) {
      console.log('Login failed: User not found');
      return res.status(400).json({ error: 'Invalid username or password' });
    }

    const user = users[0];

    const isPasswordValid = await bcrypt.compare(password, user.password);
    if (!isPasswordValid) {
      console.log('Login failed: Invalid password');
      return res.status(400).json({ error: 'Invalid username or password' });
    }

    if (!user.is_verified) {
      console.log('Login failed: User not verified');
      return res.status(400).json({ error: 'Please verify your email before logging in' });
    }

    const token = jwt.sign(
      { id: user.id, username: user.username, role: user.role },
      JWT_SECRET,
      { expiresIn: rememberMe ? '30d' : '1d' }
    );

    console.log('Login successful. Token:', token);
    console.log('User:', { id: user.id, username: user.username, role: user.role });

    res.cookie('token', token, {
      httpOnly: true,
      maxAge: rememberMe ? 30 * 24 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000,
    });

    res.locals.user = { id: user.id, username: user.username, role: user.role };

    res.json({ message: 'Login successful', username: user.username, role: user.role });
  } catch (error) {
    console.error('Login error:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

router.post('/api/logout', (req, res) => {
  res.clearCookie('token');
  res.json({ message: 'Logged out successfully' });
});

// Password reset request route
router.post('/api/forgot-password', async (req, res) => {
  const { username, email } = req.body;

  try {
      const [users] = await pool.query('SELECT * FROM users WHERE username = ? AND email = ?', [username, email]);
      if (users.length === 0) {
          return res.status(400).json({ error: 'No user found with that username and email.' });
      }

      const user = users[0];

      // Generate a token for password reset
      const resetToken = crypto.randomBytes(32).toString('hex');
      const resetTokenExpiry = Date.now() + 3600000; // Token valid for 1 hour

      // Store the token and expiry in the database
      await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?', [resetToken, resetTokenExpiry, user.id]);

      // Send email with password reset link
      const resetLink = `https://defconexpanded.com/changepassword?token=${resetToken}`;
      await transporter.sendMail({
          from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
          to: email,
          subject: 'Password Reset',
          html: `Please click this link to reset your password: <a href="${resetLink}">${resetLink}</a>`
      });

      res.json({ message: 'Password reset link sent to your email.' });
  } catch (error) {
      console.error('Forgot password error:', error);
      res.status(500).json({ error: 'Server error. Please try again later.' });
  }
});

// Password reset route
router.post('/api/reset-password', async (req, res) => {
  const { token } = req.query;
  const { password } = req.body;

  try {
      // Find the user by the reset token and ensure it's still valid
      const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()]);
      if (users.length === 0) {
          return res.status(400).json({ error: 'Invalid or expired password reset token.' });
      }

      const user = users[0];

      // Hash the new password
      const salt = await bcrypt.genSalt(10);
      const hashedPassword = await bcrypt.hash(password, salt);

      // Update the user's password and clear the reset token
      await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

      res.json({ message: 'Password changed successfully.' });
  } catch (error) {
      console.error('Reset password error:', error);
      res.status(500).json({ error: 'Server error. Please try again later.' });
  }
});

module.exports = router;

// New route for handling bug reports
app.post('/api/report-bug', async (req, res) => {
  const { bugTitle, bugDescription } = req.body;

  // Email options
  let mailOptions = {
    from: '"DEFCON Expanded" <keiron.mcphee1@gmail.com>',
    to: "keiron.mcphee1@gmail.com", // You can change this if you want to send to a different email
    subject: `New Bug Report: ${bugTitle}`,
    text: `A new bug has been reported:

Title: ${bugTitle}

Description:
${bugDescription}`,
    html: `<h2>A new bug has been reported:</h2>
<p><strong>Title:</strong> ${bugTitle}</p>
<p><strong>Description:</strong></p>
<p>${bugDescription}</p>`
  };

  try {
    // Send the email
    let info = await transporter.sendMail(mailOptions);
    console.log("Bug report email sent: %s", info.messageId);
    res.json({ message: 'Bug report submitted successfully' });
  } catch (error) {
    console.error('Error sending bug report email:', error);
    res.status(500).json({ error: 'Failed to submit bug report' });
  }
});

// Serve additional HTML pages
app.get('/about', checkAuthToken, (res) => sendHtml(res, 'about.html'));
app.get('/guides', checkAuthToken, (res) => sendHtml(res, 'guides.html'));
app.get('/media', checkAuthToken, (res) => sendHtml(res, 'media.html'));
app.get('/resources', checkAuthToken, (res) => sendHtml(res, 'resources.html'));
app.get('/laikasdefcon', checkAuthToken, (res) => sendHtml(res, 'laikasdefcon.html'));
app.get('/homepage/matchroom', checkAuthToken, (res) => sendHtml(res, 'matchroom.html'));
app.get('/homepage', checkAuthToken, (res) => sendHtml(res, 'index.html'));
app.get('/patchnotes', checkAuthToken, (res) => sendHtml(res, 'patchnotes.html'));
app.get('/issue-report', checkAuthToken, (res) => sendHtml(res, 'bugreport.html'));
app.get('/phpmyadmin', checkAuthToken, (res) => sendHtml(res, 'idiot.html'));
app.get('/leaderboard', checkAuthToken, (res) => sendHtml(res, 'leaderboard.html'));
app.get('/modlist', checkAuthToken, (res) => sendHtml(res, 'modlist.html'));
app.get('/privacypolicy', checkAuthToken, (res) => sendHtml(res, 'privacypolicy.html'));
app.get('/modlist/maps', checkAuthToken, (res) => sendHtml(res, 'mapmods.html'));
app.get('/modlist/graphics', checkAuthToken, (res) => sendHtml(res, 'graphicmods.html'));
app.get('/modlist/overhauls', checkAuthToken, (res) => sendHtml(res, 'overhaulmods.html'));
app.get('/modlist/moddingtools', checkAuthToken, (res) => sendHtml(res, 'moddingtools.html'));
app.get('/modlist/ai', checkAuthToken, (res) => sendHtml(res, 'aimods.html'));
app.get('/signup', checkAuthToken, (res) => sendHtml(res, 'signuppage.html'));
app.get('/forgotpassword', checkAuthToken, (res) => sendHtml(res, 'forgotpasswordfor816788487.html'));
app.get('/changepassword', checkAuthToken, (res) => sendHtml(res, 'changepassword248723424.html'));
app.get('/adminpanel', authenticateToken, checkRole(5), serveAdminPage('admin-panel', 5));
app.get('/leaderboardblacklistmanage', authenticateToken, checkRole(5), serveAdminPage('blacklist', 5));
app.get('/demomanage', authenticateToken, checkRole(5), serveAdminPage('demo-manage', 5));
app.get('/leaderboardmanage', authenticateToken, checkRole(5), serveAdminPage('leaderboard-manage', 5));
app.get('/accountmanage', authenticateToken, checkRole(2), serveAdminPage('account-manage', 2));
app.get('/modlistmanage', authenticateToken, checkRole(5), serveAdminPage('modmanagment', 5));
app.get('/resourcemanage', authenticateToken, checkRole(3), serveAdminPage('resourcemanagment', 3));
app.get('/serverconsole', authenticateToken, checkRole(2), serveAdminPage('server-console', 2));

// Serve special files
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'index.html'));
});

app.get('/sitemap', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'sitemap.xml'));
});

app.get('/site.webmanifest', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'site.webmanifest'));
});

app.get('/browserconfig.xml', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'browserconfig.xml'));
});

app.get('/login', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'loginpage.html'));
});

// 404 handler (this should be placed after all other routes)
app.get('*', (req, res) => {
  const requestedPath = path.join(__dirname, 'public', req.path);
  if (fs.existsSync(requestedPath) && fs.statSync(requestedPath).isFile()) {
    res.sendFile(requestedPath);
  } else {
    res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
  }
});

// Error handling middleware
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).send('Something broke!');
});

io.on('connection', (socket) => {
  console.log('A user connected to console output');
});

const originalLog = console.log;
console.log = function() {
  const timestamp = formatTimestamp(new Date());
  const args = Array.from(arguments);
  const message = `[${timestamp}] ${args.join(' ')}`;
  
  // Write to log file
  logStream.write(message + '\n');
  
  // Emit to connected clients
  io.emit('console_output', message);
  
  // Call original console.log
  originalLog.apply(console, [timestamp, ...args]);
};

//Shut down properly so she dont get fucked
process.on('SIGINT', () => {
  logStream.end(() => {
    console.log('Log file stream closed.');
    process.exit(0);
  });
});

setInterval(() => {
  const usedMemory = process.memoryUsage().heapUsed / 1024 / 1024;
  const totalMemory = process.memoryUsage().heapTotal / 1024 / 1024;
  console.log(`Server health check:
    Uptime: ${(Date.now() - startTime) / 1000} seconds
    Memory usage: ${usedMemory.toFixed(2)} MB / ${totalMemory.toFixed(2)} MB
    Active connections: ${io.engine.clientsCount}
  `);
}, 1800000); 

app.use(errorHandler);

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