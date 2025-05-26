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


const debug = require('../../debug-helpers');

router.get('/api/search-mods', async (req, res) => {
    const startTime = debug.enter('searchMods', [req.query.term], 1);
    const searchTerm = req.query.term;
    
    debug.level2('Mod search request:', { searchTerm });
    
    try {
        debug.dbQuery('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`], 2);
        const [rows] = await pool.query('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`]);
        debug.dbResult(rows, 2);
        
        debug.level2('Search results:', { searchTerm, resultsCount: rows.length });
        debug.exit('searchMods', startTime, { resultsCount: rows.length }, 1);
        res.json(rows);
    } catch (error) {
        debug.error('searchMods', error, 1);
        debug.exit('searchMods', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to search mods' });
    }
});

router.get('/api/sort-mods', async (req, res) => {
    const startTime = debug.enter('sortMods', [req.query.type], 1);
    const sortType = req.query.type;
    let query = 'SELECT * FROM modlist';

    debug.level2('Mod sort request:', { sortType });

    if (sortType === 'most-downloaded') {
        query += ' ORDER BY download_count DESC';
        debug.level3('Sorting by most downloaded');
    } else if (sortType === 'latest') {
        query += ' ORDER BY release_date DESC';
        debug.level3('Sorting by latest');
    } else {
        debug.level3('Using default sort (no specific order)');
    }

    try {
        debug.dbQuery(query, [], 2);
        const [rows] = await pool.query(query);
        debug.dbResult(rows, 2);
        
        debug.level2('Sort results:', { sortType, resultsCount: rows.length });
        debug.exit('sortMods', startTime, { resultsCount: rows.length }, 1);
        res.json(rows);
    } catch (error) {
        debug.error('sortMods', error, 1);
        debug.exit('sortMods', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to sort mods' });
    }
});

module.exports = router;