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
const { Client, GatewayIntentBits, EmbedBuilder } = require('discord.js');
const pendingDemos = new Map();
const pendingJsons = new Map();
const processedJsons = new Set();
const pendingVerifications = new Map();
let discordBotReady = false;

// database connection, do not remove or dox it!
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

// nodemailer setup, also do not dox it!
let transporter = nodemailer.createTransport({
  host: "smtp.gmail.com",
  port: 587,
  secure: false,
  auth: {
    user: "keiron.mcphee1@gmail.com",
    pass: "ueiz tkqy uqwj lwht"
  }
});

//really should move these but its fine
app.use(cookieParser());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(router);
app.use('/uploads', express.static('uploads'));

// directory setup for multer
const demoDir = path.join(__dirname, 'demo_recordings');
const resourcesDir = path.join(__dirname, 'public', 'Files');
const dedconBuildsDir = path.join(__dirname, 'public', 'Files');
const uploadDir = path.join(__dirname, 'public');
const gameLogsDir = path.join(__dirname, 'game_logs');
const modlistDir = path.join(__dirname, 'public', 'modlist');
const modPreviewsDir = path.join(__dirname, 'public', 'modpreviews');

// error handler for everything past this point
const errorHandler = (err, req, res, next) => {
  console.error('Unhandled error:', err);
  res.status(500).json({ error: 'Internal server error', details: err.message });
};

// make sure the specified folders exist and create them if they don't
[demoDir, resourcesDir, dedconBuildsDir, uploadDir, gameLogsDir, modlistDir, modPreviewsDir].forEach(dir => {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
});

// multer configuration 
const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    if (file.fieldname === 'demoFile') {
      cb(null, demoDir);
    } else if (file.fieldname === 'jsonFile') {
      cb(null, gameLogsDir);
    } else if (file.fieldname === 'resourceFile') {
      cb(null, dedconBuildsDir);
    } else if (file.fieldname === 'dedconBuildsFile') {
      cb(null, resourcesDir);
    } else if (file.fieldname === 'modFile') {
      cb(null, modlistDir);
    } else if (file.fieldname === 'previewImage') {
      cb(null, modPreviewsDir);
    } else if (file.fieldname === 'image') {
      cb(null, path.join(__dirname, 'public', 'uploads'));
    } else {
      cb(new Error('Invalid file type'));
    }
  },
  filename: function (req, file, cb) {
    if (file.fieldname === 'image') {
      const userId = req.user ? req.user.id : 'unknown';
      const fileExtension = path.extname(file.originalname);
      const randomString = crypto.randomBytes(8).toString('hex');
      const newFilename = `${userId}_${randomString}${fileExtension}`;
      cb(null, newFilename);
    } else {
      cb(null, file.originalname);
    }
  }
});

// this is is for preventing the .html extension from being abused to bypass server checks
const adminPages = [
  'admin-panel.html',
  'blacklist.html',
  'demo-manage.html',
  'account-manage.html',
  'modmanagment.html',
  'resourcemanagment.html',
  'server-console.html',
  'playerlookup.html',
  'dedconmanagment.html',
  'servermanagment.html'
];

const configFiles = [
  '1v1config.txt',
  '1v1configbest2.txt',
  '1v1configbest.txt',
  '1v1configdefault.txt',
  '1v1configtest.txt',
  '2v2config.txt',
  '2v2tournament.txt',
  '6playerffaconfig.txt',
  'noobfriendly.txt',
  '3v3ffaconfig.txt',
  'hawhaw1v1config.txt',
  'hawhaw2v2config.txt',
  '1v1muricon.txt',
  '4v4config.txt',
  '5v5config.txt',
  '8playerdiplo.txt',
  '10playerdiplo.txt',
  '16playerconfig.txt'
];

//discord bot configuration
const DISCORD_CONFIG = {
  token: 'MTMwNTQxNTU1NzM2NTEwNDY4MA.G6fE2Q.YywmUxfaA368tePVwCysMI_WAPC4p9OTD69f10',
  channelIds: [
    '1117301291652227163',
    '1305455377508470804',
    '1305580168810725447'
  ]
};

const discordBot = new Client({
  intents: [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMessages
  ]
});

//console log to let us know that the discord bot has logged in
discordBot.once('ready', () => {
  console.log(`Discord bot logged in as ${discordBot.user.tag}`);
  discordBotReady = true;
  if (pendingInitialization) {
    console.log('Discord bot ready, proceeding with pending initialization...');
    completePendingInitialization();
  }
});

let pendingInitialization = null;
let completePendingInitialization = null;
discordBot.login(DISCORD_CONFIG.token);

async function sendDemoToDiscord(demo, logData) {
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
}

async function createTerritoryMap(players, territoryImages, usingAlliances, teamColors, allianceColors) {
  try {
    const { createCanvas, loadImage } = require('canvas');
    const canvas = createCanvas(800, 400);
    const ctx = canvas.getContext('2d');

    const baseMap = await loadImage(path.join(__dirname, 'public', 'images', 'base_map.png'));
    ctx.drawImage(baseMap, 0, 0, 800, 400);

    ctx.globalAlpha = 0.7;

    for (const player of players) {
      const territory = territoryImages[player.territory];
      if (!territory) continue;

      const colorSystem = allianceColors;
      const color = colorSystem[player.alliance]?.color || '#00bf00';
      const colorHex = color.replace('#', '');

      try {
        const overlayPath = path.join(__dirname, 'public', 'images', `${territory}${colorHex}.png`);
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
}

// middleware, i really need to move all this stuff all into one place
app.use((req, res, next) => {
  const requestedFile = path.basename(req.url);
  if (adminPages.includes(requestedFile)) {
    return res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
  }
  next();
});


// first line of defence for checking user roles
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

// i forgot what this does, most likely setting public as the static directory for multer and html pages to be served

app.use(express.static('public', {
  setHeaders: (res, path, stat) => {
    if (path.endsWith('.html') && adminPages.includes(path.split('/').pop())) {
      res.set('Content-Type', 'text/plain');
    }
  }
}));
app.use(session({
  secret: 'a8001708554bfcb4bf707fece608fcce49c3ce0d88f5126b137d82b3c7aeab65', // do not dox!
  resave: false,
  saveUninitialized: true,
  cookie: { secure: process.env.NODE_ENV === 'true' }
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


// session token logic
app.use((req, res, next) => {
  if (req.cookies.token) {
    try {
      const decoded = jwt.verify(req.cookies.token, JWT_SECRET);
      req.user = decoded;
      if (!req.session.loginTime) {
        req.session.loginTime = Date.now();
      }
    } catch (err) {
      // clears the cookie when a user is logged out or their session has expired
      res.clearCookie('token');
    }
  }
  next();
});

// should fix the mod download issues!
app.use(timeout('1h'));

// remove timeout for specific routes
const removeTimeout = (req, res, next) => {
  req.setTimeout(0);
  res.setTimeout(0);
  next();
};

app.use((req, res, next) => {
  if (!req.timedout) next();
});

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

const upload = multer({ storage: storage });

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

function allPlayersHaveZeroScore(logData) {
  if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
    return false;
  }

  return logData.players.every(player => player.score === 0);
}

async function initializeServer() {
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

// authentication middleware to validate session tokens
const authenticateToken = async (req, res, next) => {
  const token = req.cookies.token;

  if (!token) {
    console.log('No token found');
    return res.status(401).json({ error: 'Not authenticated' });
  }

  try {
    const decoded = jwt.verify(token, JWT_SECRET);

    // Check the database for the user
    const [users] = await pool.query('SELECT id, username, role FROM users WHERE id = ?', [decoded.id]);
    if (users.length === 0) {
      console.log('Authentication failed: User not found in database');
      return res.status(403).json({ error: 'Invalid token' });
    }

    req.user = users[0];
    next();
  } catch (err) {
    console.log('Authentication failed: Invalid token', err);
    return res.status(403).json({ error: 'Invalid token' });
  }
};

// checks the validity of each request sent by the user
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

// 404 handler
function sendHtml(res, fileName) {
  res.sendFile(path.join(__dirname, 'public', fileName), (err) => {
    if (err) {
      res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
    }
  });
}

// admin panel monitoring data
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

// logs the ip from the user who uses the admin panel
function getClientIp(req) {
  const forwardedFor = req.headers['x-forwarded-for'];
  if (forwardedFor) {
    return forwardedFor.split(',')[0].trim();
  }
  return (req.ip || req.connection.remoteAddress).replace(/^::ffff:/, '');
}

// extra check for the admin pages, basically the same as the other middlewares
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

app.get('/api/config-files', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const configPath = __dirname;
    console.log(`Current working directory: ${process.cwd()}`);
    console.log(`Searching for config files in: ${configPath}`);

    let existingFiles = [];
    for (const filename of configFiles) {
      const filePath = path.join(configPath, filename);
      try {
        await fs.promises.access(filePath, fs.constants.R_OK);
        existingFiles.push(filename);
        console.log(`Found file: ${filePath}`);
      } catch (err) {
        console.log(`File not found: ${filePath}`);
      }
    }
    res.json(existingFiles);
  } catch (error) {
    console.error('Error reading config files:', error);
    res.status(500).json({ error: 'Unable to read config files' });
  }
});

app.get('/api/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
  try {
    if (!configFiles.includes(req.params.filename)) {
      return res.status(403).json({ error: 'Invalid file' });
    }
    const filePath = path.join(__dirname, req.params.filename);
    const content = await fs.promises.readFile(filePath, 'utf8');
    res.json({ content });
  } catch (error) {
    console.error('Error reading file:', error);
    res.status(500).json({ error: 'Unable to read file' });
  }
});

app.put('/api/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
  try {
    if (!configFiles.includes(req.params.filename)) {
      return res.status(403).json({ error: 'Invalid file' });
    }
    const filePath = path.join(__dirname, req.params.filename);
    await fs.promises.writeFile(filePath, req.body.content);
    res.json({ message: 'File updated successfully' });
  } catch (error) {
    console.error('Error writing file:', error);
    res.status(500).json({ error: 'Unable to write file' });
  }
});

// route for downloading server logs
app.get('/download-logs', checkAuthToken, (req, res) => {
  const logPath = path.join(__dirname, 'server.log');
  res.download(logPath, 'server.log', (err) => {
    if (err) {
      console.error('Error downloading log file:', err);
      res.status(500).send('Error downloading log file');
    }
  });
});

// discord widget route
app.get('/api/discord-widget', async (req, res) => {
  try {
    const discordResponse = await axios.get('https://discord.com/api/guilds/244276153517342720/widget.json');
    res.json(discordResponse.data);
  } catch (error) {
    console.error('Error fetching Discord widget:', error);
    res.status(500).json({ error: 'Failed to fetch Discord widget data' });
  }
});

// outdated probably will remove in the future
app.get('/admin-panel', authenticateToken, (req, res) => {
  if (req.user) {
    res.sendFile(path.join(__dirname, 'public', 'admin-panel.html'));
  } else {
    res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
  }
});

// CORS middleware for all routes
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  next();
});

// fetch the blacklist/whitelist
app.get('/api/whitelist', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM leaderboard_whitelist');
    res.json(rows);
  } catch (error) {
    console.error('Error fetching whitelist:', error);
    res.status(500).json({ error: 'Unable to fetch whitelist' });
  }
});

