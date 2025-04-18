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

router.post('/api/request-blacklist', authenticateToken, async (req, res) => {
    const userId = req.user.id;

    try {
        await pool.query('INSERT INTO blacklist_requests (user_id) VALUES (?)', [userId]);
        res.json({ message: 'Blacklist request submitted successfully' });
    } catch (error) {
        console.error('Error submitting blacklist request:', error);
        res.status(500).json({ error: 'Server error' });
    }
});

router.post('/api/request-account-deletion', authenticateToken, async (req, res) => {
    const userId = req.user.id;

    try {
        await pool.query('INSERT INTO account_deletion_requests (user_id) VALUES (?)', [userId]);
        res.json({ message: 'Account deletion request submitted successfully' });
    } catch (error) {
        console.error('Error submitting account deletion request:', error);
        res.status(500).json({ error: 'Server error' });
    }
});

router.post('/api/request-username-change', authenticateToken, async (req, res) => {
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

// Email change request
router.post('/api/request-email-change', authenticateToken, async (req, res) => {
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

router.get('/api/user-pending-requests', authenticateToken, async (req, res) => {
    try {
        const userId = req.user.id;

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