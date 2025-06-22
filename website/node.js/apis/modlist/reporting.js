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

router.post('/api/report-mod', authenticateToken, async (req, res) => {
    const startTime = debug.enter('reportMod', [req.body.modId, req.body.reportType, req.user.username], 1);
    const { modId, reportType } = req.body;
    const userId = req.user.id;

    debug.level2('Mod report submission:', { modId, reportType, userId, username: req.user.username });

    try {
        debug.level3('Inserting mod report into database');
        debug.dbQuery('INSERT INTO mod_reports', [modId, userId, reportType], 2);
        await pool.query(
            'INSERT INTO mod_reports (mod_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
            [modId, userId, reportType]
        );
        
        debug.level2('Mod report submitted successfully:', { modId, reportType, by: req.user.username });
        debug.exit('reportMod', startTime, 'success', 1);
        res.json({ message: 'Mod report submitted successfully' });
    } catch (error) {
        debug.error('reportMod', error, 1);
        debug.exit('reportMod', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to submit mod report' });
    }
});

module.exports = router;