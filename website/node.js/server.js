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
//Last Edited 09-07-2025

const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });

// first lets check if our env file was deleted
console.log("Environment variables loaded:", {
    DB_HOST: process.env.DB_HOST,
    DB_USER: process.env.DB_USER,
    DB_PASSWORD: process.env.DB_PASSWORD ? "SET" : "NOT SET",
    DB_NAME: process.env.DB_NAME,
    DB_PORT: process.env.DB_PORT,
    PORT: process.env.PORT,
    NODE_ENV: process.env.NODE_ENV,
    CORS_ORIGIN: process.env.FRONTEND_URL ? "SET" : "NOT SET",
    JWT_SECRET: process.env.JWT_SECRET ? "SET" : "NOT SET",
    EMAIL_HOST: process.env.EMAIL_HOST,
    EMAIL_PORT: process.env.EMAIL_PORT,
    EMAIL_USER: process.env.EMAIL_USER,
    EMAIL_PASSWORD: process.env.EMAIL_PASSWORD ? "SET" : "NOT SET",
    EMAIL_FROM: process.env.EMAIL_FROM,
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN ? "SET" : "NOT SET",
    DISCORD_CHANNEL_IDS: process.env.DISCORD_CHANNEL_IDS,
    BASE_URL: process.env.BASE_URL,
    DUMPDATABASE_SECRET: process.env.DUMPDATABASE_SECRET ? "SET" : "NOT SET",
    ENV_FILE_LOADED: process.env.DB_PASSWORD ? "YES" : "NO"
});

// the deed
process.env.DB_PASSWORD ? console.log("Hell yea") : console.log("We are fucked");

// server constants to be used
const express = require('express');
const fs = require('fs');
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const session = require('express-session');
const timeout = require('connect-timeout');
const compression = require('compression');
const app = express();
const port = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET;
const httpModule = require('http'); // For making HTTP requests
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const logStream = fs.createWriteStream(path.join(__dirname, 'server.log'), { flags: 'a' });
const pendingVerifications = new Map();
const { spawn } = require('child_process');

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
    checkPermission,
    serveAdminPage
}   = require('./authentication')

const {
    formatTimestamp,
    delay
}   = require('./shared-functions')

const debug = require('./debug-helpers');

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

const permissions = require('./permission-index');
const debugUtils = require('./debug-utils');

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
const rconRoutes = require('./apis/admin/rcon');
const consoleRoutes = require('./apis/admin/console');
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
const smurfCheckerRoutes = require('./apis/smurfchecker/smurf-checker');
const replayViewerRoutes = require('./apis/replay-viewer/replay-viewer');
const domainValidationRoutes = require('./apis/security/domain-validation');

// global error handler, created properly for the first time
const errorHandler = (err, req, res, next) => {
    const startTime = debugUtils.debugFunctionEntry('errorHandler', [err.message], 1);
    
    // build the error stack
    console.error('Unhandled error:', err);
    console.debug(2, 'Error details:', err.stack);
    console.debug(2, 'Request details:', {
        method: req.method,
        url: req.url,
        headers: req.headers,
        user: req.user?.username || 'anonymous'
    });
    
    const isDevelopment = process.env.NODE_ENV === 'development';
    
    const response = {
        error: 'Internal server error',
        timestamp: new Date().toISOString(),
        // include enhanced errors in development
        ...(isDevelopment && { details: err.message })
    };
    
    if (res.headersSent) {
        console.error('Cannot send error response - headers already sent');
        debugUtils.debugFunctionExit('errorHandler', startTime, 'headers_sent', 1);
        return next(err);
    }
    
    res.status(500).json(response);
    debugUtils.debugFunctionExit('errorHandler', startTime, 'error_handled', 1);
};

// quick bug fix, it seems like discords api likes to take fucking donkeys. and we used to have a 30 second timeout here
// the issue is that we would timmeout the request, but then discord would try to send a response causing the infamous
// "headers already sent" error.
app.use((err, req, res, next) => {
    if (req.timedout) {
        console.log('Request timed out:', req.originalUrl);
        // Don't try to send response if headers already sent
        if (!res.headersSent) {
            res.status(408).json({ 
                error: 'Request timeout',
                message: 'The request took too long to process'
            });
        }
        return;
    }
    next(err);
});

// i actually finally added proper CORS, i never worried about security until the replay viewer was added
app.use((req, res, next) => {
    const origin = req.headers.origin;
    const allowedOrigins = [
        process.env.FRONTEND_URL
    ].filter(Boolean);
    
    if (allowedOrigins.includes(origin)) {
        res.header('Access-Control-Allow-Origin', origin);
    }
    
    res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept, Authorization');
    res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.header('Access-Control-Allow-Credentials', 'true');
    next();
});

