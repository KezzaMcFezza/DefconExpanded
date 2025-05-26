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
//Last Edited 25-05-2025

const express = require('express');
const router = express.Router();

const {
    pool
} = require('../../constants');

const {
    authenticateToken,
}   = require('../../authentication')


const debug = require('../../debug-helpers');

router.post('/api/mods/:id/like', authenticateToken, async (req, res) => {
    const startTime = debug.enter('likeMod', [req.params.id, req.user.username], 1);
    const modId = req.params.id;
    const userId = req.user.id;

    debug.level2('Mod like request:', { modId, userId, username: req.user.username });

    try {
        debug.level3('Checking for existing like');
        debug.dbQuery('SELECT * FROM mod_likes WHERE mod_id = ? AND user_id = ?', [modId, userId], 2);
        const [existingLike] = await pool.query('SELECT * FROM mod_likes WHERE mod_id = ? AND user_id = ?', [modId, userId]);
        debug.dbResult(existingLike, 2);
        
        if (existingLike.length > 0) {
            debug.level1('User has already liked this mod:', { modId, userId });
            debug.exit('likeMod', startTime, 'already_liked', 1);
            return res.status(400).json({ error: 'User has already liked this mod.' });
        }

        debug.level3('Adding like to database');
        await pool.query('INSERT INTO mod_likes (mod_id, user_id) VALUES (?, ?)', [modId, userId]);
        await pool.query('UPDATE modlist SET likes_count = likes_count + 1 WHERE id = ?', [modId]);

        debug.level2(`User ${req.user.username} (ID: ${userId}) liked mod ${modId}`);
        debug.exit('likeMod', startTime, 'success', 1);
        res.status(200).json({ message: 'Mod liked!' });
    } catch (error) {
        debug.error('likeMod', error, 1);
        debug.exit('likeMod', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to like mod' });
    }
});

router.post('/api/mods/:id/favorite', authenticateToken, async (req, res) => {
    const startTime = debug.enter('favoriteMod', [req.params.id, req.user.username], 1);
    const modId = req.params.id;
    const userId = req.user.id;

    debug.level2('Mod favorite request:', { modId, userId, username: req.user.username });

    try {
        debug.level3('Checking for existing favorite');
        debug.dbQuery('SELECT * FROM mod_favorites WHERE mod_id = ? AND user_id = ?', [modId, userId], 2);
        const [existingFavorite] = await pool.query('SELECT * FROM mod_favorites WHERE mod_id = ? AND user_id = ?', [modId, userId]);
        debug.dbResult(existingFavorite, 2);
        
        if (existingFavorite.length > 0) {
            debug.level1('User has already favorited this mod:', { modId, userId });
            debug.exit('favoriteMod', startTime, 'already_favorited', 1);
            return res.status(400).json({ error: 'User has already favorited this mod.' });
        }

        debug.level3('Adding favorite to database');
        await pool.query('INSERT INTO mod_favorites (mod_id, user_id) VALUES (?, ?)', [modId, userId]);
        await pool.query('UPDATE modlist SET favorites_count = favorites_count + 1 WHERE id = ?', [modId]);

        debug.level3('Updating user profile favorites');
        const [userProfile] = await pool.query('SELECT favorites FROM user_profiles WHERE user_id = ?', [userId]);
        let currentFavorites = userProfile[0].favorites ? userProfile[0].favorites.split(',') : [];

        currentFavorites.push(modId);

        await pool.query('UPDATE user_profiles SET favorites = ? WHERE user_id = ?', [currentFavorites.join(','), userId]);
        debug.level2(`User ${req.user.username} (ID: ${userId}) favorited mod ${modId}`);
        debug.exit('favoriteMod', startTime, 'success', 1);

        res.status(200).json({ message: 'Mod favorited and user profile updated!' });
    } catch (error) {
        debug.error('favoriteMod', error, 1);
        debug.exit('favoriteMod', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to favorite mod' });
    }
});

module.exports = router;