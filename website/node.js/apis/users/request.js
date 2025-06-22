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

router.post('/api/request-blacklist', authenticateToken, async (req, res) => {
    const startTime = debug.enter('requestBlacklist', [req.user.username], 1);
    const userId = req.user.id;

    debug.level2('Blacklist request:', { userId, username: req.user.username });

    try {
        debug.level3('Inserting blacklist request into database');
        debug.dbQuery('INSERT INTO blacklist_requests (user_id) VALUES (?)', [userId], 2);
        await pool.query('INSERT INTO blacklist_requests (user_id) VALUES (?)', [userId]);
        
        debug.level2('Blacklist request submitted by:', req.user.username);
        debug.exit('requestBlacklist', startTime, 'success', 1);
        res.json({ message: 'Blacklist request submitted successfully' });
    } catch (error) {
        debug.error('requestBlacklist', error, 1);
        debug.exit('requestBlacklist', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error' });
    }
});

router.post('/api/request-account-deletion', authenticateToken, async (req, res) => {
    const startTime = debug.enter('requestAccountDeletion', [req.user.username], 1);
    const userId = req.user.id;

    debug.level2('Account deletion request:', { userId, username: req.user.username });

    try {
        debug.level3('Inserting account deletion request into database');
        debug.dbQuery('INSERT INTO account_deletion_requests (user_id) VALUES (?)', [userId], 2);
        await pool.query('INSERT INTO account_deletion_requests (user_id) VALUES (?)', [userId]);
        
        debug.level2('Account deletion request submitted by:', req.user.username);
        debug.exit('requestAccountDeletion', startTime, 'success', 1);
        res.json({ message: 'Account deletion request submitted successfully' });
    } catch (error) {
        debug.error('requestAccountDeletion', error, 1);
        debug.exit('requestAccountDeletion', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error' });
    }
});

router.post('/api/request-username-change', authenticateToken, async (req, res) => {
    const startTime = debug.enter('requestUsernameChange', [req.user.username, req.body.newUsername], 1);
    const { newUsername } = req.body;
    const userId = req.user.id;

    debug.level2('Username change request:', { userId, username: req.user.username, newUsername });

    try {
        debug.level3('Inserting username change request into database');
        debug.dbQuery('INSERT INTO username_change_requests', [userId, newUsername], 2);
        await pool.query('INSERT INTO username_change_requests (user_id, requested_username) VALUES (?, ?)',
            [userId, newUsername]);
        
        debug.level2('Username change request submitted:', { from: req.user.username, to: newUsername });
        debug.exit('requestUsernameChange', startTime, 'success', 1);
        res.json({ message: 'Username change request submitted successfully' });
    } catch (error) {
        debug.error('requestUsernameChange', error, 1);
        debug.exit('requestUsernameChange', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error' });
    }
});

router.post('/api/request-email-change', authenticateToken, async (req, res) => {
    const startTime = debug.enter('requestEmailChange', [req.user.username, req.body.newEmail], 1);
    const { newEmail } = req.body;
    const userId = req.user.id;

    debug.level2('Email change request:', { userId, username: req.user.username, newEmail });

    try {
        debug.level3('Inserting email change request into database');
        debug.dbQuery('INSERT INTO email_change_requests', [userId, newEmail], 2);
        await pool.query('INSERT INTO email_change_requests (user_id, requested_email) VALUES (?, ?)',
            [userId, newEmail]);
        
        debug.level2('Email change request submitted:', { user: req.user.username, newEmail });
        debug.exit('requestEmailChange', startTime, 'success', 1);
        res.json({ message: 'Email change request submitted successfully' });
    } catch (error) {
        debug.error('requestEmailChange', error, 1);
        debug.exit('requestEmailChange', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error' });
    }
});

router.get('/api/user-pending-requests', authenticateToken, async (req, res) => {
    const startTime = debug.enter('getUserPendingRequests', [req.user.username], 1);
    debug.level2('Fetching pending requests for user:', req.user.username);
    
    try {
        const userId = req.user.id;

        debug.level3('Fetching blacklist requests');
        const [blacklistRequests] = await pool.query(`
        SELECT bl.*, u.username
        FROM blacklist_requests bl
        JOIN users u ON u.id = bl.user_id
        WHERE bl.user_id = ?
      `, [userId]);

        debug.level3('Fetching deletion requests');
        const [deletionRequests] = await pool.query(`
        SELECT ad.*, u.username
        FROM account_deletion_requests ad
        JOIN users u ON u.id = ad.user_id
        WHERE ad.user_id = ?
      `, [userId]);

        debug.level3('Fetching username change requests');
        const [usernameChangeRequests] = await pool.query(`
        SELECT uc.*, u.username
        FROM username_change_requests uc
        JOIN users u ON u.id = uc.user_id
        WHERE uc.user_id = ?
      `, [userId]);

        debug.level3('Fetching email change requests');
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

        debug.level2('Returning user requests:', { 
            user: req.user.username, 
            totalRequests: userRequests.length,
            blacklist: blacklistRequests.length,
            deletion: deletionRequests.length,
            username: usernameChangeRequests.length,
            email: emailChangeRequests.length
        });
        debug.exit('getUserPendingRequests', startTime, { totalRequests: userRequests.length }, 1);
        res.json(userRequests);
    } catch (error) {
        debug.error('getUserPendingRequests', error, 1);
        debug.exit('getUserPendingRequests', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch user requests' });
    }
});

module.exports = router;