app.use((req, res, next) => {
    res.set('Cross-Origin-Opener-Policy', 'same-origin');
    res.set('Cross-Origin-Embedder-Policy', 'require-corp');
    
    // static resources we also set resource policy
    if (req.url.includes('/replay-viewer/') || 
        req.url.endsWith('.wasm') || 
        req.url.endsWith('.wasm.map') || 
        req.url.endsWith('.js') || 
        req.url.endsWith('.data') || 
        req.url.endsWith('.worker.js')) {
        res.set('Cross-Origin-Resource-Policy', 'cross-origin');
    }
    
    next();
});

app.use((req, res, next) => {
    if (req.headers['x-forwarded-proto'] === 'https') {
        res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
    }
    next();
});

app.use(cookieParser());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// enable gzip compression for all responses
app.use(compression({
    filter: (req, res) => {
        return compression.filter(req, res);
    },
    threshold: 1024, // only compress files larger than 1kb
    level: 6, // balanced compression level
    // use higher compression for .data files 
    chunkSize: 32 * 1024 
}));

// session middleware
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
    }
    next();
});

app.use((req, res, next) => {
    if (req.url.endsWith('.webmanifest')) {
        res.setHeader('Content-Type', 'application/manifest+json');
    }
    next();
});

// register all api routes
app.use(adminPanelRoutes);
app.use(blacklistRoutes);
app.use(dedconBuildsRoutes);
app.use(dedconConfigRoutes);
app.use(demoAdminRoutes);
app.use(modlistAdminRoutes);
app.use(reportingAdminRoutes);
app.use(resourcesAdminRoutes);
app.use(usersAdminRoutes);
app.use(rconRoutes);
app.use(consoleRoutes);
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
app.use(smurfCheckerRoutes);
app.use(replayViewerRoutes);
app.use(domainValidationRoutes);

// added this here for when i need to verify my ssl certificate from zerossl 
// as no-ip.com does not allow cname validation for some reason?
app.use('/.well-known', express.static(path.join(__dirname, '.wellknown')));

// profile picture and banner uploads location
app.use('/uploads', express.static(path.join(publicDir, 'uploads')));

// serve all static files
app.use(express.static(publicDir, {
    setHeaders: (res, path, stat) => {
        if (path.endsWith('.html') && adminPages.includes(path.split('/').pop())) {
            res.set('Content-Type', 'text/plain');
        }
        
        // apply SharedArrayBuffer headers to web assembly related files
        if (path.endsWith('.wasm') || path.endsWith('.wasm.map') || path.endsWith('.js') || path.endsWith('.data') || path.endsWith('.worker.js')) {
            res.set('Cross-Origin-Resource-Policy', 'cross-origin');
        }
    }
}));

// 404 handler function
function sendHtml(res, fileName) {
    const startTime = debugUtils.debugFunctionEntry('sendHtml', [fileName], 2);
    console.debug(2, `Serving HTML file: ${fileName}`);
    
    res.sendFile(path.join(publicDir, fileName), (err) => {
        if (err) {
            console.debug(1, `File not found: ${fileName}, serving 404`);
            res.status(404).sendFile(path.join(publicDir, '404.html'));
            debugUtils.debugFunctionExit('sendHtml', startTime, '404', 2);
        } else {
            console.debug(2, `Successfully served: ${fileName}`);
            debugUtils.debugFunctionExit('sendHtml', startTime, 'success', 2);
        }
    });
}

// main page route
app.get('/', (req, res) => {
  res.sendFile(path.join(publicDir, 'index.html'));
});

// public routes
app.get('/about', checkAuthToken, (req, res) => sendHtml(res, 'about.html'));
app.get('/about/combined-servers', checkAuthToken, (req, res) => sendHtml(res, 'combinedservers.html'));
app.get('/about/hours-played', checkAuthToken, (req, res) => sendHtml(res, 'totalhoursgraph.html'));
app.get('/about/popular-territories', checkAuthToken, (req, res) => sendHtml(res, 'popularterritories.html'));
app.get('/about/1v1-setup-statistics', checkAuthToken, (req, res) => sendHtml(res, '1v1setupstatistics.html'));
app.get('/smurfchecker', checkAuthToken, (req, res) => sendHtml(res, 'smurfchecker.html'));
app.get('/resources', checkAuthToken, (req, res) => sendHtml(res, 'resources.html'));
app.get('/homepage', checkAuthToken, (req, res) => sendHtml(res, 'index.html'));
app.get('/dedcon-builds', checkAuthToken, (req, res) => sendHtml(res, 'dedconbuilds.html'));
app.get('/phpmyadmin', checkAuthToken, (req, res) => sendHtml(res, 'idiot.html')); // for the memes
//app.get('/patchnotes', checkAuthToken, (req, res) => sendHtml(res, 'patchnotes.html'));
//app.get('/issue-report', checkAuthToken, (req, res) => sendHtml(res, 'bugreport.html'));
//app.get('/laikasdefcon', checkAuthToken, (req, res) => sendHtml(res, 'laikasdefcon.html'));
//app.get('/homepage/matchroom', checkAuthToken, (req, res) => sendHtml(res, 'matchroom.html'));
//app.get('/guides', checkAuthToken, (req, res) => sendHtml(res, 'guides.html'));
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

