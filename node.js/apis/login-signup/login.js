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
const jwt = require('jsonwebtoken');
const JWT_SECRET = process.env.JWT_SECRET;
const bcrypt = require('bcrypt');

const {
    pool
} = require('../../constants');


const debug = require('../../debug-helpers');

router.post('/api/login', async (req, res) => {
    const startTime = debug.enter('userLogin', [req.body.username], 1);
    const { username, password, rememberMe } = req.body;
    debug.level2('Login attempt:', { username, rememberMe });

    try {
        if (!JWT_SECRET) {
            debug.level1('JWT_SECRET is missing or undefined');
            debug.exit('userLogin', startTime, 'missing_jwt_secret', 1);
            return res.status(500).json({ error: 'Internal server configuration error' });
        }

        debug.dbQuery('SELECT * FROM users WHERE username = ?', [username], 2);
        const [users] = await pool.query('SELECT * FROM users WHERE username = ?', [username]);
        debug.dbResult(users, 2);

        if (users.length === 0) {
            debug.level1('Login failed: User not found:', username);
            debug.exit('userLogin', startTime, 'user_not_found', 1);
            return res.status(400).json({ error: 'Invalid username or password' });
        }

        const user = users[0];
        debug.level3('User found, verifying password');
        const isPasswordValid = await bcrypt.compare(password, user.password);
        if (!isPasswordValid) {
            debug.level1('Login failed: Invalid password for user:', username);
            debug.exit('userLogin', startTime, 'invalid_password', 1);
            return res.status(400).json({ error: 'Invalid username or password' });
        }

        if (!user.is_verified) {
            debug.level1('Login failed: User not verified:', username);
            debug.exit('userLogin', startTime, 'not_verified', 1);
            return res.status(400).json({ error: 'Please verify your email before logging in' });
        }

        const tokenPayload = { 
            id: user.id, 
            username: user.username
        };
        
        debug.level3('Creating token with payload:', tokenPayload);
        
        const token = jwt.sign(
            tokenPayload,
            JWT_SECRET,
            { expiresIn: rememberMe ? '7d' : '8h' }
        );
        
        debug.level2('Login successful. Token created for user:', username);
        
        res.cookie('token', token, {
            httpOnly: true,
            secure: process.env.NODE_ENV === 'production',
            maxAge: rememberMe ? 30 * 24 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000,
            sameSite: 'strict'
        });
        
        res.locals.user = { id: user.id, username: user.username };
        
        debug.exit('userLogin', startTime, 'success', 1);
        res.json({ 
            message: 'Login successful', 
            username: user.username,
            tokenSet: true 
        });
    } catch (error) {
        debug.error('userLogin', error, 1);
        debug.exit('userLogin', startTime, 'error', 1);
        res.status(500).json({ error: 'Internal server error' });
    }
});

module.exports = router;