// add a new player to the whitelist/blacklist
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

// remove a player from the whitelist/blacklist
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

// uploading a game to the server through the admin panel
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
  // checks if the demo already exists in the database
  try {
    const [existingDemos] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFile.originalname]);
    if (existingDemos.length > 0) {
      console.log('Demo already exists:', demoFile.originalname);
      return res.status(400).json({ error: 'A demo with this name already exists' });
    }

    const jsonContent = await fs.promises.readFile(jsonFile.path, 'utf8');
    const logData = JSON.parse(jsonContent);

    await processDemoFile(demoFile.originalname, demoFile.size, logData, jsonFile.originalname);

    console.log(`Demo successfully uploaded and processed by ${req.user.username} from IP ${clientIp}: ${demoFile.originalname}`);
    res.json({ message: 'Demo uploaded and processed successfully' });
  } catch (error) {
    console.error(`Error processing uploaded demo by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to process uploaded demo', details: error.message });
  }
});

// fetching demo information from the database for both the homepage and the admin panel
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

// updating a demo in the database
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

    // sends a stringified message into the console for easier reading
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

// fetches the most active players from top 1 to top 5 in the tournament leaderboard
app.get('/api/most-active-players', async (req, res) => {
  try {
    const { serverName, serverList, startDate, endDate, excludeNames = '', limit = 10 } = req.query;

    // Parse excluded names
    const namesToExclude = excludeNames ? excludeNames.split(',') : [];

    // Build query conditions
    let conditions = [];
    let params = [];

    // Server filter - handling both single server and server lists
    if (serverList) {
      // Split server list and create an OR condition
      const servers = serverList.split(',');
      const serverConditions = servers.map(() => 'game_type LIKE ?');
      conditions.push(`(${serverConditions.join(' OR ')})`);
      servers.forEach(server => {
        params.push(`%${server}%`);
      });
    } else if (serverName) {
      // Single server filter
      conditions.push('game_type LIKE ?');
      params.push(`%${serverName}%`);
    }

    // Date range filter
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

    // Build the query
    let query = 'SELECT * FROM demos';
    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }
    query += ' ORDER BY date DESC';

    const [demos] = await pool.query(query, params);

    // Get blacklisted players
    const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));

    // Process player game counts
    const playerGameCounts = {};

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

        // Count each player's games
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

          // Track the most recent game
          const gameDate = new Date(demo.date);
          if (!playerGameCounts[player.name].last_game_date ||
            gameDate > new Date(playerGameCounts[player.name].last_game_date)) {
            playerGameCounts[player.name].last_game_date = demo.date;
          }
        }
      } catch (error) {
        console.error('Error processing player game counts:', error);
      }
    }

    // Convert to array and add profile URLs
    let activePlayers = Object.values(playerGameCounts);

    // Add profile URLs
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

    // Sort by number of games played (descending)
    activePlayers.sort((a, b) => b.games_played - a.games_played);

    // Return only the top N most active players
    const limitedResults = activePlayers.slice(0, parseInt(limit));

    res.json(limitedResults);
  } catch (error) {
    console.error('Error fetching most active players:', error);
    res.status(500).json({ error: 'Unable to fetch active players' });
  }
});

app.get('/api/earliest-game-date', async (req, res) => {
  try {
    // Query to get the earliest game date from the demos table
    const [rows] = await pool.query('SELECT MIN(date) as earliestDate FROM demos');

    if (rows.length > 0 && rows[0].earliestDate) {
      res.json({ earliestDate: rows[0].earliestDate });
    } else {
      res.json({ earliestDate: null });
    }
  } catch (error) {
    console.error('Error fetching earliest game date:', error);
    res.status(500).json({ error: 'Unable to fetch earliest game date' });
  }
});

// loads the leaderboard data from the database and inserts it into the webpage
app.get('/api/leaderboard', async (req, res) => {
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

    const namesToExclude = excludeNames ? excludeNames.split(',') : [];
    let query = 'SELECT * FROM demos';
    let params = [];
    let conditions = [];

    // Server filter
    if (serverList) {
      // Split server list and create an OR condition
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

    // Player name filter
    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                     OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                     OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = [...params, ...Array(10).fill(`%${playerName}%`)];
    }

    // Territory filter
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

    // Date range filter
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

    // Execute the query
    const [demos] = await pool.query(query, params);
    const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));
    const playerStats = {};
    const alliances = {};
    const playerNemesis = {};

    // Process each demo to calculate player statistics
    for (const demo of demos) {
      try {
        let playersData = [];
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            // Handle different structures
            if (Array.isArray(parsedData.players)) {
              playersData = parsedData.players;
            } else if (Array.isArray(parsedData)) {
              playersData = parsedData;
            }
          }
        }

        if (playersData.length < 2) continue;

        // Check if using alliances
        const usingAlliances = playersData.some(player => player.alliance !== undefined);

        // Calculate team/alliance scores
        const groupScores = {};
        playersData.forEach(player => {
          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) return;

          if (!groupScores[groupId]) {
            groupScores[groupId] = 0;
          }
          groupScores[groupId] += player.score || 0;
        });

        // Find the winning team/alliance
        const sortedGroups = Object.entries(groupScores)
          .sort((a, b) => b[1] - a[1]);

        const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];
        const winningGroupId = isTie ? null : Number(sortedGroups[0][0]);

        for (const player of playersData) {
          if (!player.name) continue;

          if (blacklistedPlayers.has(player.name.toLowerCase())) continue;

          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) continue;

          // Determine if the player is on the winning team
          const isWinner = !isTie && groupId === winningGroupId;
          const isTieGame = isTie;

          // Create player entry if it doesn't exist
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

            // Track nemesis data - who beat this player
            if (!isTie && groupId !== winningGroupId) {
              const winningPlayers = playersData.filter(p => {
                const pGroupId = usingAlliances ? p.alliance : p.team;
                return pGroupId === winningGroupId;
              });

              // Record loss against each winning player
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

          // Track most recent game
          const gameDate = new Date(demo.date);
          if (!stats.last_game_date || gameDate > new Date(stats.last_game_date)) {
            stats.last_game_date = demo.date;
          }

          // Track alliance information
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

    // Calculate additional stats
    Object.values(playerStats).forEach(player => {
      player.win_ratio = (player.wins / player.games_played) * 100 || 0;
      player.weighted_score = player.wins * (player.wins / (player.games_played || 1));

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

    // Filter by minimum games played
    if (minGames > 1) {
      leaderboardData = leaderboardData.filter(player => player.games_played >= minGames);
    }

    if (scoreFilter) {
      leaderboardData.sort((a, b) => scoreFilter === 'highest'
        ? b.highest_score - a.highest_score
        : a.highest_score - b.highest_score);
    } else if (gameDuration) {
      // This would need more logic to track game durations per player
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
          leaderboardData.sort((a, b) => b.win_ratio - a.win_ratio || a.player_name.localeCompare(b.player_name));
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

    // Filter out excluded names before returning
    if (namesToExclude.length > 0) {
      leaderboardData = leaderboardData.filter(player =>
        !namesToExclude.includes(player.player_name)
      );
    }

    // Apply limit if specified
    if (limit && !isNaN(parseInt(limit))) {
      leaderboardData = leaderboardData.slice(0, parseInt(limit));
    }

    // If not requesting detailed stats, remove nemesis data to reduce payload size
    if (includeDetailedStats !== 'true') {
      leaderboardData.forEach(player => {
        delete player.nemesis_data;
      });
    }

    // Return the filtered data
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
    console.error('Error generating leaderboard from demos:', error);
    res.status(500).json({ error: 'Unable to generate leaderboard' });
  }
});

app.get('/api/player-nemesis', async (req, res) => {
  try {
    const { playerName, startDate, endDate } = req.query;

    if (!playerName) {
      return res.status(400).json({ error: 'Player name is required' });
    }

    // Build query conditions
    let conditions = ['(player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ? OR ' +
      'player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ? OR ' +
      'player9_name = ? OR player10_name = ?)'];
    let params = Array(10).fill(playerName);

    // Add date filters if provided
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

    // Get all games the player participated in
    const query = `SELECT * FROM demos WHERE ${conditions.join(' AND ')}`;
    const [demos] = await pool.query(query, params);

    // Process demos to find nemesis
    const opponentLosses = {};

    for (const demo of demos) {
      try {
        // Parse player data
        let playersData = [];
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            // Handle different structures
            if (Array.isArray(parsedData.players)) {
              playersData = parsedData.players;
            } else if (Array.isArray(parsedData)) {
              playersData = parsedData;
            }
          }
        }

        if (playersData.length < 2) continue; // Skip invalid data

        // Find the player and their team/alliance
        const playerInfo = playersData.find(p => p.name === playerName);
        if (!playerInfo) continue;

        const usingAlliances = playersData.some(player => player.alliance !== undefined);
        const playerGroupId = usingAlliances ? playerInfo.alliance : playerInfo.team;

        // Calculate team/alliance scores
        const groupScores = {};
        playersData.forEach(player => {
          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) return;

          if (!groupScores[groupId]) {
            groupScores[groupId] = 0;
          }
          groupScores[groupId] += player.score || 0;
        });

        // Find the winning team/alliance
        const sortedGroups = Object.entries(groupScores)
          .sort((a, b) => b[1] - a[1]);

        const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];
        const winningGroupId = isTie ? null : Number(sortedGroups[0][0]);

        // Did the player lose?
        if (!isTie && playerGroupId !== winningGroupId) {
          // Find players on the winning team
          const winningPlayers = playersData.filter(p => {
            const pGroupId = usingAlliances ? p.alliance : p.team;
            return pGroupId === winningGroupId;
          });

          // Record loss against each winning player
          winningPlayers.forEach(winner => {
            if (!winner.name || winner.name === playerName) return;

            if (!opponentLosses[winner.name]) {
              opponentLosses[winner.name] = 0;
            }
            opponentLosses[winner.name]++;
          });
        }
      } catch (error) {
        console.error('Error processing demo for nemesis:', error);
      }
    }

    // Find the nemesis (opponent with most wins against the player)
    let nemesis = null;
    let maxLosses = 0;

    Object.entries(opponentLosses).forEach(([opponent, losses]) => {
      if (losses > maxLosses) {
        nemesis = opponent;
        maxLosses = losses;
      }
    });

    res.json({
      playerName,
      nemesis: nemesis || 'None',
      lossCount: maxLosses,
      allOpponents: opponentLosses
    });

  } catch (error) {
    console.error('Error fetching player nemesis:', error);
    res.status(500).json({ error: 'Unable to fetch nemesis data' });
  }
});

app.get('/api/demo-profile-panel', async (req, res) => {
  const { playerName } = req.query;

  try {
    let query = 'SELECT * FROM demos';
    let params = [];
    let conditions = [];

    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                       OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                       OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = Array(10).fill(`%${playerName}%`);
    }

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    query += ' ORDER BY date DESC';

    const [demos] = await pool.query(query, params);

    const processedDemos = await Promise.all(demos.map(async (demo) => {
      let gameData = { players: [], spectators: [] };
      try {
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            if (parsedData.players && Array.isArray(parsedData.players)) {
              gameData.players = parsedData.players;
            }
            if (parsedData.spectators && Array.isArray(parsedData.spectators)) {
              gameData.spectators = parsedData.spectators;
            }
          }
        }
      } catch (error) {
        console.error('Error parsing players data:', error);
      }

      const updatedDemo = {
        ...demo,
        players: JSON.stringify(gameData.players),
        spectators: JSON.stringify(gameData.spectators)
      };

      return updatedDemo;
    }));

    res.json({ demos: processedDemos });

  } catch (error) {
    console.error('Error fetching demos for profile/admin panel:', error);
    res.status(500).json({ error: 'Unable to fetch demos for profile/admin panel' });
  }
});

app.get('/api/all-demos', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const [demos] = await pool.query('SELECT * FROM demos ORDER BY date DESC');
    res.json(demos);
  } catch (error) {
    console.error('Error fetching all demos:', error);
    res.status(500).json({ error: 'Unable to fetch all demos' });
  }
});

app.get('/api/games-timeline', async (req, res) => {
  try {
    const { graphType = 'individualServers', playerName, startDate, endDate } = req.query;
    
    // Base query for all types
    let query = `SELECT date, game_type, duration, players`;

    // Add territory columns for territory stats
    for (let i = 1; i <= 10; i++) {
      query += `, player${i}_territory`;
    }

    let queryParams = [];
    let conditions = [];

    let baseQuery = ` FROM demos WHERE game_type IN (
      'New Player Server',
      'New Player Server - Training Game',
      'DefconExpanded | 1v1 | Totally Random',
      'DefconExpanded | 1V1 | Best Setups Only!',
      'DefconExpanded | 1v1 | AF vs AS | Totally Random',
      'DefconExpanded | 1v1 | EU vs SA | Totally Random',
      'DefconExpanded | 1v1 | Default',
      'DefconExpanded | 2v2 | Totally Random',
      '2v2 Tournament',
      'DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random',
      'Mojo\\'s 2v2 Arena - Quick Victory',
      'Sony and Hoov\\'s Hideout',
      'DefconExpanded | 3v3 | Totally Random',
      'MURICON | 1v1 Default | 2.8.15',
      '509 CG | 2v2 | Totally Random | 2.8.15',
      '509 CG | 2v2 | Totally Random | 2.8.14.1',
      '509 CG | 1v1 | Totally Random | 2.8.15',
      '509 CG | 1v1 | Totally Random | 2.8.14.1',
      'DefconExpanded | Free For All | Random Cities',
      'DefconExpanded | 8 Player | Diplomacy',
      'DefconExpanded | 4V4 | Totally Random',
      'DefconExpanded | 10 Player | Diplomacy'
    )`;

    query += baseQuery;
    
    // Add player name filter if provided
    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? 
                     OR player4_name LIKE ? OR player5_name LIKE ? OR player6_name LIKE ? 
                     OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? 
                     OR player10_name LIKE ?)`);
      for (let i = 0; i < 10; i++) {
        queryParams.push(`%${playerName}%`);
      }
    }
    
    // Add date range filters if provided
    if (startDate) {
      conditions.push('date >= ?');
      queryParams.push(startDate);
    }
    
    if (endDate) {
      conditions.push('date <= ?');
      queryParams.push(endDate);
    }
    
    // Add conditions to query
    if (conditions.length > 0) {
      query += ` AND ${conditions.join(' AND ')}`;
    }

    query += ` ORDER BY date ASC`;

    const [rows] = await pool.query(query, queryParams);

    // Process data based on graph type
    let chartData;
    switch (graphType) {
      case 'combinedServers':
        chartData = processCombinedServersData(rows);
        break;

      case 'totalHoursPlayed':
        chartData = processTotalHoursData(rows);
        break;

      case 'popularTerritories':
        chartData = processTerritoriesData(rows);
        break;

      case 'individualServers':
      default:
        chartData = processIndividualServersData(rows);
        break;

      case '1v1setupStatistics':
        // Pass date parameters to the processing function
        chartData = process1v1SetupData(rows, { startDate, endDate });
        break;
    }

    res.json(chartData);
  } catch (error) {
    console.error('Error fetching games timeline data:', error);
    res.status(500).json({ error: 'Unable to fetch games timeline data' });
  }
});

