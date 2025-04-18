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

router.post('/api/signup', async (req, res) => {
    try {
        const { username, email, password } = req.body;

        if (!username || !email || !password) {
            return res.status(400).json({ error: 'All fields are required' });
        }

        const [existingUser] = await pool.query('SELECT * FROM users WHERE email = ? OR username = ?', [email, username]);
        if (existingUser.length > 0) {
            return res.status(400).json({ error: 'User with this email or username already exists.' });
        }

        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(password, salt);
        const verificationToken = jwt.sign({ email }, JWT_SECRET, { expiresIn: '1d' });

        pendingVerifications.set(email, {
            username,
            email,
            password: hashedPassword,
            verificationToken,
        });

        const verificationLink = `${process.env.BASE_URL}/verify?token=${verificationToken}`;
        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Verify Your Email',
            html: `Please click this link to verify your email: <a href="${verificationLink}">${verificationLink}</a>`,
        });

        res.status(200).json({ message: 'Please check your email to verify your account.' });
    } catch (error) {
        console.error('Signup error:', error);
        res.status(500).json({ error: 'Server error.' });
    }
});


router.get('/verify', async (req, res) => {
    try {
        const { token } = req.query;
        const decoded = jwt.verify(token, JWT_SECRET);
        const email = decoded.email;

        if (!pendingVerifications.has(email)) {
            return res.sendFile(path.join(publicDir, 'verificationerror.html'));
        }

        const userDetails = pendingVerifications.get(email);

        await pool.query('INSERT INTO users (username, email, password, is_verified) VALUES (?, ?, ?, 1)',
            [userDetails.username, userDetails.email, userDetails.password]);
        await pool.query('INSERT INTO user_profiles (user_id) VALUES (LAST_INSERT_ID())');

        const profileUrl = `/profile/${userDetails.username}`;
        await pool.query('UPDATE users SET profile_url = ? WHERE email = ?', [profileUrl, email]);
        pendingVerifications.delete(email);

        res.sendFile(path.join(publicDir, 'verificationsuccess.html'));
    } catch (error) {
        console.error('Verification error:', error);
        res.sendFile(path.join(publicDir, 'verificationerror.html'));
    }
});


module.exports = router;