// admin routes
app.get('/adminpanel', authenticateToken, checkPermission(permissions.PAGE_ADMIN_PANEL), serveAdminPage('admin-panel', permissions.PAGE_ADMIN_PANEL));
app.get('/leaderboardblacklistmanage', authenticateToken, checkPermission(permissions.PAGE_BLACKLIST_MANAGE), serveAdminPage('blacklist', permissions.PAGE_BLACKLIST_MANAGE));
app.get('/demomanage', authenticateToken, checkPermission(permissions.PAGE_DEMO_MANAGE), serveAdminPage('demo-manage', permissions.PAGE_DEMO_MANAGE));
app.get('/playerlookup', authenticateToken, checkPermission(permissions.PAGE_PLAYER_LOOKUP), serveAdminPage('playerlookup', permissions.PAGE_PLAYER_LOOKUP));
app.get('/defconservers', authenticateToken, checkPermission(permissions.PAGE_DEFCON_SERVERS), serveAdminPage('servermanagment', permissions.PAGE_DEFCON_SERVERS));
app.get('/accountmanage', authenticateToken, checkPermission(permissions.PAGE_ACCOUNT_MANAGE), serveAdminPage('account-manage', permissions.PAGE_ACCOUNT_MANAGE));
app.get('/modlistmanage', authenticateToken, checkPermission(permissions.PAGE_MODLIST_MANAGE), serveAdminPage('modmanagment', permissions.PAGE_MODLIST_MANAGE));
app.get('/dedconmanagment', authenticateToken, checkPermission(permissions.PAGE_DEDCON_MANAGEMENT), serveAdminPage('dedconmanagment', permissions.PAGE_DEDCON_MANAGEMENT));
app.get('/resourcemanage', authenticateToken, checkPermission(permissions.PAGE_RESOURCE_MANAGE), serveAdminPage('resourcemanagment', permissions.PAGE_RESOURCE_MANAGE));
app.get('/serverconsole', authenticateToken, checkPermission(permissions.PAGE_SERVER_CONSOLE), serveAdminPage('server-console', permissions.PAGE_SERVER_CONSOLE));
app.get('/rconconsole', authenticateToken, checkPermission(permissions.PAGE_RCON_CONSOLE), serveAdminPage('rcon-console', permissions.PAGE_RCON_CONSOLE));

// User routes
app.get('/demos/page/:pageNumber', checkAuthToken, (req, res) => {
  res.sendFile(path.join(publicDir, 'index.html'));
});

app.get('/account-settings', authenticateToken, (req, res) => {
  res.sendFile(path.join(publicDir, 'profilesettings.html'));
});

app.get('/change-password', (req, res) => {
  res.sendFile(path.join(publicDir, 'profilesettings.html'));
});

app.get('/login', (req, res) => {
  res.sendFile(path.join(publicDir, 'loginpage.html'));
});

app.get('/profile/:username', (req, res) => {
  res.sendFile(path.join(publicDir, 'profile.html'));
});

// Static file routes
app.get('/sitemap', (req, res) => {
  res.sendFile(path.join(publicDir, 'sitemap.xml'));
});

app.get('/site.webmanifest', (req, res) => {
  res.sendFile(path.join(publicDir, 'site.webmanifest'));
});

app.get('/browserconfig.xml', (req, res) => {
  res.sendFile(path.join(publicDir, 'browserconfig.xml'));
});