function processCombinedServersData(rows) {
  const gamesByDate = {};

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        allServers: 0
      };
    }

    gamesByDate[date].allServers++;
  });

  return Object.values(gamesByDate);
}

function processTotalHoursData(rows) {
  const gamesByDate = {};
  const serverTypeMapping = {
    'New Player Server': 'new_player',
    'New Player Server - Training Game': 'training',
    'DefconExpanded | 1v1 | Totally Random': 'defcon_random',
    'DefconExpanded | 1V1 | Best Setups Only!': 'defcon_best',
    'DefconExpanded | 1v1 | AF vs AS | Totally Random': 'defcon_afas',
    'DefconExpanded | 1v1 | EU vs SA | Totally Random': 'defcon_eusa',
    'DefconExpanded | 1v1 | Default': 'defcon_default',
    'DefconExpanded | 2v2 | Totally Random': 'defcon_2v2',
    '2v2 Tournament': 'tournament_2v2',
    'DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random': 'defcon_2v2_special',
    'Mojo\'s 2v2 Arena - Quick Victory': 'mojo_2v2',
    'Sony and Hoov\'s Hideout': 'sony_hoov',
    'DefconExpanded | 3v3 | Totally Random': 'defcon_3v3',
    'MURICON | 1v1 Default | 2.8.15': 'muricon',
    '509 CG | 2v2 | Totally Random | 2.8.15': 'cg_2v2_2815',
    '509 CG | 2v2 | Totally Random | 2.8.14.1': 'cg_2v2_28141',
    '509 CG | 1v1 | Totally Random | 2.8.15': 'cg_1v1_2815',
    '509 CG | 1v1 | Totally Random | 2.8.14.1': 'cg_1v1_28141',
    'DefconExpanded | Free For All | Random Cities': 'defcon_ffa',
    'DefconExpanded | 8 Player | Diplomacy': 'defcon_8p_diplo',
    'DefconExpanded | 8 Player | Diplomacy ': 'defcon_8p_diplo', 
    'DefconExpanded | 4V4 | Totally Random': 'defcon_4v4',
    'DefconExpanded | 10 Player | Diplomacy': 'defcon_10p_diplo'
  };

  // Diagnostic counters
  const diagnostics = {
    totalGames: rows.length,
    gamesByType: {},
    gamesWithDuration: 0,
    gamesWithoutDuration: 0,
    unknownGameTypes: [],
    totalHoursByType: {}
  };

  // Initialize counters for each game type
  Object.keys(serverTypeMapping).forEach(type => {
    diagnostics.gamesByType[type] = 0;
    diagnostics.totalHoursByType[type] = 0;
  });

  // Create a case-insensitive lookup
  const caseInsensitiveMapping = {};
  Object.keys(serverTypeMapping).forEach(key => {
    caseInsensitiveMapping[key.toLowerCase()] = {
      originalKey: key,
      serverKey: serverTypeMapping[key]
    };
  });

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    // Find the game type using case-insensitive matching
    let serverKey = 'unknown';
    let originalGameType = '';

    if (row.game_type) {
      const lookup = caseInsensitiveMapping[row.game_type.toLowerCase()];
      if (lookup) {
        serverKey = lookup.serverKey;
        originalGameType = lookup.originalKey;
        diagnostics.gamesByType[originalGameType]++;
      } else {
        // Unknown game type
        diagnostics.unknownGameTypes.push(row.game_type);
        console.warn(`Unknown game type: "${row.game_type}"`);
      }
    }

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        totalHours: 0
      };

      // Initialize all server types with 0 hours
      Object.values(serverTypeMapping).forEach(key => {
        gamesByDate[date][key] = 0;
      });
    }

    if (row.duration) {
      diagnostics.gamesWithDuration++;
      try {
        // Make sure to handle potential bad duration formats
        const [hours, minutes, seconds] = row.duration.split(':');
        const h = parseFloat(hours) || 0;
        const m = parseFloat(minutes) || 0;
        const s = parseFloat((seconds || '0').split('.')[0]) || 0;

        const totalHours = h + m / 60 + s / 3600;

        // Add to both total and server-specific hours
        gamesByDate[date].totalHours += totalHours;
        gamesByDate[date][serverKey] += totalHours;

        // Track total hours by game type for diagnostics
        if (originalGameType) {
          diagnostics.totalHoursByType[originalGameType] += totalHours;
        }
      } catch (error) {
        console.error('Error parsing duration:', row.duration, error);
      }
    } else {
      diagnostics.gamesWithoutDuration++;
    }
  });

  // Count unique unknown game types
  const uniqueUnknown = [...new Set(diagnostics.unknownGameTypes)];
  if (uniqueUnknown.length > 0) {
    console.log('Unknown game types:', uniqueUnknown);
  }

  return Object.values(gamesByDate);
}

function processTerritoriesData(rows) {
  const gamesByDate = {};
  const territoryMapping = {
    'North America': 'na',
    'South America': 'sa',
    'Europe': 'eu',
    'Russia': 'ru',
    'Africa': 'af',
    'Asia': 'as',
    'Australasia': 'au',
    'West Asia': 'we',
    'East Asia': 'ea',
    'Antartica': 'ant',
    'North Africa': 'naf',
    'South Africa': 'saf'
  };

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        na: 0, sa: 0, eu: 0, ru: 0, af: 0, as: 0,
        au: 0, we: 0, ea: 0, ant: 0, naf: 0, saf: 0
      };
    }

    // Count territories from all player slots
    for (let i = 1; i <= 10; i++) {
      const territory = row[`player${i}_territory`];
      if (territory) {
        const key = territoryMapping[territory];
        if (key && gamesByDate[date][key] !== undefined) {
          gamesByDate[date][key]++;
        }
      }
    }
  });

  return Object.values(gamesByDate);
}

