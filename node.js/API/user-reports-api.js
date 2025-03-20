const express = require('express');
const router = express.Router();
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');
const adminFunctions = require('../Functions/admin-functions');

// Middleware
const { authenticateToken, checkRole } = middleware;

// Get pending demo reports
router.get('/pending-demo-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const reports = await adminFunctions.getPendingReports();
    res.json(reports);
  } catch (error) {
    console.error('Error fetching pending reports:', error);
    res.status(500).json({ error: 'Failed to fetch pending reports' });
  }
});

// Get pending mod reports
router.get('/pending-mod-reports', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const reports = await adminFunctions.getPendingModReports();
    res.json(reports);
  } catch (error) {
    console.error('Error fetching pending mod reports:', error);
    res.status(500).json({ error: 'Failed to fetch pending mod reports' });
  }
});

// Get pending user requests
router.get('/pending-requests', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const requests = await adminFunctions.getPendingUserRequests();
    res.json(requests);
  } catch (error) {
    console.error('Error fetching pending requests:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Get all pending reports count for admin dashboard
router.get('/pending-reports-count', authenticateToken, async (req, res) => {
  try {
    const [result] = await pool.query(`
      SELECT 
        (SELECT COUNT(*) FROM demo_reports WHERE status = 'pending') +
        (SELECT COUNT(*) FROM mod_reports WHERE status = 'pending') as count
    `);
    console.log('Pending reports count from database:', result[0].count);
    res.json({ count: result[0].count });
  } catch (error) {
    console.error('Error fetching pending reports count:', error);
    res.status(500).json({ error: 'Failed to fetch pending reports count' });
  }
});

// Submit a demo report
router.post('/report-demo', authenticateToken, async (req, res) => {
  const { demoId, reportType } = req.body;
  const userId = req.user.id;

  try {
    await pool.query(
      'INSERT INTO demo_reports (demo_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
      [demoId, userId, reportType]
    );
    res.json({ message: 'Report submitted successfully' });
  } catch (error) {
    console.error('Error submitting report:', error);
    res.status(500).json({ error: 'Failed to submit report' });
  }
});

// Submit a mod report
router.post('/report-mod', authenticateToken, async (req, res) => {
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

// Resolve a demo report
router.put('/resolve-demo-report/:reportId', authenticateToken, checkRole(5), async (req, res) => {
  const { reportId } = req.params;
  try {
    await adminFunctions.resolveReport(reportId);
    res.json({ message: 'Report resolved successfully' });
  } catch (error) {
    console.error('Error resolving report:', error);
    res.status(500).json({ error: 'Failed to resolve report' });
  }
});

// Resolve a mod report
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

// Resolve a user request
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

// Get user's pending requests
router.get('/user-pending-requests', authenticateToken, async (req, res) => {
  try {
    const userId = req.user.id;

    const [nameChangeRequests] = await pool.query(`
      SELECT lnc.*, u.username
      FROM leaderboard_name_change_requests lnc
      JOIN users u ON u.id = lnc.user_id
      WHERE lnc.user_id = ?
    `, [userId]);

    const [blacklistRequests] = await pool.query(`
      SELECT bl.*, u.username
      FROM blacklist_requests bl
      JOIN users u ON u.id = bl.user_id
      WHERE bl.user_id = ?
    `, [userId]);

    const [deletionRequests] = await pool.query(`
      SELECT ad.*, u.username
      FROM account_deletion_requests ad
      JOIN users u ON u.id = ad.user_id
      WHERE ad.user_id = ?
    `, [userId]);

    const [usernameChangeRequests] = await pool.query(`
      SELECT uc.*, u.username
      FROM username_change_requests uc
      JOIN users u ON u.id = uc.user_id
      WHERE uc.user_id = ?
    `, [userId]);

    const [emailChangeRequests] = await pool.query(`
      SELECT ec.*, u.username
      FROM email_change_requests ec
      JOIN users u ON u.id = ec.user_id
      WHERE ec.user_id = ?
    `, [userId]);

    const userRequests = [
      ...nameChangeRequests.map(r => ({ ...r, type: 'leaderboard_name_change' })),
      ...blacklistRequests.map(r => ({ ...r, type: 'blacklist' })),
      ...deletionRequests.map(r => ({ ...r, type: 'account_deletion' })),
      ...usernameChangeRequests.map(r => ({ ...r, type: 'username_change' })),
      ...emailChangeRequests.map(r => ({ ...r, type: 'email_change' }))
    ];

    res.json(userRequests);
  } catch (error) {
    console.error('Error fetching user-specific pending requests:', error);
    res.status(500).json({ error: 'Unable to fetch user requests' });
  }
});

module.exports = router;