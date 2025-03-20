const express = require('express');
const router = express.Router();
const path = require('path');
const { pool } = require('../Functions/database-backup');
const adminFunctions = require('../Functions/admin-functions');
const middleware = require('../Middleware/middleware');
const utils = require('../Utils/shared-utils');

// Middleware
const { authenticateToken, checkRole } = middleware;

// Monitoring data
router.get('/monitoring-data', authenticateToken, async (req, res) => {
  try {
    const monitoringData = await adminFunctions.getMonitoringData();
    res.json(monitoringData);
  } catch (error) {
    console.error('Error fetching monitoring data:', error);
    res.status(500).json({ error: 'Unable to fetch monitoring data' });
  }
});

// Config files
router.get('/config-files', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const configFiles = await adminFunctions.getConfigFiles();
    res.json(configFiles);
  } catch (error) {
    console.error('Error reading config files:', error);
    res.status(500).json({ error: 'Unable to read config files' });
  }
});

router.get('/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const content = await adminFunctions.getConfigFileContent(req.params.filename);
    res.json({ content });
  } catch (error) {
    console.error('Error reading file:', error);
    res.status(500).json({ error: 'Unable to read file' });
  }
});

router.put('/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
  try {
    await adminFunctions.updateConfigFile(req.params.filename, req.body.content);
    res.json({ message: 'File updated successfully' });
  } catch (error) {
    console.error('Error writing file:', error);
    res.status(500).json({ error: 'Unable to write file' });
  }
});

// Admin panel routes for different pages
router.get('/admin-panel', authenticateToken, checkRole(5), utils.serveAdminPage('admin-panel', 5));
router.get('/blacklist', authenticateToken, checkRole(5), utils.serveAdminPage('blacklist', 5));
router.get('/demo-manage', authenticateToken, checkRole(5), utils.serveAdminPage('demo-manage', 5));
router.get('/playerlookup', authenticateToken, checkRole(2), utils.serveAdminPage('playerlookup', 2));
router.get('/server-managment', authenticateToken, checkRole(1), utils.serveAdminPage('servermanagment', 1));
router.get('/leaderboard-manage', authenticateToken, checkRole(5), utils.serveAdminPage('leaderboard-manage', 5));
router.get('/account-manage', authenticateToken, checkRole(2), utils.serveAdminPage('account-manage', 2));
router.get('/modmanagment', authenticateToken, checkRole(5), utils.serveAdminPage('modmanagment', 5));
router.get('/dedconmanagment', authenticateToken, checkRole(2), utils.serveAdminPage('dedconmanagment', 2));
router.get('/resourcemanagment', authenticateToken, checkRole(3), utils.serveAdminPage('resourcemanagment', 3));
router.get('/server-console', authenticateToken, checkRole(1), utils.serveAdminPage('server-console', 2));

// User management
router.get('/users', authenticateToken, checkRole(2), async (req, res) => {
  console.log('Fetching users. Authenticated user:', JSON.stringify(req.user, null, 2));
  try {
    const [rows] = await pool.query('SELECT id, username, email, role FROM users');
    console.log('Fetched users:', rows.length);
    res.json(rows);
  } catch (error) {
    console.error('Error fetching users:', error);
    res.status(500).json({ error: 'Unable to fetch users' });
  }
});

router.get('/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT id, username, email, role FROM users WHERE id = ?', [req.params.userId]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'User not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching user details:', error);
    res.status(500).json({ error: 'Unable to fetch user details' });
  }
});

router.put('/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
  const { userId } = req.params;
  const { username, email, role } = req.body;

  // Only allow super admins to change user roles
  if (req.user.role !== 1 && role !== undefined) {
    return res.status(403).json({ error: 'Only super admins can change user roles' });
  }

  try {
    const [result] = await pool.query(
      'UPDATE users SET username = ?, email = ?, role = ? WHERE id = ?',
      [username, email, role, userId]
    );

    if (result.affectedRows === 0) {
      res.status(404).json({ error: 'User not found' });
    } else {
      res.json({ message: 'User updated successfully' });
    }
  } catch (error) {
    console.error('Error updating user:', error);
    res.status(500).json({ error: 'Unable to update user' });
  }
});

router.delete('/users/:userId', authenticateToken, checkRole(1), async (req, res) => {
  const { userId } = req.params;

  try {
    const [result] = await pool.query('DELETE FROM users WHERE id = ?', [userId]);
    if (result.affectedRows === 0) {
      res.status(404).json({ error: 'User not found' });
    } else {
      res.json({ message: 'User deleted successfully' });
    }
  } catch (error) {
    console.error('Error deleting user:', error);
    res.status(500).json({ error: 'Unable to delete user' });
  }
});

// Pending reports
router.get('/pending-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const reports = await adminFunctions.getPendingReports();
    res.json(reports);
  } catch (error) {
    console.error('Error fetching pending reports:', error);
    res.status(500).json({ error: 'Failed to fetch pending reports' });
  }
});

router.put('/resolve-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
  const { reportId } = req.params;
  try {
    await adminFunctions.resolveReport(reportId);
    res.json({ message: 'Report resolved successfully' });
  } catch (error) {
    console.error('Error resolving report:', error);
    res.status(500).json({ error: 'Failed to resolve report' });
  }
});

// Pending mod reports
router.get('/pending-mod-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const reports = await adminFunctions.getPendingModReports();
    res.json(reports);
  } catch (error) {
    console.error('Error fetching pending mod reports:', error);
    res.status(500).json({ error: 'Failed to fetch pending mod reports' });
  }
});

router.put('/resolve-mod-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
  const { reportId } = req.params;
  try {
    await adminFunctions.resolveModReport(reportId);
    res.json({ message: 'Mod report resolved successfully' });
  } catch (error) {
    console.error('Error resolving mod report:', error);
    res.status(500).json({ error: 'Failed to resolve mod report' });
  }
});

// Pending requests (name changes, blacklisting, etc.)
router.get('/pending-requests', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const requests = await adminFunctions.getPendingUserRequests();
    res.json(requests);
  } catch (error) {
    console.error('Error fetching pending requests:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

router.put('/resolve-request/:requestId/:requestType', authenticateToken, checkRole(5), async (req, res) => {
  const { requestId, requestType } = req.params;
  const { status } = req.body;

  try {
    await adminFunctions.resolveUserRequest(requestId, requestType, status);
    res.json({ message: 'Request resolved successfully' });
  } catch (error) {
    console.error('Error resolving request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Download server logs
router.get('/download-logs', authenticateToken, checkRole(2), (req, res) => {
  const logPath = path.join(__dirname, '..', 'server.log');
  res.download(logPath, 'server.log', (err) => {
    if (err) {
      console.error('Error downloading log file:', err);
      res.status(500).send('Error downloading log file');
    }
  });
});

module.exports = router;