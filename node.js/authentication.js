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

const path = require('path');
const jwt = require('jsonwebtoken');
const fs = require('fs');
const JWT_SECRET = process.env.JWT_SECRET;

const { 
    pool,
    publicDir
 } = require('./constants');


const authenticateToken = async (req, res, next) => {
    const token = req.cookies.token;

    if (!token) {
        return res.status(401).json({ error: 'Not authenticated' });
    }

    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        const [users] = await pool.query('SELECT id, username, role FROM users WHERE id = ?', [decoded.id]);
        
        if (users.length === 0) {
            return res.status(403).json({ error: 'Invalid token' });
        }

        req.user = users[0];
        next();
    } catch (err) {
        return res.status(403).json({ error: 'Invalid token' });
    }
};

const checkAuthToken = (req, res, next) => {
    const token = req.cookies.token;
    
    if (token) {
        jwt.verify(token, JWT_SECRET, (err, user) => {
            if (err) {
                res.locals.user = null;
            } else {
                res.locals.user = { id: user.id, username: user.username };
                req.user = user;
            }
            next();
        });
    } else {
        res.locals.user = null;
        next();
    }
};

const checkRole = (requiredRole) => {
    return (req, res, next) => {
        
        if (!req.user) {
            return res.status(401).json({ error: 'Not authenticated' });
        }
        
        if (req.user.role <= requiredRole) {
            next();
        } else {
            return res.status(403).json({ error: 'Insufficient permissions' });
        }
    };
};

function serveAdminPage(pageName, minRole) {
    return (req, res) => {
        console.log(`Accessing /${pageName}. User:`, JSON.stringify(req.user, null, 2));
        fs.readFile(path.join(publicDir, `${pageName}.html`), 'utf8', (err, data) => {
            if (err) {
                console.error('Error reading file:', err);
                return res.status(500).send('Error loading page');
            }
            
            const safeRole = JSON.stringify(req.user.role);
            const modifiedHtml = data.replace('</head>', `<script>window.userRole = ${safeRole};</script></head>`);
            
            res.send(modifiedHtml);
        });
    };
}

module.exports = {
    authenticateToken,
    checkAuthToken,
    checkRole,
    serveAdminPage
};