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

const { 
    pool 
} = require('./constants');

const removeTimeout = (req, res, next) => {
    req.setTimeout(0);
    res.setTimeout(0);
    next();
};

function getClientIp(req) {
    const forwardedFor = req.headers['x-forwarded-for'];
    if (forwardedFor) {
        return forwardedFor.split(',')[0].trim();
    }
    return (req.ip || req.connection.remoteAddress).replace(/^::ffff:/, '');
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

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

module.exports = {
    removeTimeout,
    getUserLikesAndFavorites,
    getClientIp,
    fuzzyMatch,
    levenshteinDistance,
    formatTimestamp,
    delay
};