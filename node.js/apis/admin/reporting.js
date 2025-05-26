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
    checkPermission
} = require('../../authentication')

const permissions = require('../../permission-index');


const debug = require('../../debug-helpers');


router.get('/api/pending-mod-reports', authenticateToken, checkPermission(permissions.ADMIN_VIEW_REPORTS), async (req, res) => {
    try {
        const [reports] = await pool.query(`
          SELECT mr.*, m.name as mod_name, u.username 
          FROM mod_reports mr 
          JOIN modlist m ON mr.mod_id = m.id 
          JOIN users u ON mr.user_id = u.id 
          WHERE mr.status = 'pending'
      `);
        res.json(reports);
    } catch (error) {
        console.error('Error fetching pending mod reports:', error);
        res.status(500).json({ error: 'Failed to fetch pending mod reports' });
    }
});

router.put('/api/resolve-mod-report/:reportId', authenticateToken, checkPermission(permissions.ADMIN_VIEW_REPORTS), async (req, res) => {
    const { reportId } = req.params;
    try {
        await pool.query('UPDATE mod_reports SET status = "resolved" WHERE id = ?', [reportId]);
        res.json({ message: 'Mod report resolved successfully' });
    } catch (error) {
        console.error('Error resolving mod report:', error);
        res.status(500).json({ error: 'Failed to resolve mod report' });
    }
});

module.exports = router;