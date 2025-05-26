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
    checkPermission
} = require('../../authentication')

const permissions = require('../../permission-index');

const debug = require('../../debug-helpers');

router.get('/admin-panel', authenticateToken, (req, res) => {
    const startTime = debug.enter('serveAdminPanel', [req.user.username], 1);
    debug.level2('Admin panel access by:', req.user.username);
    
    if (req.user) {
        debug.level3('Serving admin panel HTML file');
        debug.exit('serveAdminPanel', startTime, 'success', 1);
        res.sendFile(path.join(__dirname, 'public', 'admin-panel.html'));
    } else {
        debug.level1('Unauthorized admin panel access attempt');
        debug.exit('serveAdminPanel', startTime, 'unauthorized', 1);
        res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
    }
});

router.get('/api/monitoring-data', authenticateToken, async (req, res) => {
    const startTime = debug.enter('getMonitoringData', [req.user?.username], 1);
    try {
        debug.level2('Fetching monitoring data for admin panel');
        const uptime = Math.floor((Date.now() - startTime) / 1000);

        debug.level3('Querying total demos count');
        debug.dbQuery('SELECT COUNT(*) as count FROM demos', [], 2);
        const [totalDemosResult] = await pool.query('SELECT COUNT(*) as count FROM demos');
        const totalDemos = totalDemosResult[0].count;
        debug.dbResult(totalDemosResult, 2);

        debug.level3('Querying pending requests count');
        const pendingQuery = `
      SELECT 
        (SELECT COUNT(*) FROM demo_reports WHERE status = 'pending') +
        (SELECT COUNT(*) FROM mod_reports WHERE status = 'pending') as total_pending
    `;
        debug.dbQuery(pendingQuery, [], 2);
        const [pendingRequestsResult] = await pool.query(pendingQuery);
        const userRequests = pendingRequestsResult[0].total_pending;
        debug.dbResult(pendingRequestsResult, 2);

        debug.level2(`Monitoring data: uptime=${uptime}s, demos=${totalDemos}, requests=${userRequests}`);
        debug.exit('getMonitoringData', startTime, { uptime, totalDemos, userRequests }, 1);

        res.json({
            uptime,
            totalDemos,
            userRequests,
        });
    } catch (error) {
        debug.error('getMonitoringData', error, 1);
        debug.exit('getMonitoringData', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch monitoring data' });
    }
});

router.get('/api/checkAuth', (req, res) => {
    const startTime = debug.enter('checkAuth', [], 2);
    const token = req.cookies.token;
    
    debug.level3('Checking authentication token');
    if (token) {
        jwt.verify(token, JWT_SECRET, (err, user) => {
            if (err) {
                debug.level2('Token verification failed');
                debug.exit('checkAuth', startTime, 'invalid_token', 2);
                res.json({ isLoggedIn: false });
            } else {
                debug.level2('Token verified successfully for user:', user.username);
                debug.exit('checkAuth', startTime, 'authenticated', 2);
                res.json({ isLoggedIn: true, username: user.username });
            }
        });
    } else {
        debug.level2('No token provided');
        debug.exit('checkAuth', startTime, 'no_token', 2);
        res.json({ isLoggedIn: false });
    }
});

router.get('/download-logs', checkAuthToken, (req, res) => {
    const startTime = debug.enter('downloadLogs', [req.user?.username], 1);
    debug.level2('Log download request by:', req.user?.username || 'unknown');
    
    const fs = require('fs');
    const archiver = require('archiver');
    
    const serverLogPath = path.join(rootDir, 'node.js', 'server.log');
    const debugLogPath = path.join(rootDir, 'node.js', 'debug.txt');
    
    debug.level3('Creating zip archive with server.log and debug.txt');
    
    res.setHeader('Content-Type', 'application/zip');
    res.setHeader('Content-Disposition', 'attachment; filename="logs.zip"');
    
    const archive = archiver('zip', {
        zlib: { level: 9 }
    });
    
    archive.on('error', (err) => {
        debug.error('downloadLogs', err, 1);
        debug.exit('downloadLogs', startTime, 'archive_error', 1);
        if (!res.headersSent) {
            res.status(500).send('Error creating log archive');
        }
    });

    archive.pipe(res);

    if (fs.existsSync(serverLogPath)) {
        debug.level3('Adding server.log to archive');
        archive.file(serverLogPath, { name: 'server.log' });
    } else {
        debug.level2('server.log not found, adding placeholder');
        archive.append('Server log file not found', { name: 'server.log' });
    }

    if (fs.existsSync(debugLogPath)) {
        debug.level3('Adding debug.txt to archive');
        archive.file(debugLogPath, { name: 'debug.txt' });
    } else {
        debug.level2('debug.txt not found, adding placeholder');
        archive.append('Debug log file not found or empty', { name: 'debug.txt' });
    }
    
    const timestamp = new Date().toISOString();
    archive.append(`Logs downloaded at: ${timestamp}\nDownloaded by: ${req.user?.username || 'unknown'}`, { name: 'download_info.txt' });
    
    archive.finalize().then(() => {
        debug.level2('Log archive created and sent successfully');
        debug.exit('downloadLogs', startTime, 'success', 1);
    }).catch((err) => {
        debug.error('downloadLogs', err, 1);
        debug.exit('downloadLogs', startTime, 'finalize_error', 1);
    });
});

router.get('/api/pending-reports', authenticateToken, checkPermission(permissions.ADMIN_VIEW_REPORTS), async (req, res) => {
    const startTime = debug.enter('getPendingReports', [req.user.username], 1);
    debug.level2('Fetching pending reports for admin:', req.user.username);
    
    try {
        const query = `
            SELECT dr.*, d.game_type, d.players, u.username 
            FROM demo_reports dr 
            JOIN demos d ON dr.demo_id = d.id 
            JOIN users u ON dr.user_id = u.id 
            WHERE dr.status = 'pending'
        `;
        debug.dbQuery(query, [], 2);
        const [reports] = await pool.query(query);
        debug.dbResult(reports, 2);
        
        debug.level2('Returning pending reports:', reports.length);
        debug.exit('getPendingReports', startTime, { reportsCount: reports.length }, 1);
        res.json(reports);
    } catch (error) {
        debug.error('getPendingReports', error, 1);
        debug.exit('getPendingReports', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to fetch pending reports' });
    }
});

router.put('/api/resolve-report/:reportId', authenticateToken, checkPermission(permissions.ADMIN_VIEW_REPORTS), async (req, res) => {
    const startTime = debug.enter('resolveReport', [req.params.reportId, req.user.username], 1);
    const { reportId } = req.params;
    
    debug.level2('Resolving report:', { reportId, by: req.user.username });
    
    try {
        debug.dbQuery('UPDATE demo_reports SET status = "resolved" WHERE id = ?', [reportId], 2);
        await pool.query('UPDATE demo_reports SET status = "resolved" WHERE id = ?', [reportId]);
        
        debug.level2('Report resolved successfully:', reportId);
        debug.exit('resolveReport', startTime, 'success', 1);
        res.json({ message: 'Report resolved successfully' });
    } catch (error) {
        debug.error('resolveReport', error, 1);
        debug.exit('resolveReport', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to resolve report' });
    }
});

router.get('/api/pending-reports-count', authenticateToken, async (req, res) => {
    const startTime = debug.enter('getPendingReportsCount', [req.user.username], 2);
    debug.level2('Fetching pending reports count');
    
    try {
        debug.dbQuery('SELECT COUNT(*) as count FROM demo_reports WHERE status = "pending"', [], 2);
        const [result] = await pool.query('SELECT COUNT(*) as count FROM demo_reports WHERE status = "pending"');
        debug.dbResult(result, 2);
        
        debug.level2('Returning pending reports count:', result[0].count);
        debug.exit('getPendingReportsCount', startTime, { count: result[0].count }, 2);
        res.json({ count: result[0].count });
    } catch (error) {
        debug.error('getPendingReportsCount', error, 1);
        debug.exit('getPendingReportsCount', startTime, 'error', 2);
        res.status(500).json({ error: 'Failed to fetch pending reports count' });
    }
});

module.exports = router;