// function to find the current replay viewer files
function findReplayViewerFiles() {
    const startTime = debugUtils.debugFunctionEntry('findReplayViewerFiles', [], 2);
    
    try {
        const files = fs.readdirSync(demoDir);
        const replayFiles = {
            html: null,
            js: null,
            wasm: null,
            wasmMap: null,
            data: null,
            version: null
        };
        
        // looks for replay_viewer_*.* files with this regex pattern
        const replayPattern = /^replay_viewer_(.+)\.(.+)$/;
        
        for (const file of files) {
            const match = file.match(replayPattern);
            if (match) {
                const version = match[1];
                const extension = match[2];
                
                if (!replayFiles.version) {
                    replayFiles.version = version;
                }
                
                switch (extension) {
                    case 'html':
                        replayFiles.html = file;
                        break;
                    case 'js':
                        replayFiles.js = file;
                        break;
                    case 'wasm':
                        replayFiles.wasm = file;
                        break;
                    case 'wasm.map':
                        replayFiles.wasmMap = file;
                        break;
                    case 'data':
                        replayFiles.data = file;
                        break;
                }
            }
        }
        
        debugUtils.debugFunctionExit('findReplayViewerFiles', startTime, replayFiles, 2);
        return replayFiles;
        
    } catch (error) {
        console.error('Error, some files could not be found', error);
        debugUtils.debugFunctionExit('findReplayViewerFiles', startTime, 'error', 2);
        return { html: null, js: null, wasm: null, wasmMap: null, data: null, version: null };
    }
}

// apply the shared array buffer headers to all files used by the replay viewer
function setSharedArrayBufferHeaders(res, path, stat) {
    res.set('Cross-Origin-Opener-Policy', 'same-origin');
    res.set('Cross-Origin-Embedder-Policy', 'require-corp');
    res.set('Cross-Origin-Resource-Policy', 'cross-origin');
    
    // Set specific content types for WebAssembly files
    if (path.endsWith('.wasm')) {
        res.set('Content-Type', 'application/wasm');
    } else if (path.endsWith('.wasm.map')) {
        res.set('Content-Type', 'application/json');
    } else if (path.endsWith('.js')) {
        res.set('Content-Type', 'application/javascript');
    } else if (path.endsWith('.html')) {
        res.set('Content-Type', 'text/html');
    } else if (path.endsWith('.css')) {
        res.set('Content-Type', 'text/css');
    } else if (path.endsWith('.png')) {
        res.set('Content-Type', 'image/png');
    } else if (path.endsWith('.jpg') || path.endsWith('.jpeg')) {
        res.set('Content-Type', 'image/jpeg');
    } else if (path.endsWith('.svg')) {
        res.set('Content-Type', 'image/svg+xml');
    } else if (path.endsWith('.ico')) {
        res.set('Content-Type', 'image/x-icon');
    } else if (path.endsWith('.data')) {
        res.set('Content-Type', 'application/octet-stream');
    } else if (path.endsWith('.worker.js')) {
        res.set('Content-Type', 'application/javascript');
    }
}

// static serving for the replay viewer
app.use('/replay-viewer', (req, res, next) => {
  if (req.path.endsWith('.dcrec')) {
      return next();
  }
  
  express.static(demoDir, {
      setHeaders: (res, path, stat) => {
          setSharedArrayBufferHeaders(res, path, stat);
          
          // 1 year cache duration, in the event that no new updates are made
          if (path.endsWith('.wasm') || path.endsWith('.wasm.map') || path.endsWith('.js') || path.endsWith('.data')) {
              res.set('Cache-Control', 'public, max-age=31536000, immutable'); // 1 year
              res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
          } 
          // cache html files for 1 day
          else if (path.endsWith('.html')) {
              res.set('Cache-Control', 'public, max-age=86400'); // 1 day
              res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
          }
          // cache service worker for short time to allow updates
          else if (path.endsWith('replay-viewer-service-worker.js')) {
              res.set('Cache-Control', 'public, max-age=3600'); // 1 hour
              res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
          }
      }
  })(req, res, next);
});

// serve the .dcrec files directly for the replay viewer
app.get('/replay-viewer/files/*.dcrec', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveDcrecFile', [req.path], 2);
    const filename = path.basename(req.path);
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving .dcrec file:', filename);
    
    const dcrecPath = path.join(demoDir, filename);
    debug.fileOp('serve', dcrecPath, 3);
    
    try {
        await fs.promises.access(dcrecPath);
        debug.level2('File found, serving .dcrec file:', filename);
        res.setHeader('Content-Type', 'application/octet-stream');
        res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
        res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
        res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
        res.sendFile(dcrecPath);
        debug.exit('serveDcrecFile', startTime, 'success', 2);
    } catch (error) {
        debug.level2('Demo file not found on disk:', dcrecPath);
        debug.apiResponse(404, 'Demo file not found', 2);
        debug.exit('serveDcrecFile', startTime, 'not_found', 2);
        res.status(404).send('Demo file not found.');
    }
});