function process1v1SetupData(rows, options = {}) {
  const { startDate, endDate } = options;
  const setupStats = {};

  // Parse date range if provided
  const parsedStartDate = startDate ? new Date(startDate) : null;
  const parsedEndDate = endDate ? new Date(endDate) : null;

  rows.forEach(row => {
    const rowDate = new Date(row.date);

    // Date range filtering
    if (parsedStartDate && rowDate < parsedStartDate) return;
    if (parsedEndDate && rowDate > parsedEndDate) return;

    let gameData = { players: [], spectators: [] };
    try {
      if (!row.players) return;

      const parsedData = JSON.parse(row.players);
      if (typeof parsedData === 'object') {
        if (parsedData.players && Array.isArray(parsedData.players)) {
          gameData.players = parsedData.players;
        } else if (Array.isArray(parsedData)) {
          gameData.players = parsedData;
        } else {
          return;
        }
      }

      // Make sure it's a valid 1v1 game
      if (gameData.players.length !== 2) return;

      const [player1, player2] = gameData.players;
      if (!player1?.territory || !player2?.territory ||
        player1.score === undefined || player2.score === undefined) {
        return;
      }

      // Create setup key (territories alphabetically ordered)
      const territories = [player1.territory, player2.territory].sort();
      const setupKey = territories.join('_vs_');

      if (!setupStats[setupKey]) {
        setupStats[setupKey] = {
          date: new Date(row.date).toISOString().split('T')[0],
          total_games: 0,
          territories: territories,
          [territories[0]]: 0,
          [territories[1]]: 0,
          total_duration: 0,
          average_score_diff: 0,
          games_with_score: 0
        };
      }

      setupStats[setupKey].total_games++;

      // Track winner
      const winner = player1.score > player2.score ? player1 : player2;
      setupStats[setupKey][winner.territory]++;

      // Track duration if available
      if (row.duration) {
        const [hours, minutes, seconds] = row.duration.split(':');
        const durationInMinutes = (parseFloat(hours) * 60) + parseFloat(minutes) + (parseFloat(seconds) / 60);
        setupStats[setupKey].total_duration += durationInMinutes;
      }

      // Track score differences
      const scoreDiff = Math.abs(player1.score - player2.score);
      setupStats[setupKey].average_score_diff =
        ((setupStats[setupKey].average_score_diff * setupStats[setupKey].games_with_score) + scoreDiff) /
        (setupStats[setupKey].games_with_score + 1);
      setupStats[setupKey].games_with_score++;

    } catch (error) {
      console.error('Error processing 1v1 game:', error);
    }
  });

  // Convert to array and calculate final stats
  const sortedSetups = Object.entries(setupStats)
    .map(([key, stats]) => ({
      setup: key,
      ...stats,
      average_duration: stats.total_duration / stats.total_games,
      win_rate: {
        [stats.territories[0]]: ((stats[stats.territories[0]] / stats.total_games) * 100).toFixed(2) + '%',
        [stats.territories[1]]: ((stats[stats.territories[1]] / stats.total_games) * 100).toFixed(2) + '%'
      }
    }))
    .sort((a, b) => b.total_games - a.total_games);

  return sortedSetups;
}

function processIndividualServersData(rows) {
  const gamesByDate = {};

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        new_player: 0,
        training: 0,
        defcon_random: 0,
        defcon_best: 0,
        defcon_default: 0,
        defcon_afas: 0,
        defcon_eusa: 0,
        defcon_2v2: 0,
        tournament_2v2: 0,
        defcon_2v2_special: 0,
        mojo_2v2: 0,
        sony_hoov: 0,
        defcon_3v3: 0,
        muricon: 0,
        cg_2v2_2815: 0,
        cg_2v2_28141: 0,
        cg_1v1_2815: 0,
        cg_1v1_28141: 0,
        defcon_ffa: 0,
        defcon_8p_diplo: 0,
        defcon_4v4: 0,
        defcon_10p_diplo: 0,
      };
    }

    // Map game types to counters
    if (row.game_type.toLowerCase() === 'new player server'.toLowerCase()) {
      gamesByDate[date].new_player++;
    } else if (row.game_type.toLowerCase() === 'new player server - training game'.toLowerCase()) {
      gamesByDate[date].training++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_random++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | best setups only!'.toLowerCase()) {
      gamesByDate[date].defcon_best++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | af vs as | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_afas++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | eu vs sa | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_eusa++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | default'.toLowerCase()) {
      gamesByDate[date].defcon_default++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 2v2 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_2v2++;
    } else if (row.game_type.toLowerCase() === '2v2 tournament'.toLowerCase()) {
      gamesByDate[date].tournament_2v2++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 2v2 | na-sa-eu-af | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_2v2_special++;
    } else if (row.game_type.toLowerCase() === 'mojo\'s 2v2 arena - quick victory'.toLowerCase()) {
      gamesByDate[date].mojo_2v2++;
    } else if (row.game_type.toLowerCase() === 'sony and hoov\'s hideout'.toLowerCase()) {
      gamesByDate[date].sony_hoov++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 3v3 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_3v3++;
    } else if (row.game_type.toLowerCase() === 'muricon | 1v1 default | 2.8.15'.toLowerCase()) {
      gamesByDate[date].muricon++;
    } else if (row.game_type.toLowerCase() === '509 cg | 2v2 | totally random | 2.8.15'.toLowerCase()) {
      gamesByDate[date].cg_2v2_2815++;
    } else if (row.game_type.toLowerCase() === '509 cg | 2v2 | totally random | 2.8.14.1'.toLowerCase()) {
      gamesByDate[date].cg_2v2_28141++;
    } else if (row.game_type.toLowerCase() === '509 cg | 1v1 | totally random | 2.8.15'.toLowerCase()) {
      gamesByDate[date].cg_1v1_2815++;
    } else if (row.game_type.toLowerCase() === '509 cg | 1v1 | totally random | 2.8.14.1'.toLowerCase()) {
      gamesByDate[date].cg_1v1_28141++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | free for all | random cities'.toLowerCase()) {
      gamesByDate[date].defcon_ffa++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 8 player | diplomacy'.toLowerCase()) {
      gamesByDate[date].defcon_8p_diplo++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 4v4 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_4v4++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 10 player | diplomacy'.toLowerCase()) {
      gamesByDate[date].defcon_10p_diplo++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 16 player | test server'.toLowerCase()) {
      gamesByDate[date].defcon_16p++;
    }
  });

  return Object.values(gamesByDate);
}

// this is the api that makes it happen, half of this stuff i have no idea how it works i got chatgpt to help me out heaps with the mysql queries
app.get('/api/demos', async (req, res) => {
  const {
    page = 1,
    sortBy = 'latest',
    playerName,
    serverName,
    territories,
    players,
    scoreFilter,
    gameDuration,
    scoreDifference,
    startDate,
    endDate,
    gamesPlayed
  } = req.query;

  const limit = 9;
  const offset = (page - 1) * limit;

  try {
    let query = 'SELECT * FROM demos';
    let countQuery = 'SELECT COUNT(*) as total FROM demos';
    let params = [];
    let conditions = [];

    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                       OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                       OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = Array(10).fill(`%${playerName}%`);
    }

    if (serverName) {
      conditions.push('LOWER(game_type) = LOWER(?)');
      params.push(serverName);
    }

    if (territories) {
      const territoryList = territories.split(',');
      const combineMode = req.query.combineMode === 'true';

      if (combineMode) {
        const territoryChecks = territoryList.map(territory =>
          `players LIKE ?`
        ).join(' AND ');

        conditions.push(`(
              (${territoryChecks})
              AND
              JSON_LENGTH(JSON_EXTRACT(players, '$.players')) = ?
          )`);

        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });

        params.push(territoryList.length);
      } else {
        const territoryConditions = territoryList.map(() =>
          `players LIKE ?`
        );
        conditions.push(`(${territoryConditions.join(' OR ')})`);
        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });
      }
    }

    if (players) {
      const playerList = players.split(',').filter(p => p.trim());
      if (playerList.length > 0) {
        playerList.forEach(player => {
          conditions.push(
            `(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
              OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
              OR player9_name LIKE ? OR player10_name LIKE ?)`
          );
          params.push(...Array(10).fill(`%${player}%`));
        });
      }
    }

    if (gamesPlayed) {
      const minGames = parseInt(gamesPlayed);
      const playerColumns = ['player1_name', 'player2_name', 'player3_name', 'player4_name',
        'player5_name', 'player6_name', 'player7_name', 'player8_name',
        'player9_name', 'player10_name'];

      const gameCountSubqueries = playerColumns.map(col => `
        AND (${col} IS NULL OR ${col} IN (
          SELECT player_name 
          FROM (
            SELECT COALESCE(player1_name, player2_name, player3_name, 
                          player4_name, player5_name, player6_name,
                          player7_name, player8_name, player9_name, 
                          player10_name) as player_name,
                   COUNT(*) as game_count
            FROM demos
            GROUP BY player_name
            HAVING game_count >= ?
          ) as frequent_players
        ))
      `);

      conditions.push(`1=1 ${gameCountSubqueries.join(' ')}`);
      params.push(...Array(playerColumns.length).fill(minGames));
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
      countQuery += ' WHERE ' + conditions.join(' AND ');
    }

    let demos;
    let totalDemos;
    let totalPages;

    if (scoreDifference) {
      const [allDemosWithDiffs] = await pool.query(query + ' ORDER BY date DESC', params);
      const processedDemos = allDemosWithDiffs.map(demo => {
        let groupScores = {};

        try {
          let parsedPlayers = [];
          if (demo.players) {
            const playerData = JSON.parse(demo.players);
            parsedPlayers = playerData.players || playerData;

            const usingAlliances = parsedPlayers.some(player => player.alliance !== undefined);

            parsedPlayers.forEach(player => {
              const groupId = usingAlliances ? player.alliance : player.team;
              if (!groupScores[groupId]) {
                groupScores[groupId] = 0;
              }
              groupScores[groupId] += player.score || 0;
            });
          }

          const scores = Object.values(groupScores);
          const scoreDiff = scores.length >= 2 ?
            Math.abs(Math.max(...scores) - Math.min(...scores)) : 0;

          return {
            ...demo,
            scoreDiff
          };

        } catch (error) {
          console.error('Error calculating score difference:', error);
          return {
            ...demo,
            scoreDiff: 0
          };
        }
      });

      processedDemos.sort((a, b) => {
        return scoreDifference === 'largest' ?
          b.scoreDiff - a.scoreDiff :
          a.scoreDiff - b.scoreDiff;
      });

      const start = offset;
      const end = offset + limit;
      demos = processedDemos.slice(start, end);
      totalDemos = processedDemos.length;
      totalPages = Math.ceil(totalDemos / limit);

    } else {
      if (scoreFilter) {
        query += ` ORDER BY GREATEST(
          COALESCE(player1_score, 0), COALESCE(player2_score, 0),
          COALESCE(player3_score, 0), COALESCE(player4_score, 0),
          COALESCE(player5_score, 0), COALESCE(player6_score, 0),
          COALESCE(player7_score, 0), COALESCE(player8_score, 0),
          COALESCE(player9_score, 0), COALESCE(player10_score, 0)
        ) ${scoreFilter === 'highest' ? 'DESC' : 'ASC'}`;
      } else if (gameDuration) {
        query += ` ORDER BY TIME_TO_SEC(duration) ${gameDuration === 'longest' ? 'DESC' : 'ASC'}`;
      } else {
        query += ` ORDER BY ${sortBy === 'most-downloaded' ? 'download_count DESC' : 'date DESC'}`;
      }

      query += ' LIMIT ? OFFSET ?';
      params.push(limit, offset);

      const [fetchedDemos] = await pool.query(query, params);
      const [countResult] = await pool.query(countQuery, params.slice(0, -2));

      demos = fetchedDemos;
      totalDemos = countResult[0].total;
      totalPages = Math.ceil(totalDemos / limit);
    }

    const updatedDemos = await Promise.all(demos.map(async (demo) => {
      let gameData = { players: [], spectators: [] };
      try {
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            if (parsedData.players && Array.isArray(parsedData.players)) {
              gameData.players = parsedData.players;
              if (parsedData.spectators && Array.isArray(parsedData.spectators)) {
                gameData.spectators = parsedData.spectators;
              }
            } else if (Array.isArray(parsedData)) {
              gameData.players = parsedData;
            }
          }
        }
      } catch (error) {
        console.error('Error parsing players data:', error);
        gameData = { players: [], spectators: [] };
      }

      const players = ['player1_name', 'player2_name', 'player3_name', 'player4_name',
        'player5_name', 'player6_name', 'player7_name', 'player8_name',
        'player9_name', 'player10_name'];

      for (const playerKey of players) {
        if (demo[playerKey]) {
          const [userProfile] = await pool.query(`
            SELECT u.username 
            FROM user_profiles up
            JOIN users u ON up.user_id = u.id
            WHERE up.defcon_username = ?
          `, [demo[playerKey]]);

          if (userProfile.length > 0) {
            demo[playerKey + '_profileUrl'] = `/profile/${userProfile[0].username}`;
          }
        }
      }

      return {
        ...demo,
        players: JSON.stringify(gameData.players),
        spectators: JSON.stringify(gameData.spectators)
      };
    }));

    res.json({
      demos: updatedDemos,
      currentPage: parseInt(page),
      totalPages,
      totalDemos
    });
  } catch (error) {
    console.error('Error fetching demos:', error);
    res.status(500).json({ error: 'Unable to fetch demos' });
  }
});

