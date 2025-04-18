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

const express = require('express');
const router = express.Router();

const {
    pool
} = require('../../constants');

const {
    authenticateToken,
}   = require('../../authentication')

router.post('/api/mods/:id/like', authenticateToken, async (req, res) => {
    const modId = req.params.id;
    const userId = req.user.id;

    try {

        const [existingLike] = await pool.query('SELECT * FROM mod_likes WHERE mod_id = ? AND user_id = ?', [modId, userId]);
        if (existingLike.length > 0) {
            return res.status(400).json({ error: 'User has already liked this mod.' });
        }

        await pool.query('INSERT INTO mod_likes (mod_id, user_id) VALUES (?, ?)', [modId, userId]);
        await pool.query('UPDATE modlist SET likes_count = likes_count + 1 WHERE id = ?', [modId]);

        console.log(`User ${req.user.username} (ID: ${userId}) liked mod ${modId}`);

        res.status(200).json({ message: 'Mod liked!' });
    } catch (error) {
        console.error('Error liking mod:', error);
        res.status(500).json({ error: 'Unable to like mod' });
    }
});


router.post('/api/mods/:id/favorite', authenticateToken, async (req, res) => {
    const modId = req.params.id;
    const userId = req.user.id;

    try {

        const [existingFavorite] = await pool.query('SELECT * FROM mod_favorites WHERE mod_id = ? AND user_id = ?', [modId, userId]);
        if (existingFavorite.length > 0) {
            return res.status(400).json({ error: 'User has already favorited this mod.' });
        }

        await pool.query('INSERT INTO mod_favorites (mod_id, user_id) VALUES (?, ?)', [modId, userId]);
        await pool.query('UPDATE modlist SET favorites_count = favorites_count + 1 WHERE id = ?', [modId]);

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

module.exports = router;