// serve the replay viewer html file
app.get('/replay-viewer/:filename', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveReplayViewer', [req.params.filename], 1);
    const filename = req.params.filename;
    
    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Requested replay viewer for:', filename);
    
    const replayFiles = findReplayViewerFiles();
    debug.level2('Found replay viewer files:', replayFiles);
    
    let replayViewerPath;
    let jsFileName = 'defcon.js'; // mostly for development, we would never use this on the live server
    
    if (replayFiles.html) {
        replayViewerPath = path.join(demoDir, replayFiles.html);
        jsFileName = replayFiles.js || 'defcon.js';
    } else {
        debug.level2('No replay viewer HTML found');
        debug.apiResponse(404, 'Replay viewer not found', 2);
        debug.exit('serveReplayViewer', startTime, 'html_not_found', 1);
        return res.status(404).send('Replay viewer not found. Please build the WebAssembly replay viewer first.');
    }
    
    debug.fileOp('check', replayViewerPath, 3);
    
    try {
        await fs.promises.access(replayViewerPath);
        const dcrecPath = path.join(demoDir, filename);
        debug.fileOp('check', dcrecPath, 2);
        
        try {
            await fs.promises.access(dcrecPath);
            debug.level3('Demo file exists: true');
        } catch (error) {
            debug.level2('Demo file not found:', dcrecPath);
            debug.apiResponse(404, 'Demo file not found', 2);
            debug.exit('serveReplayViewer', startTime, 'demo_not_found', 1);
            return res.status(404).send('Demo file not found.');
        }
      
        // read the html file and inject the command line arguments + file loading logic
        debug.fileOp('read', replayViewerPath, 2);
        
        try {
            const data = await fs.promises.readFile(replayViewerPath, 'utf8');
            
            debug.level2('HTML file read successfully, length:', data.length);
            debug.level2('Injecting file loading logic for:', filename);
            debug.level2('Using JS file:', jsFileName);
          
            // inject file loading logic and command line arguments into the html file
            const scriptPattern = new RegExp(`<script[^>]*src="${jsFileName.replace('.', '\\.')}"[^>]*><\\/script>`);
            const beforeMatch = data.match(scriptPattern);
            debug.level3('Script injection pattern:', scriptPattern.toString());
            debug.level3('Found script tag:', beforeMatch ? beforeMatch[0] : 'NOT FOUND');
            
            // this is mostly for development, we would never use this on the live server
            const fallbackPattern = /<script[^>]*src="defcon\.js"[^>]*><\/script>/;
            const actualPattern = beforeMatch ? scriptPattern : fallbackPattern;
          
            const modifiedHtml = data.replace(
                actualPattern,
                `<script>
                    window.Module = window.Module || {};
                    window.Module.arguments = ['-l', '${filename}'];
                    window.Module.print = function(text) {
                    };
                    window.Module.printErr = function(text) {
                        console.error('[WASM Error]:', text);
                    };
                    
                    window.Module.preRun = window.Module.preRun || [];
                    window.Module.preRun.push(async function() {
                        
                        try {
                            const response = await fetch('/replay-viewer/files/${filename}');
                            
                            if (!response.ok) {
                                throw new Error(\`HTTP error! status: \${response.status}\`);
                            }

                            const arrayBuffer = await response.arrayBuffer();
                            const uint8Array = new Uint8Array(arrayBuffer);

                            FS.writeFile('${filename}', uint8Array);
                            
                            if (FS.analyzePath('${filename}').exists) {
                                const stats = FS.stat('${filename}');
                            } else {
                            }
                        } catch (error) {
                        }
                    });
                  
                </script>
                <script async type="text/javascript" src="${jsFileName}"></script>`
            );
          
            // now time to see if the replacement worked
            const afterMatch = modifiedHtml.match(actualPattern);
            debug.level3('Script tag after replacement:', afterMatch ? 'STILL FOUND (ERROR)' : 'REPLACED SUCCESSFULLY');
            debug.level3('Module.preRun injected:', modifiedHtml.includes('Module.preRun'));
            debug.level2('File loading logic injected, sending HTML response');
            debug.apiResponse(200, 'HTML with injected WebAssembly logic', 2);
            debug.exit('serveReplayViewer', startTime, 'success', 1);
            
            // set CORS headers for SharedArrayBuffer support (required for web assembly threading)
            res.set('Cross-Origin-Opener-Policy', 'same-origin');
            res.set('Cross-Origin-Embedder-Policy', 'require-corp');
            res.set('Cross-Origin-Resource-Policy', 'cross-origin');
            res.set('Content-Type', 'text/html');
            
            res.send(modifiedHtml);
            
        } catch (error) {
            debug.error('serveReplayViewer', error, 1);
            debug.apiResponse(500, 'Error reading replay viewer file', 1);
            debug.exit('serveReplayViewer', startTime, 'file_read_error', 1);
            return res.status(500).send('Error reading replay viewer file.');
        }
        
    } catch (error) {
        // darth vader noises
        debug.level2('Replay viewer HTML not found:', replayViewerPath);
        debug.apiResponse(404, 'Replay viewer not found', 2);
        debug.exit('serveReplayViewer', startTime, 'html_not_found', 1);
        res.status(404).send('Replay viewer not found. Please build the WebAssembly replay viewer first.');
    }
});

