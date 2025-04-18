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
const crypto = require('crypto');
const bcrypt = require('bcrypt');

const {
    pool,
    transporter
} = require('../../constants');

router.post('/api/forgot-password', async (req, res) => {
    const { username, email } = req.body;

    try {
        const [users] = await pool.query('SELECT * FROM users WHERE username = ? AND email = ?', [username, email]);
        if (users.length === 0) {
            return res.status(400).json({ error: 'No user found with that username and email.' });
        }

        const user = users[0];
        const resetToken = crypto.randomBytes(32).toString('hex');
        const resetTokenExpiry = Date.now() + 3600000;

        await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?', [resetToken, resetTokenExpiry, user.id]);

        const resetLink = `${process.env.BASE_URL}/changepassword?token=${resetToken}`;
        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Password Reset',
            html: `Please click this link to reset your password: <a href="${resetLink}">${resetLink}</a>`
        });

        res.json({ message: 'Password reset link sent to your email.' });
    } catch (error) {
        console.error('Forgot password error:', error);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});


router.post('/api/reset-password', async (req, res) => {
    const { token } = req.query;
    const { password } = req.body;

    try {
        const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()]);
        if (users.length === 0) {
            return res.status(400).json({ error: 'Invalid or expired password reset token.' });
        }

        const user = users[0];
        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(password, salt);

        await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

        res.json({ message: 'Password changed successfully.' });
    } catch (error) {
        console.error('Reset password error:', error);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});

router.post('/api/request-password-change', async (req, res) => {
    const { email } = req.body;

    try {
        const [users] = await pool.query('SELECT * FROM users WHERE email = ?', [email]);
        if (users.length === 0) {
            return res.status(400).json({ error: 'No user found with that email.' });
        }

        const user = users[0];
        const changeToken = crypto.randomBytes(32).toString('hex');
        const changeTokenExpiry = Date.now() + 3600000;

        await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?',
            [changeToken, changeTokenExpiry, user.id]);

        const changeLink = `${process.env.BASE_URL}/change-password?token=${changeToken}`;

        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Password Change Request',
            html: `Please click this link to change your password: <a href="${changeLink}">${changeLink}</a>`
        });

        res.json({ message: 'Password change link sent to your email.' });
    } catch (error) {
        console.error('Password change request error:', error);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});

router.post('/api/change-password', async (req, res) => {
    const { token } = req.query;
    const { newPassword, confirmPassword } = req.body;

    if (!token || !newPassword || newPassword !== confirmPassword) {
        return res.status(400).json({ error: 'Invalid request or passwords do not match.' });
    }

    try {
        const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?',
            [token, Date.now()]);

        if (users.length === 0) {
            return res.status(400).json({ error: 'Invalid or expired token.' });
        }

        const user = users[0];
        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(newPassword, salt);

        await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?',
            [hashedPassword, user.id]);

        res.json({ message: 'Password changed successfully.' });
    } catch (error) {
        console.error('Error changing password:', error);
        res.status(500).json({ error: 'Failed to change password.' });
    }
});

module.exports = router;