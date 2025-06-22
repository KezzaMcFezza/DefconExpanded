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

router.get('/api/earliest-game-date', async (req, res) => {
    const startTime = debug.enter('getEarliestGameDate', [], 1);
    debug.level2('Earliest game date request');
    
    try {
        debug.dbQuery('SELECT MIN(date) as earliestDate FROM demos', [], 2);
        const [rows] = await pool.query('SELECT MIN(date) as earliestDate FROM demos');
        debug.dbResult(rows, 2);

        if (rows.length > 0 && rows[0].earliestDate) {
            debug.level2('Returning earliest date:', rows[0].earliestDate);
            debug.exit('getEarliestGameDate', startTime, { earliestDate: rows[0].earliestDate }, 1);
            res.json({ earliestDate: rows[0].earliestDate });
        } else {
            debug.level2('No earliest date found');
            debug.exit('getEarliestGameDate', startTime, 'no_date', 1);
            res.json({ earliestDate: null });
        }
    } catch (error) {
        debug.error('getEarliestGameDate', error, 1);
        debug.exit('getEarliestGameDate', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch earliest game date' });
    }
});

module.exports = router;