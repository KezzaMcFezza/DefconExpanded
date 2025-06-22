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
    checkAuthToken,
    checkPermission
} = require('../../authentication');

const permissions = require('../../permission-index');


const debug = require('../../debug-helpers');

router.get('/api/current-user', checkAuthToken, (req, res) => {
    debug.apiRequest('GET', '/api/current-user', req.user);
    const startTime = debug.enter('getCurrentUser', [req.user?.username], 1);
    
    if (req.user) {
        debug.level2('Returning authenticated user data:', req.user.username);
        debug.exit('getCurrentUser', startTime, 'authenticated_user', 1);
        res.json({ 
            user: { 
                id: req.user.id, 
                username: req.user.username, 
                permissions: req.user.permissions || []
            } 
        });
    } else if (res.locals.user) {
        debug.level2('Returning local user data:', res.locals.user.username);
        debug.exit('getCurrentUser', startTime, 'local_user', 1);
        res.json({ 
            user: res.locals.user 
        });
    } else {
        debug.level2('No authenticated user found');
        debug.exit('getCurrentUser', startTime, 'not_authenticated', 1);
        res.status(401).json({ error: 'Not authenticated' });
    }
});

router.get('/api/permissions/manifest', authenticateToken, checkPermission(permissions.PAGE_ADMIN_PANEL), (req, res) => {
    try {
        const manifest = {};
        
        Object.entries(permissions).forEach(([key, value]) => {
            manifest[key] = value;
        });
        
        res.json({
            manifest: manifest,
            userPermissions: req.user.permissions || []
        });
    } catch (error) {
        console.error('Error creating permission manifest:', error);
        res.status(500).json({ error: 'Unable to create permission manifest' });
    }
});

router.get('/api/users', authenticateToken, checkPermission(permissions.USER_VIEW_LIST), async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT id, username, email, perms FROM users');
        
        const formattedRows = rows.map(user => ({
            ...user,
            permissions: user.perms ? user.perms.split(',').map(p => parseInt(p.trim())) : []
        }));
        
        res.json(formattedRows);
    } catch (error) {
        console.error('Error fetching users:', error);
        res.status(500).json({ error: 'Unable to fetch users' });
    }
});

router.get('/api/users/:userId', authenticateToken, checkPermission(permissions.USER_VIEW_DETAILS), async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT id, username, email, perms FROM users WHERE id = ?', [req.params.userId]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'User not found' });
        } else {
            const user = rows[0];
            user.permissions = user.perms ? user.perms.split(',').map(p => parseInt(p.trim())) : [];
            res.json(user);
        }
    } catch (error) {
        console.error('Error fetching user details:', error);
        res.status(500).json({ error: 'Unable to fetch user details' });
    }
});

router.put('/api/users/:userId', authenticateToken, checkPermission(permissions.USER_EDIT_BASIC), async (req, res) => {
    const { userId } = req.params;
    const { username, email, permissions: newPermissions } = req.body;

    try {
        if (username || email) {
            const updateFields = [];
            const updateValues = [];
            
            if (username) {
                updateFields.push('username = ?');
                updateValues.push(username);
            }
            
            if (email) {
                updateFields.push('email = ?');
                updateValues.push(email);
            }
            
            if (updateFields.length > 0) {
                const query = `UPDATE users SET ${updateFields.join(', ')} WHERE id = ?`;
                updateValues.push(userId);
                await pool.query(query, updateValues);
            }
        }
        
        if (newPermissions && req.user.permissions.includes(permissions.USER_EDIT_PERMISSIONS)) {
            const permissionsString = newPermissions.join(',');
            await pool.query('UPDATE users SET perms = ? WHERE id = ?', [permissionsString, userId]);
        } else if (newPermissions && !req.user.permissions.includes(permissions.USER_EDIT_PERMISSIONS)) {
            return res.status(403).json({ error: 'You do not have permission to edit user permissions' });
        }
        
        res.json({ message: 'User updated successfully' });
    } catch (error) {
        console.error('Error updating user:', error);
        res.status(500).json({ error: 'Unable to update user' });
    }
});

router.delete('/api/users/:userId', authenticateToken, checkPermission(permissions.USER_DELETE), async (req, res) => {
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

router.get('/api/pending-requests', authenticateToken, checkPermission(permissions.USER_APPROVE_REQUESTS), async (req, res) => {
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

router.put('/api/resolve-request/:requestId/:requestType', authenticateToken, checkPermission(permissions.USER_APPROVE_REQUESTS), async (req, res) => {
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