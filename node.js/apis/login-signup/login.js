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
const jwt = require('jsonwebtoken');
const JWT_SECRET = process.env.JWT_SECRET;
const bcrypt = require('bcrypt');

const {
    pool
} = require('../../constants');

router.post('/api/login', async (req, res) => {
    const { username, password, rememberMe } = req.body;
    console.log(`Login attempt for username: ${username}`);

    try {
        if (!JWT_SECRET) {
            console.error('JWT_SECRET is missing or undefined');
            return res.status(500).json({ error: 'Internal server configuration error' });
        }

        const [users] = await pool.query('SELECT * FROM users WHERE username = ?', [username]);
        if (users.length === 0) {
            console.log('Login failed: User not found');
            return res.status(400).json({ error: 'Invalid username or password' });
        }

        const user = users[0];
        const isPasswordValid = await bcrypt.compare(password, user.password);
        if (!isPasswordValid) {
            console.log('Login failed: Invalid password');
            return res.status(400).json({ error: 'Invalid username or password' });
        }

        if (!user.is_verified) {
            console.log('Login failed: User not verified');
            return res.status(400).json({ error: 'Please verify your email before logging in' });
        }

        const tokenPayload = { 
            id: user.id, 
            username: user.username, 
            role: user.role 
        };

        console.log('Creating token with payload:', tokenPayload);
        
        const token = jwt.sign(
            tokenPayload,
            JWT_SECRET,
            { expiresIn: rememberMe ? '7d' : '8h' }
        );

        console.log('Login successful. Token created.');
        
        res.cookie('token', token, {
            httpOnly: true,
            secure: process.env.NODE_ENV === 'production',
            maxAge: rememberMe ? 30 * 24 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000,
            sameSite: 'strict'
        });

        res.locals.user = { id: user.id, username: user.username, role: user.role };

        res.json({ 
            message: 'Login successful', 
            username: user.username, 
            role: user.role,
            tokenSet: true 
        });
    } catch (error) {
        console.error('Login error:', error);
        res.status(500).json({ error: 'Internal server error' });
    }
});

module.exports = router;