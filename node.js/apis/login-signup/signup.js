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
const path = require('path');
const jwt = require('jsonwebtoken');
const JWT_SECRET = process.env.JWT_SECRET;
const bcrypt = require('bcrypt');
const pendingVerifications = new Map();

const {
    pool,
    transporter,
    publicDir
} = require('../../constants');


const debug = require('../../debug-helpers');

router.post('/api/signup', async (req, res) => {
    const startTime = debug.enter('userSignup', [req.body.username, req.body.email], 1);
    try {
        const { username, email, password } = req.body;
        debug.level2('Signup attempt:', { username, email });

        if (!username || !email || !password) {
            debug.level1('Signup failed: Missing required fields');
            debug.exit('userSignup', startTime, 'missing_fields', 1);
            return res.status(400).json({ error: 'All fields are required' });
        }

        debug.dbQuery('SELECT * FROM users WHERE email = ? OR username = ?', [email, username], 2);
        const [existingUser] = await pool.query('SELECT * FROM users WHERE email = ? OR username = ?', [email, username]);
        debug.dbResult(existingUser, 2);

        if (existingUser.length > 0) {
            debug.level1('Signup failed: User already exists:', { username, email });
            debug.exit('userSignup', startTime, 'user_exists', 1);
            return res.status(400).json({ error: 'User with this email or username already exists.' });
        }

        debug.level3('Hashing password');
        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(password, salt);
        const verificationToken = jwt.sign({ email }, JWT_SECRET, { expiresIn: '1d' });

        debug.level3('Storing pending verification:', email);
        pendingVerifications.set(email, {
            username,
            email,
            password: hashedPassword,
            verificationToken,
        });

        debug.level2('Sending verification email to:', email);
        const verificationLink = `${process.env.BASE_URL}/verify?token=${verificationToken}`;
        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Verify Your Email',
            html: `Please click this link to verify your email: <a href="${verificationLink}">${verificationLink}</a>`,
        });

        debug.level2('Signup successful, verification email sent:', { username, email });
        debug.exit('userSignup', startTime, 'success', 1);
        res.status(200).json({ message: 'Please check your email to verify your account.' });
    } catch (error) {
        debug.error('userSignup', error, 1);
        debug.exit('userSignup', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error.' });
    }
});

router.get('/verify', async (req, res) => {
    const startTime = debug.enter('verifyEmail', [req.query.token], 1);
    try {
        const { token } = req.query;
        debug.level2('Email verification attempt with token');

        debug.level3('Verifying JWT token');
        const decoded = jwt.verify(token, JWT_SECRET);
        const email = decoded.email;
        debug.level3('Token verified for email:', email);

        if (!pendingVerifications.has(email)) {
            debug.level1('Verification failed: No pending verification for email:', email);
            debug.exit('verifyEmail', startTime, 'no_pending_verification', 1);
            return res.sendFile(path.join(publicDir, 'verificationerror.html'));
        }

        const userDetails = pendingVerifications.get(email);
        debug.level2('Creating verified user account:', userDetails.username);

        debug.dbQuery('INSERT INTO users', [userDetails.username, userDetails.email], 2);
        await pool.query('INSERT INTO users (username, email, password, is_verified) VALUES (?, ?, ?, 1)',
            [userDetails.username, userDetails.email, userDetails.password]);
        await pool.query('INSERT INTO user_profiles (user_id) VALUES (LAST_INSERT_ID())');

        const profileUrl = `/profile/${userDetails.username}`;
        await pool.query('UPDATE users SET profile_url = ? WHERE email = ?', [profileUrl, email]);
        
        debug.level3('Removing pending verification:', email);
        pendingVerifications.delete(email);

        debug.level2('Email verification successful:', { username: userDetails.username, email });
        debug.exit('verifyEmail', startTime, 'success', 1);
        res.sendFile(path.join(publicDir, 'verificationsuccess.html'));
    } catch (error) {
        debug.error('verifyEmail', error, 1);
        debug.exit('verifyEmail', startTime, 'error', 1);
        res.sendFile(path.join(publicDir, 'verificationerror.html'));
    }
});

module.exports = router;