// then use the following route to get the demo details
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

// api for downloading demo files, really should fix this since it directly exposes the api name
app.get('/api/download/:demoName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    if (rows.length === 0) {
      return res.status(404).send('Demo not found');
    }

    const demoPath = path.join(demoDir, rows[0].name);
    if (!fs.existsSync(demoPath)) {
      return res.status(404).send('Demo file not found');
    }

    // Update the download count on the database for the frontend to display
    await pool.query('UPDATE demos SET download_count = download_count + 1 WHERE name = ?', [req.params.demoName]);

    // Initiate file download
    res.download(demoPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Demo download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during demo download:', err);

          // Check if the headers are already sent before trying to send a new response
          if (!res.headersSent) {
            return res.status(500).send('Error downloading demo');
          }
        }
      } else {
        console.log(`Demo downloaded successfully by client with IP: ${clientIp}`);
      }
    });

  } catch (error) {
    console.error('Error downloading demo:', error);

    // Ensure no duplicate response is sent
    if (!res.headersSent) {
      res.status(500).send('Error downloading demo');
    }
  }
});

// route to delete a demo from the database and associated reports, this does not delete the actual file itself
app.delete('/api/demo/:demoId', authenticateToken, checkRole(1), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Demo deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    await pool.query('START TRANSACTION');

    const [demoData] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
    if (demoData.length === 0) {
      await pool.query('ROLLBACK');
      return res.status(404).json({ error: 'Demo not found' });
    }

    await pool.query(
      'INSERT INTO deleted_demos (demo_name, deleted_by) VALUES (?, ?)',
      [demoData[0].name, req.user.id]
    );

    await pool.query('DELETE FROM demo_reports WHERE demo_id = ?', [req.params.demoId]);

    const [result] = await pool.query('DELETE FROM demos WHERE id = ?', [req.params.demoId]);

    await pool.query('COMMIT');

    console.log(`Demo successfully deleted by ${req.user.username} from IP ${clientIp}:`);
    console.log(JSON.stringify(demoData[0], null, 2));

    res.json({ message: 'Demo and associated data deleted successfully' });
  } catch (error) {
    await pool.query('ROLLBACK');
    console.error(`Error deleting demo by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to delete demo' });
  }
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

// fetch the expanded builds from the database
app.get('/api/resources', async (req, res) => {
  try {
    const [resources] = await pool.query('SELECT * FROM resources ORDER BY date DESC');
    res.json(resources);
  } catch (error) {
    console.error('Error fetching resources:', error);
    res.status(500).json({ error: 'Unable to fetch resources' });
  }
});

// route to upload a new resource to the website
app.post('/api/upload-resource', authenticateToken, upload.single('resourceFile'), checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  // let me know who uploaded the resource
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

    // now we can move the file to the files directory
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

// deleting a resource using the admin panel, this does not delete the actual file itself
app.delete('/api/resource/:resourceId', authenticateToken, checkRole(1), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Resource deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    const [resourceData] = await pool.query('SELECT name, version FROM resources WHERE id = ?', [req.params.resourceId]);
    const [result] = await pool.query('DELETE FROM resources WHERE id = ?', [req.params.resourceId]);
    if (result.affectedRows === 0) {
      // lets me know if someone is doing some tom foolery
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

// resource update route using the admin panel
app.put('/api/resource/:resourceId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  const { resourceId } = req.params;
  const { name, version, date, platform, playerCount } = req.body;

  // let me know who is editing the resource
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

// route to download a build from the expanded builds page
app.get('/api/download-resource/:resourceName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [req.params.resourceName]);
    if (rows.length === 0) {
      return res.status(404).send('Resource not found');
    }

    const resourcePath = path.join(resourcesDir, rows[0].name);

    // make sure the file exists
    if (!fs.existsSync(resourcePath)) {
      return res.status(404).send('Resource file not found');
    }

    res.download(resourcePath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Resource download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during resource download:', err);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading resource');
          }
        }
      } else {
        console.log(`Resource downloaded successfully by client with IP: ${clientIp}`);
      }
    });
  } catch (error) {
    console.error('Error downloading resource:', error);
    res.status(500).send('Error downloading resource');
  }
});

// fetches the resource data from the database
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

// route that serves the modlist page, fetches the data from the database
app.get('/api/mods', async (req, res) => {
  try {
    const { type, sort, search } = req.query;

    let query = `
      SELECT m.*, 
             m.download_count, 
             COUNT(DISTINCT ml.user_id) as likes_count, 
             COUNT(DISTINCT mf.user_id) as favorites_count
    `;
    const params = [];
    const conditions = [];

    if (type) {
      conditions.push('m.type = ?');
      params.push(type);
    }

    query += ' FROM modlist m';
    query += ' LEFT JOIN mod_likes ml ON m.id = ml.mod_id';
    query += ' LEFT JOIN mod_favorites mf ON m.id = mf.mod_id';

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    query += ' GROUP BY m.id';

    // Updated sorting logic
    switch (sort) {
      case 'most-downloaded':
        query += ' ORDER BY m.download_count DESC';
        break;
      case 'latest':
        query += ' ORDER BY m.release_date DESC';
        break;
      case 'most-liked':
        query += ' ORDER BY likes_count DESC';
        break;
      case 'most-favorited':
        query += ' ORDER BY favorites_count DESC';
        break;
      default:
        query += ' ORDER BY m.download_count DESC';  // Default to most-downloaded
    }

    const [rows] = await pool.query(query, params);

    // If a user is logged in, fetch their likes and favorites
    let userLikesAndFavorites = { likes: [], favorites: [] };
    if (req.user) {
      userLikesAndFavorites = await getUserLikesAndFavorites(req.user.id);
    }

    // Fuzzy search logic
    let results;
    if (search) {
      const searchTerms = search.toLowerCase().split(' ');
      results = rows.filter(mod =>
        searchTerms.every(term =>
          fuzzyMatch(term, mod.name || '') ||
          fuzzyMatch(term, mod.creator || '') ||
          fuzzyMatch(term, mod.description || '')
        )
      );
      console.log('Number of fuzzy results:', results.length);
    } else {
      results = rows;
    }

    const modsWithUserInfo = results.map(mod => ({
      ...mod,
      isLiked: userLikesAndFavorites.likes.includes(mod.id),
      isFavorited: userLikesAndFavorites.favorites.includes(mod.id)
    }));

    res.json(modsWithUserInfo);
  } catch (error) {
    console.error('Error fetching mods:', error);
    res.status(500).json({ error: 'Unable to fetch mods', details: error.message });
  }
});

app.get('/api/dedcon-builds', async (req, res) => {
  try {
    const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
    res.json(builds);
  } catch (error) {
    console.error('Error fetching builds:', error);
    res.status(500).json({ error: 'Unable to fetch builds' });
  }
});

app.get('/api/admin/dedcon-builds', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
    res.json(builds);
  } catch (error) {
    console.error('Error fetching builds for admin:', error);
    res.status(500).json({ error: 'Unable to fetch builds' });
  }
});

// Upload new build
app.post('/api/upload-dedcon-build', authenticateToken, upload.single('dedconBuildsFile'), checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Build upload by ${req.user.username} from IP ${clientIp}`);

  if (!req.file) {
    console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
    return res.status(400).json({ error: 'No file uploaded' });
  }

  try {
    const { version, releaseDate, platform, playerCount } = req.body;
    const originalName = req.file.originalname;
    // Use the dedconBuildsDir instead of resourcesDir
    const filePath = path.join(dedconBuildsDir, originalName);

    if (!version || !platform || !playerCount) {
      console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
      return res.status(400).json({ error: 'Version, platform, and player count are required' });
    }

    console.log(`Processing build upload by ${req.user.username} from IP ${clientIp}:`,
      JSON.stringify({ name: originalName, version, releaseDate, platform, playerCount }, null, 2));

    // Move the uploaded file to the correct directory
    fs.renameSync(req.file.path, filePath);
    const stats = fs.statSync(filePath);
    const uploadDate = releaseDate ? new Date(releaseDate) : new Date();

    const [result] = await pool.query(
      'INSERT INTO dedcon_builds (name, size, release_date, version, platform, player_count, file_path) VALUES (?, ?, ?, ?, ?, ?, ?)',
      [originalName, stats.size, uploadDate, version, platform, playerCount, filePath]
    );

    console.log(`Build successfully uploaded by ${req.user.username} from IP ${clientIp}`);
    res.json({
      message: 'Build uploaded successfully',
      buildName: originalName,
      version: version,
      platform: platform,
      playerCount: playerCount
    });
  } catch (error) {
    console.error(`Error uploading build by ${req.user.username} from IP ${clientIp}:`, error.message);

    if (req.file && req.file.path) {
      try {
        fs.unlinkSync(req.file.path);
      } catch (unlinkError) {
        console.error('Error deleting uploaded file:', unlinkError.message);
      }
    }

    res.status(500).json({ error: 'Unable to upload build', details: error.message });
  }
});

// Download build route
app.get('/api/download-dedcon-build/:buildName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE name = ?', [req.params.buildName]);
    if (rows.length === 0) {
      return res.status(404).send('Build not found');
    }

    const buildPath = rows[0].file_path;

    if (!fs.existsSync(buildPath)) {
      return res.status(404).send('Build file not found');
    }

    await pool.query('UPDATE dedcon_builds SET download_count = download_count + 1 WHERE name = ?', [req.params.buildName]);

    res.download(buildPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Build download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during build download:', err);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading build');
          }
        }
      } else {
        console.log(`Build downloaded successfully by client with IP: ${clientIp}`);
      }
    });
  } catch (error) {
    console.error('Error downloading build:', error);
    res.status(500).send('Error downloading build');
  }
});

