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
const path = require('path');
const fs = require('fs');

const {
    pool,
    rootDir
} = require('../../constants');

const {
    removeTimeout,
    getUserLikesAndFavorites,
    fuzzyMatch
}   = require('../../shared-functions')

router.get('/api/mods', async (req, res) => {
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
                query += ' ORDER BY m.download_count DESC';
        }

        const [rows] = await pool.query(query, params);

        let userLikesAndFavorites = { likes: [], favorites: [] };
        if (req.user) {
            userLikesAndFavorites = await getUserLikesAndFavorites(req.user.id);
        }

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


router.get('/api/download-mod/:id', removeTimeout, async (req, res) => {
    try {
        const [mod] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
        if (mod.length === 0) {
            console.log(`Mod not found: ID ${req.params.id}`);
            return res.status(404).json({ error: 'Mod not found' });
        }

        const modPath = path.join(rootDir, mod[0].file_path);
        console.log(`Attempting to download mod: ${modPath}`);

        if (!fs.existsSync(modPath)) {
            console.error(`Mod file not found: ${modPath}`);
            return res.status(404).json({ error: 'Mod file not found' });
        }

        await pool.query('UPDATE modlist SET download_count = download_count + 1 WHERE id = ?', [req.params.id]);

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

module.exports = router;