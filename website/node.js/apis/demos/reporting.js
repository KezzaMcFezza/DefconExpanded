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
    authenticateToken
}   = require('../../authentication')


const debug = require('../../debug-helpers');

router.post('/api/report-demo', authenticateToken, async (req, res) => {
    const startTime = debug.enter('reportDemo', [req.body.demoId, req.body.reportType, req.user.username], 1);
    const { demoId, reportType } = req.body;
    const userId = req.user.id;

    debug.level2('Demo report submission:', { demoId, reportType, userId, username: req.user.username });

    try {
        debug.level3('Inserting demo report into database');
        debug.dbQuery('INSERT INTO demo_reports', [demoId, userId, reportType], 2);
        await pool.query(
            'INSERT INTO demo_reports (demo_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
            [demoId, userId, reportType]
        );
        
        debug.level2('Demo report submitted successfully:', { demoId, reportType, by: req.user.username });
        debug.exit('reportDemo', startTime, 'success', 1);
        res.json({ message: 'Report submitted successfully' });
    } catch (error) {
        debug.error('reportDemo', error, 1);
        debug.exit('reportDemo', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to submit report' });
    }
});

module.exports = router;