// serve source files for WebAssembly debugging
app.get('/contrib/*', (req, res) => {
    const startTime = debug.enter('serveSourceFile', [req.path], 2);
    
    // only serve source files in development or when debugging
    const isDevelopment = process.env.NODE_ENV === 'development';
    const isDebugMode = req.headers['user-agent']?.includes('Chrome') && req.headers.referer?.includes('replay-viewer');
    
    if (!isDevelopment && !isDebugMode) {
        debug.level2('Source file access denied - not in debug mode');
        debug.apiResponse(403, 'Source files not available in production', 2);
        debug.exit('serveSourceFile', startTime, 'access_denied', 2);
        return res.status(403).send('Source files not available in production');
    }
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving source file for debugging:', req.path);
    
    // construct the source file path from project root
    const sourcePath = path.join(__dirname, '../..', req.path);
    debug.fileOp('serve_source', sourcePath, 3);
    
    try {
        // check if file exists
        if (fs.existsSync(sourcePath)) {
            debug.level2('Source file found, serving:', path.basename(sourcePath));
            res.setHeader('Content-Type', 'text/plain; charset=utf-8');
            res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
            res.sendFile(sourcePath);
            debug.exit('serveSourceFile', startTime, 'success', 2);
        } else {
            debug.level2('Source file not found:', sourcePath);
            debug.apiResponse(404, 'Source file not found', 2);
            debug.exit('serveSourceFile', startTime, 'not_found', 2);
            res.status(404).send('Source file not found');
        }
    } catch (error) {
        debug.error('serveSourceFile', error, 1);
        debug.apiResponse(500, 'Error serving source file', 1);
        debug.exit('serveSourceFile', startTime, 'error', 1);
        res.status(500).send('Error serving source file');
    }
});

// serve source files from the source directory for WebAssembly debugging
app.get('/source/*', (req, res) => {
    const startTime = debug.enter('serveSourceDirectory', [req.path], 2);
    
    // only serve source files in development or when explicitly debugging
    const isDevelopment = process.env.NODE_ENV === 'development';
    const isDebugMode = req.headers['user-agent']?.includes('Chrome') && req.headers.referer?.includes('replay-viewer');
    
    if (!isDevelopment && !isDebugMode) {
        debug.level2('Source file access denied - not in debug mode');
        debug.apiResponse(403, 'Source files not available in production', 2);
        debug.exit('serveSourceDirectory', startTime, 'access_denied', 2);
        return res.status(403).send('Source files not available in production');
    }
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving source file for debugging:', req.path);
    
    // strip /source prefix and construct path from project root
    const relativePath = req.path.replace('/source/', '');
    const sourcePath = path.join(__dirname, '../..', relativePath);
    debug.fileOp('serve_source', sourcePath, 3);
    
    try {
        // check if file exists
        if (fs.existsSync(sourcePath)) {
            debug.level2('Source file found, serving:', path.basename(sourcePath));
            res.setHeader('Content-Type', 'text/plain; charset=utf-8');
            res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
            res.sendFile(sourcePath);
            debug.exit('serveSourceDirectory', startTime, 'success', 2);
        } else {
            debug.level2('Source file not found:', sourcePath);
            debug.apiResponse(404, 'Source file not found', 2);
            debug.exit('serveSourceDirectory', startTime, 'not_found', 2);
            res.status(404).send('Source file not found');
        }
    } catch (error) {
        debug.error('serveSourceDirectory', error, 1);
        debug.apiResponse(500, 'Error serving source file', 1);
        debug.exit('serveSourceDirectory', startTime, 'error', 1);
        res.status(500).send('Error serving source file');
    }
});

