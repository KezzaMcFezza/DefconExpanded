const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

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