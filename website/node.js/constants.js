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
//Last Edited 04-08-2025

const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const multer = require('multer');
const crypto = require('crypto'); 
const nodemailer = require('nodemailer');
const rootDir = path.join(__dirname, '..');
const publicDir = path.join(rootDir, 'public');
const demoDir = path.join(rootDir, 'demo_recordings');
const replayViewerDir = path.join(rootDir, 'replay_viewer');
const syncPracticeDir = path.join(rootDir, 'sync_practice');
const defconDir = path.join(rootDir, 'defcon');
const resourcesDir = path.join(publicDir, 'Files');
const dedconBuildsDir = path.join(publicDir, 'Files');
const replayBuildsDir = path.join(publicDir, 'Files');
const uploadDir = publicDir;
const gameLogsDir = path.join(rootDir, 'game_logs');
const modlistDir = path.join(publicDir, 'modlist');
const modPreviewsDir = path.join(publicDir, 'modpreviews');

const pool = mysql.createPool({
    host: process.env.DB_HOST,
    user: process.env.DB_USER,
    password: process.env.DB_PASSWORD,
    database: process.env.DB_NAME,
    connectionLimit: 20,
    queueLimit: 0,
    waitForConnections: true,
    enableKeepAlive: true,
    keepAliveInitialDelay: 10000,
    connectTimeout: 10000,
});

let transporter = nodemailer.createTransport({
    host: process.env.EMAIL_HOST,
    port: process.env.EMAIL_PORT,
    secure: false,
    auth: {
        user: process.env.EMAIL_USER,
        pass: process.env.EMAIL_PASSWORD
    }
});

[demoDir, resourcesDir, dedconBuildsDir, replayBuildsDir, uploadDir, gameLogsDir, modlistDir, modPreviewsDir].forEach(dir => {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
});

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
        } else if (file.fieldname === 'replayBuildsFile') {
            cb(null, replayBuildsDir);
        } else if (file.fieldname === 'modFile') {
            cb(null, modlistDir);
        } else if (file.fieldname === 'previewImage') {
            cb(null, modPreviewsDir);
        } else if (file.fieldname === 'image') {
            cb(null, path.join(rootDir, 'public', 'uploads'));
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

const upload = multer({ storage: storage });

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
    'replayviewermanagment.html',
    'servermanagment.html',
    'rcon-console.html'
];

const configFiles = [
    'DefconExpanded Test Server.txt',
    'DefconExpanded 1v1 Totally Random.txt',
    'DefconExpanded 1v1 Default.txt',
    'DefconExpanded 1v1 Best Setups Only.txt',
    'DefconExpanded 1v1 Best Setups Only Second Server.txt',
    'DefconExpanded 1v1 Cursed Setups Only.txt',
    'DefconExpanded 1v1 Lots of Units.txt',
    'DefconExpanded 1v1 UK and Ireland.txt',
    'DefconExpanded 1v1 Totally Random - Second Server.txt',
    'MURICON 1v1 Default 2.8.15.txt',
    'DefconExpanded 2v2 Totally Random.txt',
    'DefconExpanded 2v2 UK and Ireland.txt',
    'DefconExpanded 2v2 Tournament.txt',
    'DefconExpanded 2v2 Max Cities - Pop.txt',
    'MURICON 1v1 Totally Random 2.8.15.txt',
    '509 CG 1v1 Totally Random 2.8.15.txt',
    '509 CG 2v2 Totally Random 2.8.15.txt',
    'Raizer\'s Russia vs USA Totally Random.txt',
    'Sony and Hoov\'s Hideout.txt',
    'New Players Welcome Come and Play.txt',
    'DefconExpanded 3v3 Totally Random.txt',
    'DefconExpanded 3v3 Totally Random Second Server.txt',
    'DefconExpanded 3v3 Totally Random - Second Server.txt',
    'MURICON UK Mod.txt',
    'DefconExpanded Free For All Random Cities.txt',
    'DefconExpanded Training.txt',
    'DefconExpanded Diplomacy UK and Ireland.txt',
    'DefconExpanded 4v4 Totally Random.txt',
    'DefconExpanded 5v5 Totally Random.txt',
    'DefconExpanded 8 Player Diplomacy.txt',
    'DefconExpanded 10 Player Diplomacy.txt',
    'DefconExpanded 16 Player Test Server.txt'
];

const DISCORD_CONFIG = {
    token: process.env.DISCORD_BOT_TOKEN,
    channelIds: process.env.DISCORD_CHANNEL_IDS.split(',')
};

module.exports = {
    pool,
    transporter,
    demoDir,
    resourcesDir,
    dedconBuildsDir,
    replayBuildsDir,
    uploadDir,
    gameLogsDir,
    modlistDir,
    modPreviewsDir,
    adminPages,
    configFiles,
    DISCORD_CONFIG,
    storage,
    upload,
    rootDir,
    replayViewerDir,
    syncPracticeDir,
    defconDir,
    publicDir,
    gameLogsDir
};