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

const fs = require('fs');
const debugUtils = require('./debug-utils');
const { 
    pool 
} = require('./constants');

const removeTimeout = (req, res, next) => {
    req.setTimeout(0);
    res.setTimeout(0);
    next();
};

function getClientIp(req) {
    try {
        const forwardedFor = req.headers['x-forwarded-for'];
        if (forwardedFor) {
            return forwardedFor.split(',')[0].trim();
        }
        return (req.ip || req.connection.remoteAddress).replace(/^::ffff:/, '');
    } catch (error) {
        console.error('Error getting client IP:', error);
        return 'unknown';
    }
}

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

const fuzzyMatch = (needle, haystack, threshold = 0.3) => {
    const needleLower = needle.toLowerCase();
    const haystackLower = haystack.toLowerCase();

    if (haystackLower.includes(needleLower)) return true;

    const distance = levenshteinDistance(needleLower, haystackLower);
    const maxLength = Math.max(needleLower.length, haystackLower.length);
    return distance / maxLength <= threshold;
};

function formatTimestamp(date) {
    try {
        const pad = (num) => (num < 10 ? '0' + num : num);

        const year = date.getFullYear();
        const month = pad(date.getMonth() + 1);
        const day = pad(date.getDate());
        const hours = pad(date.getHours());
        const minutes = pad(date.getMinutes());
        const seconds = pad(date.getSeconds());
        const milliseconds = pad(Math.floor(date.getMilliseconds() / 10));

        return `${year}-${month}-${day}-${hours}:${minutes}:${seconds}.${milliseconds}`;
    } catch (error) {
        console.error('Error formatting timestamp:', error);
        return new Date().toISOString();
    }
}

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// helper function to set shared array buffer headers for WebAssembly files
function setSharedArrayBufferHeaders(res, filePath, stat) {
    res.set('Cross-Origin-Opener-Policy', 'same-origin');
    res.set('Cross-Origin-Embedder-Policy', 'require-corp');
    res.set('Cross-Origin-Resource-Policy', 'cross-origin');
    
    // Set specific content types for WebAssembly files
    if (filePath.endsWith('.wasm')) {
        res.set('Content-Type', 'application/wasm');
    } else if (filePath.endsWith('.wasm.map')) {
        res.set('Content-Type', 'application/json');
    } else if (filePath.endsWith('.js')) {
        res.set('Content-Type', 'application/javascript');
    } else if (filePath.endsWith('.html')) {
        res.set('Content-Type', 'text/html');
    } else if (filePath.endsWith('.css')) {
        res.set('Content-Type', 'text/css');
    } else if (filePath.endsWith('.png')) {
        res.set('Content-Type', 'image/png');
    } else if (filePath.endsWith('.jpg') || filePath.endsWith('.jpeg')) {
        res.set('Content-Type', 'image/jpeg');
    } else if (filePath.endsWith('.svg')) {
        res.set('Content-Type', 'image/svg+xml');
    } else if (filePath.endsWith('.ico')) {
        res.set('Content-Type', 'image/x-icon');
    } else if (filePath.endsWith('.data')) {
        res.set('Content-Type', 'application/octet-stream');
    } else if (filePath.endsWith('.worker.js')) {
        res.set('Content-Type', 'application/javascript');
    }
}

// generic function to find Emscripten build files (replay viewer, sync practice, etc.)
function findEmscriptenFiles(directory, filePrefix) {
    const startTime = debugUtils.debugFunctionEntry('findEmscriptenFiles', [directory, filePrefix], 2);
    
    try {
        const files = fs.readdirSync(directory);
        const emscriptenFiles = {
            html: null,
            js: null,
            wasm: null,
            wasmMap: null,
            data: null,
            version: null
        };
        
        // pattern matches files like: replay_viewer_1.0.0.html, sync_practice_2.0.1.wasm, etc.
        const filePattern = new RegExp(`^${filePrefix}_(.+)\\.(.+)$`);
        
        for (const file of files) {
            const match = file.match(filePattern);
            if (match) {
                const version = match[1];
                const extension = match[2];
                
                if (!emscriptenFiles.version) {
                    emscriptenFiles.version = version;
                }
                
                switch (extension) {
                    case 'html':
                        emscriptenFiles.html = file;
                        break;
                    case 'js':
                        emscriptenFiles.js = file;
                        break;
                    case 'wasm':
                        emscriptenFiles.wasm = file;
                        break;
                    case 'wasm.map':
                        emscriptenFiles.wasmMap = file;
                        break;
                    case 'data':
                        emscriptenFiles.data = file;
                        break;
                }
            }
        }
        
        debugUtils.debugFunctionExit('findEmscriptenFiles', startTime, emscriptenFiles, 2);
        return emscriptenFiles;
        
    } catch (error) {
        console.error(`Error finding ${filePrefix} files:`, error);
        debugUtils.debugFunctionExit('findEmscriptenFiles', startTime, 'error', 2);
        return { html: null, js: null, wasm: null, wasmMap: null, data: null, version: null };
    }
}

module.exports = {
    removeTimeout,
    getUserLikesAndFavorites,
    getClientIp,
    fuzzyMatch,
    levenshteinDistance,
    formatTimestamp,
    delay,
    setSharedArrayBufferHeaders,
    findEmscriptenFiles
};