app.get('*', (req, res) => {
  const requestedPath = path.join(publicDir, req.path);
  
  // use async instead, should be faster
  fs.promises.access(requestedPath)
    .then(async () => {
      const stats = await fs.promises.stat(requestedPath);
      if (stats.isFile()) {
        res.sendFile(requestedPath);
      } else {
        res.status(404).sendFile(path.join(publicDir, '404.html'));
      }
    })
    .catch(() => {
      res.status(404).sendFile(path.join(publicDir, '404.html'));
    });
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
    const startTime = debugUtils.debugFunctionEntry('initializeServer', [], 1);
    console.log("Starting server initialization...");
    console.debug(2, 'Initializing server components...');

    try {
        const initializationPromise = new Promise((resolve, reject) => {
            if (discordState.isReady) { 
                console.debug(2, 'Discord bot already ready');
                resolve();
            } else {
                console.log('Waiting for Discord bot to be ready...');
                console.debug(2, 'Discord bot not ready, setting up pending initialization');
                global.pendingInitialization = true;
                global.completePendingInitialization = resolve;
            }
        });

        await initializationPromise;
        console.log("Discord bot is ready, proceeding with demo processing...");
        console.debug(2, 'Discord initialization complete, proceeding with demo processing');

        pendingDemos.clear();
        pendingJsons.clear();
        processedJsons.clear();
        console.debug(2, 'Cleared pending collections');

        const demoFiles = await fs.promises.readdir(demoDir);
        console.log(`Found ${demoFiles.length} demo files in the directory.`);
        console.debug(2, `Demo directory scan complete: ${demoFiles.length} files`);
        
        const jsonFiles = await fs.promises.readdir(gameLogsDir);
        console.log(`Found ${jsonFiles.length} JSON files in the directory.`);
        console.debug(2, `JSON directory scan complete: ${jsonFiles.length} files`);
        
        const [rows] = await pool.query('SELECT name, log_file FROM demos');
        console.debug(2, `Database query complete: ${rows.length} existing demos`);
        
        const existingDemos = new Map(rows.map(row => [row.name, row.log_file]));
        const existingJsonFiles = new Set(rows.map(row => row.log_file));

        for (const demoFile of demoFiles) {
            if (demoFile.endsWith('.dcrec')) {
                if (!existingDemos.has(demoFile)) {
                    const demoStats = await fs.promises.stat(path.join(demoDir, demoFile));
                    pendingDemos.set(demoFile, { stats: demoStats, addedTime: Date.now() });
                    console.log(`Existing demo ${demoFile} added to pending list.`);
                    console.debug(3, `Demo file stats for ${demoFile}:`, demoStats);
                } else {
                    console.log(`Demo ${demoFile} already exists in the database. Skipping.`);
                    console.debug(3, `Skipping existing demo: ${demoFile}`);
                }
            }
        }

        for (const jsonFile of jsonFiles) {
            if (jsonFile.endsWith('_full.json')) {
                if (!existingJsonFiles.has(jsonFile)) {
                    pendingJsons.set(jsonFile, { path: path.join(gameLogsDir, jsonFile), addedTime: Date.now() });
                    console.log(`Existing JSON ${jsonFile} added to pending list.`);
                    console.debug(3, `Added JSON to pending: ${jsonFile}`);
                } else {
                    processedJsons.add(jsonFile);
                    console.log(`JSON ${jsonFile} is already linked to a demo in the database. Skipping.`);
                    console.debug(3, `Skipping processed JSON: ${jsonFile}`);
                }
            }
        }

        console.log(`Loaded ${pendingDemos.size} pending demo files.`);
        console.log(`Loaded ${pendingJsons.size} pending JSON files.`);
        console.log(`${existingDemos.size} demos already exist in the database.`);
        console.debug(2, `Initialization summary - Pending demos: ${pendingDemos.size}, Pending JSONs: ${pendingJsons.size}, Existing: ${existingDemos.size}`);

        await checkForMatch();
        console.debug(2, 'Initial demo matching check completed');

        debugUtils.debugFunctionExit('initializeServer', startTime, 'success', 1);
    } catch (error) {
        console.error('Error during server initialization:', error);
        console.debug(1, 'Server initialization failed:', error.message);
        debugUtils.debugFunctionExit('initializeServer', startTime, 'error', 1);
        throw error;
    }
}

const debugLogWatchers = new Map();

function startDebugLogTail(socket) {
  if (debugLogWatchers.has(socket.id)) {
    return; 
  }
  
  const debugFilePath = path.join(__dirname, 'debug.txt');
  
  // check if debug file exists
  if (!fs.existsSync(debugFilePath)) {
    socket.emit('debug_output', 'Debug file not found. Debug messages will appear here when debug level > 0.');
    return;
  }
  
  try {
    // watch for changes to debug file
    const watcher = chokidar.watch(debugFilePath, {
      persistent: true,
      usePolling: true,
      interval: 1000
    });
    
    let lastSize = 0;
    
    try {
      const stats = fs.statSync(debugFilePath);
      lastSize = stats.size;
    } catch (err) {
      console.error('Error getting debug file stats:', err);
    }
    
    watcher.on('change', () => {
      try {
        const stats = fs.statSync(debugFilePath);
        if (stats.size > lastSize) {
          // file has grown, read new content
          const stream = fs.createReadStream(debugFilePath, {
            start: lastSize,
            end: stats.size - 1
          });
          
          let newContent = '';
          stream.on('data', (chunk) => {
            newContent += chunk.toString();
          });
          
          stream.on('end', () => {
            const lines = newContent.split('\n').filter(line => line.trim());
            lines.forEach(line => {
              if (socket.debugLogStream) {
                socket.emit('debug_output', line);
              }
            });
          });
          
          lastSize = stats.size;
        }
      } catch (err) {
        console.error('Error reading debug file changes:', err);
      }
    });
    
    debugLogWatchers.set(socket.id, watcher);
    
    // send last few lines of debug file
    try {
      const content = fs.readFileSync(debugFilePath, 'utf8');
      const lines = content.split('\n').filter(line => line.trim());
      const recentLines = lines.slice(-10);
      recentLines.forEach(line => {
        if (socket.debugLogStream) {
          socket.emit('debug_output', line);
        }
      });
    } catch (err) {
      console.error('Error reading initial debug file content:', err);
    }
    
  } catch (err) {
    console.error('Error setting up debug log watcher:', err);
    socket.emit('debug_output', 'Error setting up debug log monitoring.');
  }
}

function stopDebugLogTail(socket) {
  const watcher = debugLogWatchers.get(socket.id);
  if (watcher) {
    watcher.close();
    debugLogWatchers.delete(socket.id);
  }
}

// route for the server console output
io.on('connection', (socket) => {
  console.log('A user connected to console output');
  
  // handle logstream controls
  socket.on('start_server_log_stream', () => {
    socket.serverLogStream = true;
    console.log('Server log stream started for client');
  });
  
  socket.on('stop_server_log_stream', () => {
    socket.serverLogStream = false;
    console.log('Server log stream stopped for client');
  });
  
  socket.on('start_debug_log_stream', () => {
    socket.debugLogStream = true;
    console.log('Debug log stream started for client');
    // start tailing debug file
    startDebugLogTail(socket);
  });
  
  socket.on('stop_debug_log_stream', () => {
    socket.debugLogStream = false;
    console.log('Debug log stream stopped for client');
    // stop tailing debug file
    stopDebugLogTail(socket);
  });
  
  socket.on('disconnect', () => {
    console.log('Client disconnected from console output');
    stopDebugLogTail(socket);
  });
});

// override console.log for logging
const originalLog = console.log;
console.log = function () {
  const timestamp = formatTimestamp(new Date());
  const args = Array.from(arguments);
  const message = `[${timestamp}] ${args.join(' ')}`;

  logStream.write(message + '\n');

  // only enable for the admin panel log stream
  io.sockets.sockets.forEach((socket) => {
    if (socket.serverLogStream) {
      socket.emit('console_output', message);
    }
  });

  originalLog.apply(console, [timestamp, ...args]);
};

// check if we have lost connection to the database
pool.getConnection((err, connection) => {
    if (err) {
        console.error('Error connecting to the database:', err);
    } else {
        console.log('Successfully connected to the database');
        connection.release();
    }
});

// interval to prevent pending verification tokens from forever being alone
setInterval(() => {
    for (let [email, { verificationToken }] of pendingVerifications) {
        try {
            jwt.verify(verificationToken, JWT_SECRET);
        } catch (error) {
            pendingVerifications.delete(email);
        }
    }
}, 3600000); // cleans up every hour

// shutdown handler
process.on('SIGINT', async () => {
    console.log('Shutting down...');
    discordState.isReady = false; 
    global.pendingInitialization = null;
    global.completePendingInitialization = null;

    logStream.end(() => {
        console.log('Log file stream closed.');
        process.exit(0);
    });
});

app.use(errorHandler);


const server = http.listen(port, async () => {
    console.log(`Defcon Expanded Demo and File Server Listening at http://localhost:${port}`);
    console.log(`Server started at: ${new Date().toISOString()}`);
    console.log(`Environment: ${process.env.NODE_ENV || 'development'}`);
    console.log(`Node.js version: ${process.version}`);
    console.log(`Platform: ${process.platform}`);
    
    // set server instance for debug utilities
    debugUtils.setServerInstance(server);
    
    try {
      await initializeServer();
      console.log("Server initialization completed successfully.");
    } catch (error) {
      console.error("Assuming you broke something? - ", error);
    }
  });
  