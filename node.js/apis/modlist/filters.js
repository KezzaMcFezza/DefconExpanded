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

router.get('/api/search-mods', async (req, res) => {
    const searchTerm = req.query.term;
    try {
        const [rows] = await pool.query('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`]);
        res.json(rows);
    } catch (error) {
        console.error('Error searching mods:', error);
        res.status(500).json({ error: 'Unable to search mods' });
    }
});

router.get('/api/sort-mods', async (req, res) => {
    const sortType = req.query.type;
    let query = 'SELECT * FROM modlist';

    if (sortType === 'most-downloaded') {
        query += ' ORDER BY download_count DESC';
    } else if (sortType === 'latest') {
        query += ' ORDER BY release_date DESC';
    }

    try {
        const [rows] = await pool.query(query);
        res.json(rows);
    } catch (error) {
        console.error('Error sorting mods:', error);
        res.status(500).json({ error: 'Unable to sort mods' });
    }
});

module.exports = router;