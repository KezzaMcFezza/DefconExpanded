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

router.get('/api/earliest-game-date', async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT MIN(date) as earliestDate FROM demos');

        if (rows.length > 0 && rows[0].earliestDate) {
            res.json({ earliestDate: rows[0].earliestDate });
        } else {
            res.json({ earliestDate: null });
        }
    } catch (error) {
        console.error('Error fetching earliest game date:', error);
        res.status(500).json({ error: 'Unable to fetch earliest game date' });
    }
});

module.exports = router;