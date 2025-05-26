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
const crypto = require('crypto');
const bcrypt = require('bcrypt');

const {
    pool,
    transporter
} = require('../../constants');


const debug = require('../../debug-helpers');

router.post('/api/forgot-password', async (req, res) => {
    const startTime = debug.enter('forgotPassword', [req.body.username, req.body.email], 1);
    const { username, email } = req.body;

    debug.level2('Forgot password request:', { username, email });

    try {
        debug.level3('Looking up user by username and email');
        debug.dbQuery('SELECT * FROM users WHERE username = ? AND email = ?', [username, email], 2);
        const [users] = await pool.query('SELECT * FROM users WHERE username = ? AND email = ?', [username, email]);
        debug.dbResult(users, 2);
        
        if (users.length === 0) {
            debug.level1('No user found with provided username and email');
            debug.exit('forgotPassword', startTime, 'user_not_found', 1);
            return res.status(400).json({ error: 'No user found with that username and email.' });
        }

        const user = users[0];
        const resetToken = crypto.randomBytes(32).toString('hex');
        const resetTokenExpiry = Date.now() + 3600000;

        debug.level3('Generating reset token and updating user record');
        await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?', [resetToken, resetTokenExpiry, user.id]);

        const resetLink = `${process.env.BASE_URL}/changepassword?token=${resetToken}`;
        debug.level2('Sending password reset email to:', email);
        
        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Password Reset',
            html: `Please click this link to reset your password: <a href="${resetLink}">${resetLink}</a>`
        });

        debug.level2('Password reset email sent successfully to:', email);
        debug.exit('forgotPassword', startTime, 'success', 1);
        res.json({ message: 'Password reset link sent to your email.' });
    } catch (error) {
        debug.error('forgotPassword', error, 1);
        debug.exit('forgotPassword', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});

router.post('/api/reset-password', async (req, res) => {
    const startTime = debug.enter('resetPassword', [req.query.token], 1);
    const { token } = req.query;
    const { password } = req.body;

    debug.level2('Password reset request with token');

    try {
        debug.level3('Validating reset token');
        debug.dbQuery('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()], 2);
        const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()]);
        debug.dbResult(users, 2);
        
        if (users.length === 0) {
            debug.level1('Invalid or expired reset token');
            debug.exit('resetPassword', startTime, 'invalid_token', 1);
            return res.status(400).json({ error: 'Invalid or expired password reset token.' });
        }

        const user = users[0];
        debug.level3('Hashing new password');
        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(password, salt);

        debug.level3('Updating user password and clearing reset token');
        await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

        debug.level2('Password reset successfully for user:', user.username);
        debug.exit('resetPassword', startTime, 'success', 1);
        res.json({ message: 'Password changed successfully.' });
    } catch (error) {
        debug.error('resetPassword', error, 1);
        debug.exit('resetPassword', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});

router.post('/api/request-password-change', async (req, res) => {
    const startTime = debug.enter('requestPasswordChange', [req.body.email], 1);
    const { email } = req.body;

    debug.level2('Password change request for email:', email);

    try {
        debug.level3('Looking up user by email');
        debug.dbQuery('SELECT * FROM users WHERE email = ?', [email], 2);
        const [users] = await pool.query('SELECT * FROM users WHERE email = ?', [email]);
        debug.dbResult(users, 2);
        
        if (users.length === 0) {
            debug.level1('No user found with provided email');
            debug.exit('requestPasswordChange', startTime, 'user_not_found', 1);
            return res.status(400).json({ error: 'No user found with that email.' });
        }

        const user = users[0];
        const changeToken = crypto.randomBytes(32).toString('hex');
        const changeTokenExpiry = Date.now() + 3600000;

        debug.level3('Generating change token and updating user record');
        await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?',
            [changeToken, changeTokenExpiry, user.id]);

        const changeLink = `${process.env.BASE_URL}/change-password?token=${changeToken}`;
        debug.level2('Sending password change email to:', email);

        await transporter.sendMail({
            from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
            to: email,
            subject: 'Password Change Request',
            html: `Please click this link to change your password: <a href="${changeLink}">${changeLink}</a>`
        });

        debug.level2('Password change email sent successfully to:', email);
        debug.exit('requestPasswordChange', startTime, 'success', 1);
        res.json({ message: 'Password change link sent to your email.' });
    } catch (error) {
        debug.error('requestPasswordChange', error, 1);
        debug.exit('requestPasswordChange', startTime, 'error', 1);
        res.status(500).json({ error: 'Server error. Please try again later.' });
    }
});

router.post('/api/change-password', async (req, res) => {
    const startTime = debug.enter('changePassword', [req.query.token], 1);
    const { token } = req.query;
    const { newPassword, confirmPassword } = req.body;

    debug.level2('Password change request with token');

    if (!token || !newPassword || newPassword !== confirmPassword) {
        debug.level1('Invalid request or passwords do not match');
        debug.exit('changePassword', startTime, 'invalid_request', 1);
        return res.status(400).json({ error: 'Invalid request or passwords do not match.' });
    }

    try {
        debug.level3('Validating change token');
        debug.dbQuery('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()], 2);
        const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?',
            [token, Date.now()]);
        debug.dbResult(users, 2);

        if (users.length === 0) {
            debug.level1('Invalid or expired change token');
            debug.exit('changePassword', startTime, 'invalid_token', 1);
            return res.status(400).json({ error: 'Invalid or expired token.' });
        }

        const user = users[0];
        debug.level3('Hashing new password');
        const salt = await bcrypt.genSalt(10);
        const hashedPassword = await bcrypt.hash(newPassword, salt);

        debug.level3('Updating user password and clearing change token');
        await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?',
            [hashedPassword, user.id]);

        debug.level2('Password changed successfully for user:', user.username);
        debug.exit('changePassword', startTime, 'success', 1);
        res.json({ message: 'Password changed successfully.' });
    } catch (error) {
        debug.error('changePassword', error, 1);
        debug.exit('changePassword', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to change password.' });
    }
});

module.exports = router;