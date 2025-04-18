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

const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const multer = require('multer');
const crypto = require('crypto'); 
const nodemailer = require('nodemailer');
const rootDir = path.join(__dirname, '..');
const publicDir = path.join(rootDir, 'public');
const demoDir = path.join(rootDir, 'demo_recordings');
const resourcesDir = path.join(publicDir, 'Files');
const dedconBuildsDir = path.join(publicDir, 'Files');
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

[demoDir, resourcesDir, dedconBuildsDir, uploadDir, gameLogsDir, modlistDir, modPreviewsDir].forEach(dir => {
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
    publicDir,
    gameLogsDir
};