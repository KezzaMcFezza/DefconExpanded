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

const path = require('path');
const jwt = require('jsonwebtoken');
const fs = require('fs');
const JWT_SECRET = process.env.JWT_SECRET;

const { 
    pool,
    publicDir
 } = require('./constants');


const debugUtils = require('./debug-utils');


const authenticateToken = async (req, res, next) => {
    const startTime = debugUtils.debugFunctionEntry('authenticateToken', [req.url], 2);
    const token = req.cookies.token;

    if (!token) {
        console.debug(2, 'Authentication failed: No token provided');
        debugUtils.debugFunctionExit('authenticateToken', startTime, 'no_token', 2);
        return res.status(401).json({ error: 'Not authenticated' });
    }

    try {
        console.debug(3, 'Verifying JWT token');
        const decoded = jwt.verify(token, JWT_SECRET);
        console.debug(3, 'Token decoded successfully, user ID:', decoded.id);
        
        const [users] = await pool.query('SELECT id, username, perms FROM users WHERE id = ?', [decoded.id]);
        console.debug(3, `Database query for user ${decoded.id} returned ${users.length} results`);
        
        if (users.length === 0) {
            console.debug(2, 'Authentication failed: User not found in database');
            debugUtils.debugFunctionExit('authenticateToken', startTime, 'user_not_found', 2);
            return res.status(403).json({ error: 'Invalid token' });
        }

        const user = users[0];
        user.permissions = user.perms ? user.perms.split(',').map(p => parseInt(p.trim())) : [];
        
        req.user = user;
        console.debug(2, `Authentication successful for user: ${user.username} with ${user.permissions.length} permissions`);
        debugUtils.debugFunctionExit('authenticateToken', startTime, 'success', 2);
        next();
    } catch (err) {
        console.debug(1, 'Token verification failed:', err.message);
        debugUtils.debugFunctionExit('authenticateToken', startTime, 'error', 2);
        return res.status(403).json({ error: 'Invalid token' });
    }
};

const checkAuthToken = (req, res, next) => {
    const startTime = debugUtils.debugFunctionEntry('checkAuthToken', [req.url], 3);
    const token = req.cookies.token;
    
    if (token) {
        console.debug(3, 'Token found, verifying...');
        jwt.verify(token, JWT_SECRET, async (err, user) => {
            if (err) {
                console.debug(3, 'Token verification failed:', err.message);
                res.locals.user = null;
                debugUtils.debugFunctionExit('checkAuthToken', startTime, 'invalid_token', 3);
                next();
            } else {
                try {
                    console.debug(3, 'Token valid, fetching user data for ID:', user.id);
                    const [users] = await pool.query('SELECT id, username, perms FROM users WHERE id = ?', [user.id]);
                    
                    if (users.length === 0) {
                        console.debug(3, 'User not found in database');
                        res.locals.user = null;
                    } else {
                        const userData = users[0];
                        userData.permissions = userData.perms ? userData.perms.split(',').map(p => parseInt(p.trim())) : [];
                        
                        res.locals.user = { 
                            id: userData.id, 
                            username: userData.username,
                            permissions: userData.permissions
                        };
                        req.user = userData;
                        console.debug(3, `User data loaded for: ${userData.username}`);
                    }
                } catch (error) {
                    console.error('Error fetching user permissions:', error);
                    console.debug(2, 'Database error while fetching user:', error.message);
                    res.locals.user = null;
                }
                debugUtils.debugFunctionExit('checkAuthToken', startTime, 'success', 3);
                next();
            }
        });
    } else {
        console.debug(3, 'No token found');
        res.locals.user = null;
        debugUtils.debugFunctionExit('checkAuthToken', startTime, 'no_token', 3);
        next();
    }
};

const checkPermission = (requiredPermission) => {
    return (req, res, next) => {
        const startTime = debugUtils.debugFunctionEntry('checkPermission', [requiredPermission, req.user?.username], 2);
        
        if (!req.user) {
            console.debug(2, 'Permission check failed: User not authenticated');
            debugUtils.debugFunctionExit('checkPermission', startTime, 'not_authenticated', 2);
            return res.status(401).json({ error: 'Not authenticated' });
        }
        
        if (req.user.permissions && req.user.permissions.includes(requiredPermission)) {
            console.debug(2, `Permission granted: ${req.user.username} has permission ${requiredPermission}`);
            debugUtils.debugFunctionExit('checkPermission', startTime, 'granted', 2);
            next();
        } else {
            console.debug(2, `Permission denied: ${req.user.username} lacks permission ${requiredPermission}`);
            debugUtils.debugFunctionExit('checkPermission', startTime, 'denied', 2);
            return res.status(403).json({ error: 'Insufficient permissions' });
        }
    };
};

const checkAnyPermission = (permissionArray) => {
    return (req, res, next) => {
        if (!req.user) {
            return res.status(401).json({ error: 'Not authenticated' });
        }
        
        if (req.user.permissions && permissionArray.some(permission => req.user.permissions.includes(permission))) {
            next();
        } else {
            return res.status(403).json({ error: 'Insufficient permissions' });
        }
    };
};

function serveAdminPage(pageName, requiredPermission) {
    return (req, res) => {
        fs.readFile(path.join(publicDir, `${pageName}.html`), 'utf8', (err, data) => {
            if (err) {
                console.error('Error reading file:', err);
                return res.status(500).send('Error loading page');
            }
            
            const safePermissions = JSON.stringify(req.user.permissions || []);
            const modifiedHtml = data.replace('</head>', `<script>window.userPermissions = ${safePermissions};</script></head>`);
            
            res.send(modifiedHtml);
        });
    };
};

module.exports = {
    authenticateToken,
    checkAuthToken,
    checkPermission,
    checkAnyPermission,
    serveAdminPage
};