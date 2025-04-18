// authentication.js - Revised
const jwt = require('jsonwebtoken');
const JWT_SECRET = process.env.JWT_SECRET;
const { pool } = require('./constants');

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

module.exports = {
    authenticateToken,
    checkAuthToken,
    checkRole
};