// Delete build route
app.delete('/api/dedcon-build/:buildId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  console.log(`Admin action initiated: Build deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    const [buildData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
    const [result] = await pool.query('DELETE FROM dedcon_builds WHERE id = ?', [req.params.buildId]);

    if (result.affectedRows === 0) {
      console.log(`Failed build deletion attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${req.params.buildId})`);
      res.status(404).json({ error: 'Build not found' });
    } else {
      console.log(`Build successfully deleted by ${req.user.username} from IP ${clientIp}:`, JSON.stringify(buildData[0], null, 2));
      res.json({ message: 'Build deleted successfully' });
    }
  } catch (error) {
    console.error(`Error deleting build by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to delete build' });
  }
});

// Get single build details
app.get('/api/dedcon-build/:buildId', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Build not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching build details:', error);
    res.status(500).json({ error: 'Unable to fetch build details' });
  }
});

// Update build details
app.put('/api/dedcon-build/:buildId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = getClientIp(req);
  const { buildId } = req.params;
  const { name, version, release_date, platform, player_count } = req.body;

  console.log(`Admin action initiated: Build edit by ${req.user.username} from IP ${clientIp}`);
  console.log(`Editing build ID ${buildId}:`, JSON.stringify({ name, version, release_date, platform, player_count }, null, 2));

  try {
    const [oldData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [buildId]);
    const [result] = await pool.query(
      'UPDATE dedcon_builds SET name = ?, version = ?, release_date = ?, platform = ?, player_count = ? WHERE id = ?',
      [name, version, new Date(release_date), platform, player_count, buildId]
    );

    if (result.affectedRows === 0) {
      console.log(`Failed build edit attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${buildId})`);
      res.status(404).json({ error: 'Build not found' });
    } else {
      console.log(`Build successfully edited by ${req.user.username} from IP ${clientIp}:`);
      console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
      console.log(`New data:`, JSON.stringify({ name, version, release_date, platform, player_count }, null, 2));
      res.json({ message: 'Build updated successfully' });
    }
  } catch (error) {
    console.error(`Error updating build by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to update build' });
  }
});

// fetches the likes and favorites for a specific user, then displays them as colored in icons on the modlist
async function getUserLikesAndFavorites(userId) {
  try {
    const [likes] = await pool.query('SELECT mod_id FROM mod_likes WHERE user_id = ?', [userId]);
    const [favorites] = await pool.query('SELECT mod_id FROM mod_favorites WHERE user_id = ?', [userId]);

    return {
      likes: likes.map(like => like.mod_id),
      favorites: favorites.map(favorite => favorite.mod_id)
    };
  } catch (error) {
    console.error('Error fetching user likes and favorites:', error);
    return { likes: [], favorites: [] };
  }
}

// route for users liking a mod
app.post('/api/mods/:id/like', authenticateToken, async (req, res) => {
  const modId = req.params.id;
  const userId = req.user.id;

  try {
    // first check if the user has already liked the mod
    const [existingLike] = await pool.query('SELECT * FROM mod_likes WHERE mod_id = ? AND user_id = ?', [modId, userId]);
    if (existingLike.length > 0) {
      return res.status(400).json({ error: 'User has already liked this mod.' });
    }
    // if not continue
    await pool.query('INSERT INTO mod_likes (mod_id, user_id) VALUES (?, ?)', [modId, userId]);

    // update the likes count in the modlist table
    await pool.query('UPDATE modlist SET likes_count = likes_count + 1 WHERE id = ?', [modId]);

    // let the server know that a user liked a mod
    console.log(`User ${req.user.username} (ID: ${userId}) liked mod ${modId}`);

    res.status(200).json({ message: 'Mod liked!' });
  } catch (error) {
    console.error('Error liking mod:', error);
    res.status(500).json({ error: 'Unable to like mod' });
  }
});

// route for users favoriting a mod
app.post('/api/mods/:id/favorite', authenticateToken, async (req, res) => {
  const modId = req.params.id;
  const userId = req.user.id;

  try {
    // first check if the user has already favorited the mod
    const [existingFavorite] = await pool.query('SELECT * FROM mod_favorites WHERE mod_id = ? AND user_id = ?', [modId, userId]);
    if (existingFavorite.length > 0) {
      return res.status(400).json({ error: 'User has already favorited this mod.' });
    }

    // if they have not, continue
    await pool.query('INSERT INTO mod_favorites (mod_id, user_id) VALUES (?, ?)', [modId, userId]);

    // update the favorites count in the modlist table
    await pool.query('UPDATE modlist SET favorites_count = favorites_count + 1 WHERE id = ?', [modId]);

    // fetches the users favourite mods from the database, then displays them on their user profile page
    const [userProfile] = await pool.query('SELECT favorites FROM user_profiles WHERE user_id = ?', [userId]);
    let currentFavorites = userProfile[0].favorites ? userProfile[0].favorites.split(',') : [];

    currentFavorites.push(modId);

    await pool.query('UPDATE user_profiles SET favorites = ? WHERE user_id = ?', [currentFavorites.join(','), userId]);
    console.log(`User ${req.user.username} (ID: ${userId}) favorited mod ${modId}`);

    res.status(200).json({ message: 'Mod favorited and user profile updated!' });
  } catch (error) {
    console.error('Error favoriting mod:', error);
    res.status(500).json({ error: 'Unable to favorite mod' });
  }
});

