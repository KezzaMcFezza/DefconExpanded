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

router.post('/api/report-mod', authenticateToken, async (req, res) => {
    const { modId, reportType } = req.body;
    const userId = req.user.id;

    try {
        await pool.query(
            'INSERT INTO mod_reports (mod_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
            [modId, userId, reportType]
        );
        res.json({ message: 'Mod report submitted successfully' });
    } catch (error) {
        console.error('Error submitting mod report:', error);
        res.status(500).json({ error: 'Failed to submit mod report' });
    }
});

module.exports = router;