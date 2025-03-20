const express = require('express');
const router = express.Router();
const fs = require('fs');
const path = require('path');
const loginSignupFunctions = require('../Functions/login-signup-functions');
const middleware = require('../Middleware/middleware');

// Middleware
const { authenticateToken } = middleware;

// Sign up
router.post('/signup', async (req, res) => {
  try {
    const { username, email, password } = req.body;
    const result = await loginSignupFunctions.registerUser(username, email, password);
    res.status(200).json(result);
  } catch (error) {
    console.error('Signup error:', error);
    res.status(500).json({ error: error.message || 'Server error.' });
  }
});

// Verify email
router.get('/verify', async (req, res) => {
  try {
    const { token } = req.query;
    const result = await loginSignupFunctions.verifyUser(token);
    
    if (result.success) {
      res.sendFile(path.join(__dirname, '..', 'public', 'verificationsuccess.html'));
    } else {
      res.sendFile(path.join(__dirname, '..', 'public', 'verificationerror.html'));
    }
  } catch (error) {
    console.error('Verification error:', error);
    res.sendFile(path.join(__dirname, '..', 'public', 'verificationerror.html'));
  }
});

// Login
router.post('/login', async (req, res) => {
  try {
    const { username, password, rememberMe } = req.body;
    const result = await loginSignupFunctions.loginUser(username, password, rememberMe);
    
    // Set cookie
    res.cookie('token', result.token, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      maxAge: rememberMe ? 30 * 24 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000,
    });

    res.json({ 
      message: 'Login successful', 
      username: result.user.username, 
      role: result.user.role 
    });
  } catch (error) {
    console.error('Login error:', error);
    res.status(400).json({ error: error.message || 'Invalid username or password' });
  }
});

// Logout
router.post('/logout', (req, res) => {
  res.clearCookie('token');
  res.json({ message: 'Logged out successfully' });
});

// Forgot password
router.post('/forgot-password', async (req, res) => {
  try {
    const { username, email } = req.body;
    const result = await loginSignupFunctions.requestPasswordReset(username, email);
    res.json(result);
  } catch (error) {
    console.error('Forgot password error:', error);
    res.status(400).json({ error: error.message || 'Server error. Please try again later.' });
  }
});

// Reset password
router.post('/reset-password', async (req, res) => {
  try {
    const { token } = req.query;
    const { password } = req.body;
    const result = await loginSignupFunctions.resetPassword(token, password);
    res.json(result);
  } catch (error) {
    console.error('Reset password error:', error);
    res.status(400).json({ error: error.message || 'Server error. Please try again later.' });
  }
});

// Request password change (for logged in users)
router.post('/request-password-change', async (req, res) => {
  try {
    const { email } = req.body;
    const result = await loginSignupFunctions.requestPasswordChange(email);
    res.json(result);
  } catch (error) {
    console.error('Password change request error:', error);
    res.status(400).json({ error: error.message || 'Server error. Please try again later.' });
  }
});

// Change password
router.post('/change-password', async (req, res) => {
  try {
    const { token } = req.query;
    const { newPassword, confirmPassword } = req.body;
    const result = await loginSignupFunctions.changePassword(token, newPassword, confirmPassword);
    res.json(result);
  } catch (error) {
    console.error('Password change error:', error);
    res.status(400).json({ error: error.message || 'Failed to change password.' });
  }
});

// Get current user
router.get('/current-user', authenticateToken, (req, res) => {
  if (req.user) {
    res.json({ user: { id: req.user.id, username: req.user.username, role: req.user.role } });
  } else {
    res.status(401).json({ error: 'Not authenticated' });
  }
});

// Check auth status
router.get('/checkAuth', (req, res) => {
  const token = req.cookies.token;
  if (token) {
    try {
      const user = jwt.verify(token, process.env.JWT_SECRET);
      res.json({ isLoggedIn: true, username: user.username });
    } catch (err) {
      res.json({ isLoggedIn: false });
    }
  } else {
    res.json({ isLoggedIn: false });
  }
});

// Request account deletion
router.post('/request-account-deletion', authenticateToken, async (req, res) => {
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO account_deletion_requests (user_id) VALUES (?)', [userId]);
    res.json({ message: 'Account deletion request submitted successfully' });
  } catch (error) {
    console.error('Error submitting account deletion request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Request username change
router.post('/request-username-change', authenticateToken, async (req, res) => {
  const { newUsername } = req.body;
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO username_change_requests (user_id, requested_username) VALUES (?, ?)',
      [userId, newUsername]);
    res.json({ message: 'Username change request submitted successfully' });
  } catch (error) {
    console.error('Error submitting username change request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Request email change
router.post('/request-email-change', authenticateToken, async (req, res) => {
  const { newEmail } = req.body;
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO email_change_requests (user_id, requested_email) VALUES (?, ?)',
      [userId, newEmail]);
    res.json({ message: 'Email change request submitted successfully' });
  } catch (error) {
    console.error('Error submitting email change request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Get user pending requests
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