// route to download a mod from the modlist
app.get('/api/download-mod/:id', removeTimeout, async (req, res) => {
  try {
    const [mod] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
    if (mod.length === 0) {
      console.log(`Mod not found: ID ${req.params.id}`);
      return res.status(404).json({ error: 'Mod not found' });
    }

    const modPath = path.join(__dirname, mod[0].file_path);
    console.log(`Attempting to download mod: ${modPath}`);

    if (!fs.existsSync(modPath)) {
      console.error(`Mod file not found: ${modPath}`);
      return res.status(404).json({ error: 'Mod file not found' });
    }

    // update the download count in the modlist table
    await pool.query('UPDATE modlist SET download_count = download_count + 1 WHERE id = ?', [req.params.id]);

    // preserve the original file name on download
    const downloadFilename = path.basename(mod[0].file_path);

    res.download(modPath, downloadFilename, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Mod download aborted by client with IP: ${clientIp}`);
        } else {
          console.error(`Error downloading mod: ${err.message}`);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading mod');
          }
        }
      } else {
        console.log(`Mod downloaded successfully (${downloadFilename}) by client with IP: ${clientIp}`);
      }
    });

  } catch (error) {
    console.error('Error in download-mod route:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// fetches the mod from the database, then displays it on the admin panel
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

// route to upload a new mod from the admin panel
app.post('/api/mods', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  try {
    const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
    const modFile = req.files['modFile'] ? req.files['modFile'][0] : null;
    const previewImage = req.files['previewImage'] ? req.files['previewImage'][0] : null;
    const modFilePath = modFile ? path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/') : null;
    const previewImagePath = previewImage ? path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/') : null;
    const fileSize = modFile ? modFile.size : 0;

    // now insert the mod into the database with the provided information from the admin
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

// route to update a mod from the admin panel
app.put('/api/mods/:id', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  const { id } = req.params;
  const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
  const clientIp = getClientIp(req);

  console.log(`Admin action initiated: Mod update by ${req.user.username} from IP ${clientIp}`);

  try {
    // fetch the old data of the mod before updating
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

    // if all these checks pass proceed to inserting the new information to the database
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

// route to delete a mod, this does not delete the file, only the information on the database
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

// api for searching mods
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

// route for sorting the mods, will be updated soon to include most liked and most favourited
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

// main route for sending a console log of any given user request
app.get('/api/current-user', checkAuthToken, (req, res) => {
  if (req.user) {
    res.json({ user: { id: req.user.id, username: req.user.username, role: req.user.role } });
  } else {
    res.status(401).json({ error: 'Not authenticated' });
  }
});

// route for admins to fetch users from the admin panel, really important that only admins have access!
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

// route to display a single user
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

// route to update user role, name and email
app.put('/api/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
  const { userId } = req.params;
  const { username, email, role } = req.body;

  // only allow super admins to change user roles
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

// route for deleting a user, this is last resort nobody is going to use this
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

// route for logged in users to submit a demo report for admins to review
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

// route for the admin panel to fetch all pending demo reports, which will update the user report count
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

// route for elevated users to resolve a specific report
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

// display the total pending user reports on the monitoring data for the admin panel page
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

// route for logged in users to report a mod to admins for review
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

// route for elevated users to view current pending mod reports, which will update the user report count
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

// route for elevated users to resolve a specific mod report
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

// route to handle user signup
router.post('/api/signup', async (req, res) => {
  try {
    const { username, email, password } = req.body;

    if (!username || !email || !password) {
      return res.status(400).json({ error: 'All fields are required' });
    }

    // first check if the username or email already exists in the database
    const [existingUser] = await pool.query('SELECT * FROM users WHERE email = ? OR username = ?', [email, username]);
    if (existingUser.length > 0) {
      return res.status(400).json({ error: 'User with this email or username already exists.' });
    }

    // if not continue to hash and salt their password using bcrpyt
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(password, salt);

    // after submitting a signup request, send them a verification email
    const verificationToken = jwt.sign({ email }, JWT_SECRET, { expiresIn: '1d' });

    // store the user details temorarily to prevent them not being able to sign up if their verification fails
    pendingVerifications.set(email, {
      username,
      email,
      password: hashedPassword,
      verificationToken,
    });

    // now send the verification email
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

// after the signup process, continue to verification
router.get('/verify', async (req, res) => {
  try {
    const { token } = req.query;
    const decoded = jwt.verify(token, JWT_SECRET);
    const email = decoded.email;

    // now check if the users information exists in the pending map
    if (!pendingVerifications.has(email)) {
      return res.sendFile(path.join(__dirname, 'public', 'verificationerror.html'));
    }

    // fetch this data
    const userDetails = pendingVerifications.get(email);

    // now insert the user into the users table and create a user profile
    await pool.query('INSERT INTO users (username, email, password, is_verified) VALUES (?, ?, ?, 1)',
      [userDetails.username, userDetails.email, userDetails.password]);
    await pool.query('INSERT INTO user_profiles (user_id) VALUES (LAST_INSERT_ID())');

    // creates a profile url for the user that has signed up
    const profileUrl = `/profile/${userDetails.username}`;
    await pool.query('UPDATE users SET profile_url = ? WHERE email = ?', [profileUrl, email]);
    pendingVerifications.delete(email);

    // finally send the user to the verification success page
    res.sendFile(path.join(__dirname, 'public', 'verificationsuccess.html'));
  } catch (error) {
    console.error('Verification error:', error);
    res.sendFile(path.join(__dirname, 'public', 'verificationerror.html'));
  }
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

// route that handles user login
router.post('/api/login', async (req, res) => {
  const { username, password, rememberMe } = req.body;

  // first check if the username exists in the database
  try {
    const [users] = await pool.query('SELECT * FROM users WHERE username = ?', [username]);
    if (users.length === 0) {
      console.log('Login failed: User not found');
      return res.status(400).json({ error: 'Invalid username or password' });
    }

    const user = users[0];

    // checks if the password matches the hashed password stored in the database
    const isPasswordValid = await bcrypt.compare(password, user.password);
    if (!isPasswordValid) {
      console.log('Login failed: Invalid password');
      return res.status(400).json({ error: 'Invalid username or password' });
    }

    // now check if the user has been verified before allowing them to log in
    if (!user.is_verified) {
      console.log('Login failed: User not verified');
      return res.status(400).json({ error: 'Please verify your email before logging in' });
    }

    // now assign a session token the user
    const token = jwt.sign(
      { id: user.id, username: user.username, role: user.role },
      JWT_SECRET,
      { expiresIn: rememberMe ? '30d' : '1d' }
    );

    console.log('Login successful. Token:', token);
    console.log('User:', { id: user.id, username: user.username, role: user.role });

    // even the remember me session will expire after a certain period
    res.cookie('token', token, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      maxAge: rememberMe ? 30 * 24 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000,
    });

    res.locals.user = { id: user.id, username: user.username, role: user.role };

    res.json({ message: 'Login successful', username: user.username, role: user.role });
  } catch (error) {
    console.error('Login error:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// route for users to loge out
router.post('/api/logout', (req, res) => {
  // clear the session token
  res.clearCookie('token');
  res.json({ message: 'Logged out successfully' });
});

// route for users to attempt to retreive their password
router.post('/api/forgot-password', async (req, res) => {
  const { username, email } = req.body;

  // first check if the username exists in the database and the email matches the one provided
  try {
    const [users] = await pool.query('SELECT * FROM users WHERE username = ? AND email = ?', [username, email]);
    if (users.length === 0) {
      return res.status(400).json({ error: 'No user found with that username and email.' });
    }

    // if yes continue to send them a reset email
    const user = users[0];
    const resetToken = crypto.randomBytes(32).toString('hex');
    const resetTokenExpiry = Date.now() + 3600000;

    await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?', [resetToken, resetTokenExpiry, user.id]);

    const resetLink = `https://defconexpanded.com/changepassword?token=${resetToken}`; // change this to https://defconexpanded.com/
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

// route for users to change their password
router.post('/api/reset-password', async (req, res) => {
  const { token } = req.query;
  const { password } = req.body;

  // first try to fetch the reset token from the users table
  try {
    const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()]);
    if (users.length === 0) {
      return res.status(400).json({ error: 'Invalid or expired password reset token.' });
    }
    // if yes continue to change their password

    const user = users[0];
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(password, salt);

    await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

    res.json({ message: 'Password changed successfully.' });
  } catch (error) {
    console.error('Reset password error:', error);
    res.status(500).json({ error: 'Server error. Please try again later.' });
  }
});

// route for fetching a users profile using the url inside their profile on the database
app.get('/api/profile/:username', async (req, res) => {
  const username = req.params.username;
  const mode = req.query.mode || 'vanilla';

  try {
    const query = `
          SELECT users.username, user_profiles.*
          FROM users
          JOIN user_profiles ON users.id = user_profiles.user_id
          WHERE users.username = ?
      `;

    const [rows] = await pool.query(query, [username]);

    if (rows.length === 0) {
      return res.status(404).json({ error: 'Profile not found' });
    }

    const userProfile = rows[0];
    const territories = {
      vanilla: ['North America', 'South America', 'Europe', 'Africa', 'Asia', 'Russia'],
      '8player': ['North America', 'South America', 'Europe', 'Africa', 'East Asia', 'West Asia', 'Russia', 'Australasia'],
      '10player': ['North America', 'South America', 'Europe', 'North Africa', 'South Africa', 'Russia', 'East Asia', 'South Asia', 'Australasia', 'Antartica']
    };

    const validTerritories = territories[mode] || territories.vanilla;

    // Initialize response data
    const responseData = {
      ...userProfile,
      winLossRatio: 'Not enough data',
      totalGames: 0,
      wins: 0,
      losses: 0,
      favoriteMods: [],
      main_contributions: userProfile.main_contributions ? userProfile.main_contributions.split(',') : [],
      guides_and_mods: userProfile.guides_and_mods ? userProfile.guides_and_mods.split(',') : [],
      record_score: userProfile.record_score || 0,
      territoryStats: {
        bestTerritory: 'N/A',
        worstTerritory: 'N/A'
      }
    };

    if (userProfile.defcon_username) {
      const gamesQuery = `SELECT players, duration FROM demos WHERE players LIKE ?`;
      const [games] = await pool.query(gamesQuery, [`%${userProfile.defcon_username}%`]);

      let territoryStats = {};
      let highestScore = userProfile.record_score || 0;

      // Calculate average game duration
      let totalDuration = 0;
      let gamesWithDuration = 0;

      games.forEach(game => {
        try {
          // Parse and normalize the player data
          const parsedData = JSON.parse(game.players);
          const playersArray = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);

          const userPlayer = playersArray.find(p => p.name === userProfile.defcon_username);

          // Calculate duration for average
          if (game.duration) {
            const [hours, minutes] = game.duration.split(':').map(Number);
            const durationInMinutes = (hours * 60) + minutes;
            totalDuration += durationInMinutes;
            gamesWithDuration++;
          }

          if (userPlayer && validTerritories.includes(userPlayer.territory)) {
            const usingAlliances = userPlayer.alliance !== undefined;
            const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;

            // Calculate scores
            const scores = {};
            playersArray.forEach(player => {
              const groupId = usingAlliances ? player.alliance : player.team;
              scores[groupId] = (scores[groupId] || 0) + player.score;
            });

            const userGroupScore = scores[userGroupId] || 0;
            const userGroupIsWinner = Object.entries(scores)
              .every(([groupId, score]) => groupId === userGroupId.toString() || score <= userGroupScore);

            const territory = userPlayer.territory;
            territoryStats[territory] = territoryStats[territory] || { wins: 0, losses: 0 };

            if (userGroupIsWinner) {
              responseData.wins++;
              territoryStats[territory].wins++;
            } else {
              responseData.losses++;
              territoryStats[territory].losses++;
            }

            responseData.totalGames++;
            highestScore = Math.max(highestScore, userPlayer.score);
          }
        } catch (err) {
          console.error('Error processing game data:', err);
        }
      });

      // Add average game duration to response
      responseData.avgGameDuration = gamesWithDuration > 0 ? totalDuration / gamesWithDuration : 0;

      // Calculate win/loss ratio
      if (responseData.totalGames > 0) {
        responseData.winLossRatio = (responseData.wins / responseData.totalGames).toFixed(2);
      }

      // Find best and worst territories
      let bestRatio = -1;
      let worstRatio = 2;

      Object.entries(territoryStats).forEach(([territory, stats]) => {
        const total = stats.wins + stats.losses;
        if (total >= 3) { // Minimum threshold of 3 games for territory statistics
          const ratio = stats.wins / total;

          if (ratio > bestRatio) {
            bestRatio = ratio;
            responseData.territoryStats.bestTerritory = territory;
          }

          if (ratio < worstRatio) {
            worstRatio = ratio;
            responseData.territoryStats.worstTerritory = territory;
          }
        }
      });

      // Update record score if necessary
      if (highestScore > responseData.record_score) {
        await pool.query(
          'UPDATE user_profiles SET record_score = ? WHERE user_id = ?',
          [highestScore, userProfile.user_id]
        );
        responseData.record_score = highestScore;
      }
    }

    // Fetch favorite mods
    if (userProfile.favorites) {
      const favoriteModIds = userProfile.favorites.split(',').filter(id => id);
      if (favoriteModIds.length > 0) {
        const [mods] = await pool.query(
          'SELECT * FROM modlist WHERE id IN (?)',
          [favoriteModIds]
        );
        responseData.favoriteMods = mods;
      }
    }

    // Send the response
    res.json(responseData);

  } catch (error) {
    console.error('Error fetching profile:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

app.get('/api/profile-arch-nemesis', async (req, res) => {
  try {
    const { playerName } = req.query;

    if (!playerName) {
      return res.status(400).json({ error: 'Player name is required' });
    }

    // Fetch all games involving this player
    const [demos] = await pool.query(
      `SELECT * FROM demos 
       WHERE player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ? OR 
             player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ? OR 
             player9_name = ? OR player10_name = ?
       ORDER BY date DESC`,
      Array(10).fill(playerName)
    );

    // Track all interactions with other players
    const playerInteractions = {};

    // Process each demo
    for (const demo of demos) {
      try {
        // Parse players data
        let playersData = [];

        if (demo.players) {
          let parsedData;

          try {
            parsedData = JSON.parse(demo.players);
          } catch (parseError) {
            console.error('JSON parse error:', parseError);
            continue;
          }

          if (parsedData && typeof parsedData === 'object') {
            // Handle different data structures
            if (Array.isArray(parsedData.players)) {
              playersData = parsedData.players;
            } else if (Array.isArray(parsedData)) {
              playersData = parsedData;
            }
          }
        }

        if (playersData.length < 2) continue;

        // Find the player in this game
        const userPlayer = playersData.find(p => p.name === playerName);
        if (!userPlayer) continue;

        // Determine if alliances are being used
        const usingAlliances = playersData.some(p => p.alliance !== undefined);
        const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;

        if (userGroupId === undefined) continue; // Skip if player has no team/alliance

        // Calculate team scores
        const groupScores = {};
        playersData.forEach(player => {
          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) return;

          if (!groupScores[groupId]) {
            groupScores[groupId] = 0;
          }
          groupScores[groupId] += player.score || 0;
        });

        // Sort teams by score
        const sortedGroups = Object.entries(groupScores).sort((a, b) => b[1] - a[1]);
        const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];

        // Determine winning team
        let winningGroupId = null;
        if (!isTie && sortedGroups.length > 0) {
          winningGroupId = Number(sortedGroups[0][0]);
        }

        // Determine if user's team won
        const userTeamWon = winningGroupId !== null && userGroupId === winningGroupId;

        // Process each opponent
        playersData.forEach(opponent => {
          if (opponent.name === playerName) return; // Skip self

          const opponentGroupId = usingAlliances ? opponent.alliance : opponent.team;
          if (opponentGroupId === undefined) return; // Skip if opponent has no team/alliance

          // Initialize opponent record if needed
          if (!playerInteractions[opponent.name]) {
            playerInteractions[opponent.name] = {
              games: 0,
              wins: 0,
              losses: 0,
              sameTeam: 0,
              ties: 0,
              otherOutcomes: 0
            };
          }

          // Always increment games played
          playerInteractions[opponent.name].games++;

          // Determine relationship and outcome
          if (opponentGroupId === userGroupId) {
            // On same team
            playerInteractions[opponent.name].sameTeam++;
          } else if (isTie) {
            // Game was a tie
            playerInteractions[opponent.name].ties++;
          } else if (userTeamWon) {
            // User's team won - count as win against this opponent
            playerInteractions[opponent.name].wins++;
          } else if (opponentGroupId === winningGroupId) {
            // Opponent's team won - count as loss to this opponent
            playerInteractions[opponent.name].losses++;
          } else {
            // Neither player's team won (3rd team won)
            playerInteractions[opponent.name].otherOutcomes++;
          }
        });

      } catch (error) {
        console.error('Error processing demo for arch nemesis:', error);
      }
    }

    // Determine the arch nemesis (player with most games against the user)
    let archNemesis = null;
    let maxGames = 0;
    let nemesisWins = 0;
    let nemesisLosses = 0;
    let totalGames = 0;
    let sameTeamGames = 0;
    let tieGames = 0;
    let otherOutcomes = 0;

    Object.entries(playerInteractions).forEach(([opponent, record]) => {
      // Only consider if they have at least 3 games together
      if (record.games >= 3 && record.games > maxGames) {
        archNemesis = opponent;
        maxGames = record.games;
        nemesisWins = record.wins;
        nemesisLosses = record.losses;
        totalGames = record.games;
        sameTeamGames = record.sameTeam;
        tieGames = record.ties;
        otherOutcomes = record.otherOutcomes;
      }
    });

    res.json({
      playerName,
      archNemesis: archNemesis || 'None yet',
      gamesPlayed: totalGames,
      userWins: nemesisWins,
      userLosses: nemesisLosses,
      sameTeamGames,
      tieGames,
      otherOutcomes,
      debug: {
        recordedGames: demos.length,
        trackedInteractions: Object.keys(playerInteractions).length
      }
    });

  } catch (error) {
    console.error('Error calculating arch nemesis:', error);
    res.status(500).json({ error: 'Unable to calculate arch nemesis', details: error.message });
  }
});

app.get('/api/recent-game/:username', async (req, res) => {
  const { username } = req.params;

  try {
    const query = `
      SELECT * FROM demos
      WHERE player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ?
            OR player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ?
            OR player9_name = ? OR player10_name = ?
      ORDER BY date DESC
      LIMIT 1
    `;
    const [recentGame] = await pool.query(query, Array(10).fill(username));

    if (recentGame.length === 0) {
      return res.status(404).json({ error: 'No recent games found' });
    }

    res.json(recentGame[0]);
  } catch (error) {
    console.error('Error fetching recent game:', error);
    res.status(500).json({ error: 'Unable to fetch recent game' });
  }
});


// route for users to update their profile
app.post('/api/update-profile', authenticateToken, async (req, res) => {
  const userId = req.user.id;
  const {
    discord_username,
    steam_id,
    bio,
    defcon_username,
    years_played,
    main_contributions,
    guides_and_mods
  } = req.body;

  try {
    await pool.query(`
          UPDATE user_profiles 
          SET discord_username = ?, 
              steam_id = ?,
              bio = ?,
              defcon_username = ?, 
              years_played = ?,
              main_contributions = ?,
              guides_and_mods = ?
          WHERE user_id = ?
      `, [
      discord_username,
      steam_id,
      bio,
      defcon_username,
      years_played,
      main_contributions.join(','),
      guides_and_mods.join(','),
      userId
    ]);

    res.json({ success: true, message: 'Profile updated successfully' });
  } catch (error) {
    console.error('Error updating profile:', error);
    res.status(500).json({ success: false, message: 'Failed to update profile' });
  }
});


// route for uploading profile pictures and banners
app.post('/api/upload-profile-image', upload.single('image'), async (req, res) => {
  if (!req.file) {
    console.error('No file uploaded');
    return res.status(400).json({ success: false, error: 'No file uploaded' });
  }

  const userId = req.user.id;
  const imageType = req.body.type;

  try {
    const fileExtension = path.extname(req.file.originalname);
    const newFileName = `${userId}_${imageType}${fileExtension}`;
    const newFilePath = path.join('public', 'uploads', newFileName);

    fs.renameSync(req.file.path, newFilePath);

    const imageUrl = `/uploads/${newFileName}`;

    const updateField = imageType === 'profile' ? 'profile_picture' : 'banner_image';
    await pool.query(`UPDATE user_profiles SET ${updateField} = ? WHERE user_id = ?`, [imageUrl, userId]);

    console.log(`Image uploaded and database updated for user ${userId}, type: ${imageType}`);
    res.json({ success: true, imageUrl: imageUrl });
  } catch (error) {
    console.error('Error updating profile image:', error);
    res.status(500).json({ success: false, error: 'Failed to update profile image' });
  }
});

router.post('/api/request-password-change', async (req, res) => {
  const { email } = req.body;

  try {
    // Check if the user exists with the provided email
    const [users] = await pool.query('SELECT * FROM users WHERE email = ?', [email]);
    if (users.length === 0) {
      return res.status(400).json({ error: 'No user found with that email.' });
    }

    const user = users[0];
    const changeToken = crypto.randomBytes(32).toString('hex');
    const changeTokenExpiry = Date.now() + 3600000; // Token expires in 1 hour

    await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?',
      [changeToken, changeTokenExpiry, user.id]);

    // Generate password change link
    const changeLink = `https://defconexpanded.com/change-password?token=${changeToken}`;

    // Send email with password change link
    await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Password Change Request',
      html: `Please click this link to change your password: <a href="${changeLink}">${changeLink}</a>`
    });

    res.json({ message: 'Password change link sent to your email.' });
  } catch (error) {
    console.error('Password change request error:', error);
    res.status(500).json({ error: 'Server error. Please try again later.' });
  }
});

