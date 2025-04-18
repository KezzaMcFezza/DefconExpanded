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
    checkAuthToken,
    checkRole
} = require('../../authentication');

router.get('/api/current-user', checkAuthToken, (req, res) => {
    if (req.user) {
        res.json({ 
            user: { 
                id: req.user.id, 
                username: req.user.username, 
                role: req.user.role 
            } 
        });
    } else if (res.locals.user) {
        res.json({ 
            user: res.locals.user 
        });
    } else {
        res.status(401).json({ error: 'Not authenticated' });
    }
});

router.get('/api/users', authenticateToken, checkRole(2), async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT id, username, email, role FROM users');
        res.json(rows);
    } catch (error) {
        console.error('Error fetching users:', error);
        res.status(500).json({ error: 'Unable to fetch users' });
    }
});

router.get('/api/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
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

router.put('/api/users/:userId', authenticateToken, checkRole(2), async (req, res) => {
    const { userId } = req.params;
    const { username, email, role } = req.body;


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

router.delete('/api/users/:userId', authenticateToken, checkRole(1), async (req, res) => {
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

router.get('/api/pending-requests', authenticateToken, checkRole(5), async (req, res) => {
    try {

        const [blacklistRequests] = await pool.query(`
          SELECT bl.*, u.username
          FROM blacklist_requests bl
          JOIN users u ON u.id = bl.user_id
          WHERE bl.status = "pending"
      `);

        const [deletionRequests] = await pool.query(`
          SELECT ad.*, u.username
          FROM account_deletion_requests ad
          JOIN users u ON u.id = ad.user_id
          WHERE ad.status = "pending"
      `);

        const [usernameChangeRequests] = await pool.query(`
          SELECT uc.*, u.username
          FROM username_change_requests uc
          JOIN users u ON u.id = uc.user_id
          WHERE uc.status = "pending"
      `);

        const [emailChangeRequests] = await pool.query(`
          SELECT ec.*, u.username
          FROM email_change_requests ec
          JOIN users u ON u.id = ec.user_id
          WHERE ec.status = "pending"
      `);

        const allRequests = [
            ...blacklistRequests.map(r => ({ ...r, type: 'blacklist' })),
            ...deletionRequests.map(r => ({ ...r, type: 'account_deletion' })),
            ...usernameChangeRequests.map(r => ({ ...r, type: 'username_change' })),
            ...emailChangeRequests.map(r => ({ ...r, type: 'email_change' }))
        ];

        res.json(allRequests);
    } catch (error) {
        console.error('Error fetching pending requests:', error);
        res.status(500).json({ error: 'Server error' });
    }
});

router.put('/api/resolve-request/:requestId/:requestType', authenticateToken, checkRole(5), async (req, res) => {
    const { requestId, requestType } = req.params;
    const { status } = req.body;

    try {
        let tableName;
        switch (requestType) {
            case 'blacklist':
                tableName = 'blacklist_requests';
                break;
            case 'account_deletion':
                tableName = 'account_deletion_requests';
                break;
            case 'username_change':
                tableName = 'username_change_requests';
                break;
            case 'email_change':
                tableName = 'email_change_requests';
                break;
            default:
                return res.status(400).json({ error: 'Invalid request type' });
        }

        await pool.query(`UPDATE ${tableName} SET status = ? WHERE id = ?`, [status, requestId]);

        if (status === 'approved') {
            switch (requestType) {
                case 'blacklist':
                    console.log('Blacklist request approved. Admin needs to manually blacklist the user.');
                    break;
                case 'account_deletion':
                    console.log('Account deletion request approved. Admin needs to manually delete the user account.');
                    break;
                case 'username_change':
                    console.log('Username change request approved. Admin needs to manually update the username.');
                    break;
                case 'email_change':
                    console.log('Email change request approved. Admin needs to manually update the email.');
                    break;
                default:
                    console.log('Unknown request type approved.');
            }
        }

        res.json({ message: 'Request resolved successfully' });
    } catch (error) {
        console.error('Error resolving request:', error);
        res.status(500).json({ error: 'Server error' });
    }
});

module.exports = router;