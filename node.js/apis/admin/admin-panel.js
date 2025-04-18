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
const path = require('path');
const jwt = require('jsonwebtoken');
const startTime = Date.now();
const JWT_SECRET = process.env.JWT_SECRET;

const {
    pool,
    rootDir
} = require('../../constants');

const {
    authenticateToken,
    checkAuthToken,
    checkRole
}   = require('../../authentication')

router.get('/admin-panel', authenticateToken, (req, res) => {
    if (req.user) {
        res.sendFile(path.join(__dirname, 'public', 'admin-panel.html'));
    } else {
        res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
    }
});

router.get('/api/monitoring-data', authenticateToken, async (req, res) => {
    try {
        const uptime = Math.floor((Date.now() - startTime) / 1000);

        const [totalDemosResult] = await pool.query('SELECT COUNT(*) as count FROM demos');
        const totalDemos = totalDemosResult[0].count;

        const [pendingRequestsResult] = await pool.query(`
      SELECT 
        (SELECT COUNT(*) FROM demo_reports WHERE status = 'pending') +
        (SELECT COUNT(*) FROM mod_reports WHERE status = 'pending') as total_pending
    `);
        const userRequests = pendingRequestsResult[0].total_pending;

        res.json({
            uptime,
            totalDemos,
            userRequests,
        });
    } catch (error) {
        console.error('Error fetching monitoring data:', error);
        res.status(500).json({ error: 'Unable to fetch monitoring data' });
    }
});

router.get('/api/checkAuth', (req, res) => {
    const token = req.cookies.token;
    if (token) {
        jwt.verify(token, JWT_SECRET, (err, user) => {
            if (err) {
                res.json({ isLoggedIn: false });
            } else {
                res.json({ isLoggedIn: true, username: user.username });
            }
        });
    } else {
        res.json({ isLoggedIn: false });
    }
});

router.get('/download-logs', checkAuthToken, (req, res) => {
    const logPath = path.join(rootDir, 'server.log');
    res.download(logPath, 'server.log', (err) => {
        if (err) {
            console.error('Error downloading log file:', err);
            res.status(500).send('Error downloading log file');
        }
    });
});

router.get('/api/pending-reports', authenticateToken, checkRole(5), async (req, res) => {
    try {
        const [reports] = await pool.query(`
            SELECT dr.*, d.game_type, d.players, u.username 
            FROM demo_reports dr 
            JOIN demos d ON dr.demo_id = d.id 
            JOIN users u ON dr.user_id = u.id 
            WHERE dr.status = 'pending'
        `);
        res.json(reports);
    } catch (error) {
        console.error('Error fetching pending reports:', error);
        res.status(500).json({ error: 'Failed to fetch pending reports' });
    }
});

router.put('/api/resolve-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
    const { reportId } = req.params;
    try {
        await pool.query('UPDATE demo_reports SET status = "resolved" WHERE id = ?', [reportId]);
        res.json({ message: 'Report resolved successfully' });
    } catch (error) {
        console.error('Error resolving report:', error);
        res.status(500).json({ error: 'Failed to resolve report' });
    }
});

router.get('/api/pending-reports-count', authenticateToken, async (req, res) => {
    try {
        const [result] = await pool.query('SELECT COUNT(*) as count FROM demo_reports WHERE status = "pending"');
        console.log('Pending reports count from database:', result[0].count);
        res.json({ count: result[0].count });
    } catch (error) {
        console.error('Error fetching pending reports count:', error);
        res.status(500).json({ error: 'Failed to fetch pending reports count' });
    }
});

module.exports = router;