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


const debug = require('../../debug-helpers');

router.get('/api/mods', async (req, res) => {
    const startTime = debug.enter('getMods', [req.query], 1);
    try {
        const { type, sort, search } = req.query;
        debug.level2('Mods query parameters:', { type, sort, search });

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
            debug.level3('Type filter applied:', type);
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
                debug.level3('Sorting by most downloaded');
                break;
            case 'latest':
                query += ' ORDER BY m.release_date DESC';
                debug.level3('Sorting by latest');
                break;
            case 'most-liked':
                query += ' ORDER BY likes_count DESC';
                debug.level3('Sorting by most liked');
                break;
            case 'most-favorited':
                query += ' ORDER BY favorites_count DESC';
                debug.level3('Sorting by most favorited');
                break;
            default:
                query += ' ORDER BY m.download_count DESC';
                debug.level3('Using default sort (most downloaded)');
        }

        debug.dbQuery(query, params, 2);
        const [rows] = await pool.query(query, params);
        debug.dbResult(rows, 2);

        let userLikesAndFavorites = { likes: [], favorites: [] };
        if (req.user) {
            debug.level3('Fetching user likes and favorites for user:', req.user.username);
            userLikesAndFavorites = await getUserLikesAndFavorites(req.user.id);
            debug.level3('User likes/favorites:', { 
                likes: userLikesAndFavorites.likes.length, 
                favorites: userLikesAndFavorites.favorites.length 
            });
        }

        let results;
        if (search) {
            debug.level2('Applying search filter:', search);
            const searchTerms = search.toLowerCase().split(' ');
            results = rows.filter(mod =>
                searchTerms.every(term =>
                    fuzzyMatch(term, mod.name || '') ||
                    fuzzyMatch(term, mod.creator || '') ||
                    fuzzyMatch(term, mod.description || '')
                )
            );
            debug.level2('Search results:', { original: rows.length, filtered: results.length });
        } else {
            results = rows;
        }

        const modsWithUserInfo = results.map(mod => ({
            ...mod,
            isLiked: userLikesAndFavorites.likes.includes(mod.id),
            isFavorited: userLikesAndFavorites.favorites.includes(mod.id)
        }));

        debug.level2('Returning mods:', modsWithUserInfo.length);
        debug.exit('getMods', startTime, { modsCount: modsWithUserInfo.length }, 1);
        res.json(modsWithUserInfo);
    } catch (error) {
        debug.error('getMods', error, 1);
        debug.exit('getMods', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch mods', details: error.message });
    }
});

router.get('/api/download-mod/:id', removeTimeout, async (req, res) => {
    const startTime = debug.enter('downloadMod', [req.params.id], 1);
    debug.level2('Mod download request for ID:', req.params.id);
    
    try {
        debug.dbQuery('SELECT * FROM modlist WHERE id = ?', [req.params.id], 2);
        const [mod] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
        debug.dbResult(mod, 2);
        
        if (mod.length === 0) {
            debug.level1('Mod not found for ID:', req.params.id);
            debug.exit('downloadMod', startTime, 'not_found', 1);
            return res.status(404).json({ error: 'Mod not found' });
        }

        const modPath = path.join(rootDir, mod[0].file_path);
        debug.fileOp('check', modPath, 3);

        if (!fs.existsSync(modPath)) {
            debug.level1('Mod file not found on disk:', modPath);
            debug.exit('downloadMod', startTime, 'file_not_found', 1);
            return res.status(404).json({ error: 'Mod file not found' });
        }

        debug.level3('Incrementing download count for mod:', mod[0].name);
        await pool.query('UPDATE modlist SET download_count = download_count + 1 WHERE id = ?', [req.params.id]);

        const downloadFilename = path.basename(mod[0].file_path);
        debug.level2('Starting mod download:', downloadFilename);

        res.download(modPath, downloadFilename, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    debug.level2(`Mod download aborted by client with IP: ${clientIp}`);
                } else {
                    debug.error('downloadMod', err, 1);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading mod');
                    }
                }
            } else {
                debug.level2(`Mod downloaded successfully (${downloadFilename}) by client with IP: ${clientIp}`);
                debug.exit('downloadMod', startTime, 'success', 1);
            }
        });

    } catch (error) {
        debug.error('downloadMod', error, 1);
        debug.exit('downloadMod', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error' });
    }
});

module.exports = router;