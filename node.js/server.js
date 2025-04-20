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
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });

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

// server constants to be used
const express = require('express');
const router = express.Router();
const fs = require('fs');
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const session = require('express-session');
const timeout = require('connect-timeout');
const app = express();
const port = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET;
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const logStream = fs.createWriteStream(path.join(__dirname, 'server.log'), { flags: 'a' });
const pendingVerifications = new Map();

// functions to be imported from the modules
const {
    pool,
    demoDir,
    adminPages,
    publicDir,
    gameLogsDir
} = require('./constants');

const {
    authenticateToken,
    checkAuthToken,
    checkRole,
    serveAdminPage
}   = require('./authentication')

const {
    formatTimestamp,
    delay
}   = require('./shared-functions')

const {
    checkForMatch,
    demoExistsInDatabase,
    pendingDemos,
    pendingJsons,
    processedJsons 
}   = require('./demos')

const {
    discordState
} = require('./discord')

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

// authentication middleware
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

// static file serving
app.use(express.static(publicDir, {
    setHeaders: (res, path, stat) => {
        if (path.endsWith('.html') && adminPages.includes(path.split('/').pop())) {
            res.set('Content-Type', 'text/plain');
        }
    }
}));

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

app.use(timeout('1h'));

app.use((req, res, next) => {
    if (!req.timedout) next();
});

// watches the demo directory for new dcrec files
console.log(`Watching demo directory: ${demoDir}`);
const demoWatcher = chokidar.watch(`${demoDir}/*.{dcrec,d8crec,d10crec}`, {
    ignored: /(^|[\/\\])\../,
    persistent: true
});

// watches the log file directory for new json files
console.log(`Watching log file directory: ${gameLogsDir}`);
const jsonWatcher = chokidar.watch(`${gameLogsDir}/{game_*.json,game8p_*.json,game10p_*.json}`, {
    ignored: /(^|[\/\\])\../,
    persistent: true
});

demoWatcher
    .on('add', async (demoPath) => {
        console.log(`New demo detected: ${demoPath}`);
        const demoFileName = path.basename(demoPath);
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
        
        await checkForMatch();
    })
    .on('error', error => console.error(`Demo watcher error: ${error}`));

jsonWatcher
    .on('add', async (jsonPath) => {
        console.log(`New JSON log file detected: ${jsonPath}`);
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

async function initializeServer() {
    console.log("Starting server initialization...");

    const initializationPromise = new Promise((resolve, reject) => {
        if (discordState.isReady) { 
            resolve();
        } else {
            console.log('Waiting for Discord bot to be ready...');
            pendingInitialization = true;
            completePendingInitialization = resolve;
        }
    });

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

app.use((err, req, res, next) => {
    console.error('Error details:', err);
    res.status(500).json({ error: 'Internal server error', details: err.message });
});

// http headers 
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
}, 3600000); // cleans up every hour


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


// homepage handler
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

// if you ever see this then something is broken
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).send('You fucked up');
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
    discordState.isReady = false; 
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