router.post('/api/change-password', async (req, res) => {
  const { token } = req.query;
  const { newPassword, confirmPassword } = req.body;

  if (!token || !newPassword || newPassword !== confirmPassword) {
    return res.status(400).json({ error: 'Invalid request or passwords do not match.' });
  }

  try {
    const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?',
      [token, Date.now()]);

    if (users.length === 0) {
      return res.status(400).json({ error: 'Invalid or expired token.' });
    }

    const user = users[0];

    // Encrypt the new password
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(newPassword, salt);

    await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?',
      [hashedPassword, user.id]);

    res.json({ message: 'Password changed successfully.' });
  } catch (error) {
    console.error('Error changing password:', error);
    res.status(500).json({ error: 'Failed to change password.' });
  }
});

app.post('/api/request-blacklist', authenticateToken, async (req, res) => {
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO blacklist_requests (user_id) VALUES (?)', [userId]);
    res.json({ message: 'Blacklist request submitted successfully' });
  } catch (error) {
    console.error('Error submitting blacklist request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

app.post('/api/request-account-deletion', authenticateToken, async (req, res) => {
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO account_deletion_requests (user_id) VALUES (?)', [userId]);
    res.json({ message: 'Account deletion request submitted successfully' });
  } catch (error) {
    console.error('Error submitting account deletion request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

app.post('/api/request-username-change', authenticateToken, async (req, res) => {
  const { newUsername } = req.body;
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO username_change_requests (user_id, requested_username) VALUES (?, ?)',
      [userId, newUsername]);
    res.json({ message: 'Username change request submitted successfully' });
  } catch (error) {
    console.error('Error submitting username change request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Email change request
app.post('/api/request-email-change', authenticateToken, async (req, res) => {
  const { newEmail } = req.body;
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO email_change_requests (user_id, requested_email) VALUES (?, ?)',
      [userId, newEmail]);
    res.json({ message: 'Email change request submitted successfully' });
  } catch (error) {
    console.error('Error submitting email change request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

app.get('/api/pending-requests', authenticateToken, checkRole(5), async (req, res) => {
  try {

    const [blacklistRequests] = await pool.query(`
          SELECT bl.*, u.username
          FROM blacklist_requests bl
          JOIN users u ON u.id = bl.user_id
          WHERE bl.status = "pending"
      `);

    const [deletionRequests] = await pool.query(`
          SELECT ad.*, u.username
          FROM account_deletion_requests ad
          JOIN users u ON u.id = ad.user_id
          WHERE ad.status = "pending"
      `);

    const [usernameChangeRequests] = await pool.query(`
          SELECT uc.*, u.username
          FROM username_change_requests uc
          JOIN users u ON u.id = uc.user_id
          WHERE uc.status = "pending"
      `);

    const [emailChangeRequests] = await pool.query(`
          SELECT ec.*, u.username
          FROM email_change_requests ec
          JOIN users u ON u.id = ec.user_id
          WHERE ec.status = "pending"
      `);

    const allRequests = [
      ...blacklistRequests.map(r => ({ ...r, type: 'blacklist' })),
      ...deletionRequests.map(r => ({ ...r, type: 'account_deletion' })),
      ...usernameChangeRequests.map(r => ({ ...r, type: 'username_change' })),
      ...emailChangeRequests.map(r => ({ ...r, type: 'email_change' }))
    ];

    res.json(allRequests);
  } catch (error) {
    console.error('Error fetching pending requests:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

app.get('/api/user-pending-requests', authenticateToken, async (req, res) => {
  try {
    const userId = req.user.id; // Retrieve the user's ID from the token

    const [blacklistRequests] = await pool.query(`
      SELECT bl.*, u.username
      FROM blacklist_requests bl
      JOIN users u ON u.id = bl.user_id
      WHERE bl.user_id = ?
    `, [userId]);

    const [deletionRequests] = await pool.query(`
      SELECT ad.*, u.username
      FROM account_deletion_requests ad
      JOIN users u ON u.id = ad.user_id
      WHERE ad.user_id = ?
    `, [userId]);

    const [usernameChangeRequests] = await pool.query(`
      SELECT uc.*, u.username
      FROM username_change_requests uc
      JOIN users u ON u.id = uc.user_id
      WHERE uc.user_id = ?
    `, [userId]);

    const [emailChangeRequests] = await pool.query(`
      SELECT ec.*, u.username
      FROM email_change_requests ec
      JOIN users u ON u.id = ec.user_id
      WHERE ec.user_id = ?
    `, [userId]);

    const userRequests = [
      ...blacklistRequests.map(r => ({ ...r, type: 'blacklist' })),
      ...deletionRequests.map(r => ({ ...r, type: 'account_deletion' })),
      ...usernameChangeRequests.map(r => ({ ...r, type: 'username_change' })),
      ...emailChangeRequests.map(r => ({ ...r, type: 'email_change' }))
    ];

    res.json(userRequests);
  } catch (error) {
    console.error('Error fetching user-specific pending requests:', error);
    res.status(500).json({ error: 'Unable to fetch user requests' });
  }
});


app.put('/api/resolve-request/:requestId/:requestType', authenticateToken, checkRole(5), async (req, res) => {
  const { requestId, requestType } = req.params;
  const { status } = req.body;

  try {
    let tableName;
    switch (requestType) {
      case 'blacklist':
        tableName = 'blacklist_requests';
        break;
      case 'account_deletion':
        tableName = 'account_deletion_requests';
        break;
      case 'username_change':
        tableName = 'username_change_requests';
        break;
      case 'email_change':
        tableName = 'email_change_requests';
        break;
      default:
        return res.status(400).json({ error: 'Invalid request type' });
    }

    await pool.query(`UPDATE ${tableName} SET status = ? WHERE id = ?`, [status, requestId]);

    if (status === 'approved') {
      switch (requestType) {
        case 'blacklist':
          console.log('Blacklist request approved. Admin needs to manually blacklist the user.');
          break;
        case 'account_deletion':
          console.log('Account deletion request approved. Admin needs to manually delete the user account.');
          break;
        case 'username_change':
          console.log('Username change request approved. Admin needs to manually update the username.');
          break;
        case 'email_change':
          console.log('Email change request approved. Admin needs to manually update the email.');
          break;
        default:
          console.log('Unknown request type approved.');
      }
    }

    res.json({ message: 'Request resolved successfully' });
  } catch (error) {
    console.error('Error resolving request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});


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
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// route for demos page numbers
app.get('/demos/page/:pageNumber', checkAuthToken, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.get('/account-settings', authenticateToken, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'profilesettings.html'));
});

app.get('/change-password', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'profilesettings.html'));
});

app.get('/sitemap', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'sitemap.xml'));
});

app.get('/site.webmanifest', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'site.webmanifest'));
});

app.get('/browserconfig.xml', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'browserconfig.xml'));
});

app.get('/login', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'loginpage.html'));
});

app.get('/profile/:username', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'profile.html'));
});

// 404 route handler
app.get('*', (req, res) => {
  const requestedPath = path.join(__dirname, 'public', req.path);
  if (fs.existsSync(requestedPath) && fs.statSync(requestedPath).isFile()) {
    res.sendFile(requestedPath);
  } else {
    res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
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