const bcrypt = require('bcrypt');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const path = require('path');
const { pool, transporter } = require('./database-backup');

// JWT secret
const JWT_SECRET = 'a8001708554bfcb4bf707fece608fcce49c3ce0d88f5126b137d82b3c7aeab65';

// Map to store pending verifications
const pendingVerifications = new Map();

// Register a new user
const registerUser = async (username, email, password) => {
  try {
    if (!username || !email || !password) {
      throw new Error('All fields are required');
    }

    // Check if user already exists
    const [existingUser] = await pool.query('SELECT * FROM users WHERE email = ? OR username = ?', [email, username]);
    if (existingUser.length > 0) {
      throw new Error('User with this email or username already exists');
    }

    // Hash the password
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(password, salt);

    // Create verification token
    const verificationToken = jwt.sign({ email }, JWT_SECRET, { expiresIn: '1d' });

    // Store pending verification
    pendingVerifications.set(email, {
      username,
      email,
      password: hashedPassword,
      verificationToken,
    });

    // Send verification email
    const verificationLink = `https://defconexpanded.com/verify?token=${verificationToken}`;
    await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Verify Your Email',
      html: `Please click this link to verify your email: <a href="${verificationLink}">${verificationLink}</a>`,
    });

    return { success: true, message: 'Please check your email to verify your account.' };
  } catch (error) {
    console.error('Registration error:', error);
    throw error;
  }
};

// Verify user email
const verifyUser = async (token) => {
  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    const email = decoded.email;

    // Check if user info exists in pending verifications
    if (!pendingVerifications.has(email)) {
      return { success: false, message: 'Invalid or expired verification token' };
    }

    const userDetails = pendingVerifications.get(email);

    // Insert user into database
    await pool.query('INSERT INTO users (username, email, password, is_verified) VALUES (?, ?, ?, 1)',
      [userDetails.username, userDetails.email, userDetails.password]);
    
    await pool.query('INSERT INTO user_profiles (user_id) VALUES (LAST_INSERT_ID())');

    // Create profile URL
    const profileUrl = `/profile/${userDetails.username}`;
    await pool.query('UPDATE users SET profile_url = ? WHERE email = ?', [profileUrl, email]);
    
    // Remove from pending verifications
    pendingVerifications.delete(email);

    return { success: true, message: 'Email verified successfully!' };
  } catch (error) {
    console.error('Verification error:', error);
    throw error;
  }
};

// Login user
const loginUser = async (username, password, rememberMe) => {
  try {
    // Check if user exists
    const [users] = await pool.query('SELECT * FROM users WHERE username = ?', [username]);
    if (users.length === 0) {
      throw new Error('Invalid username or password');
    }

    const user = users[0];

    // Check password
    const isPasswordValid = await bcrypt.compare(password, user.password);
    if (!isPasswordValid) {
      throw new Error('Invalid username or password');
    }

    // Check if user is verified
    if (!user.is_verified) {
      throw new Error('Please verify your email before logging in');
    }

    // Generate token
    const token = jwt.sign(
      { id: user.id, username: user.username, role: user.role },
      JWT_SECRET,
      { expiresIn: rememberMe ? '30d' : '1d' }
    );

    return {
      success: true,
      token,
      user: {
        id: user.id,
        username: user.username,
        role: user.role
      }
    };
  } catch (error) {
    console.error('Login error:', error);
    throw error;
  }
};

// Request password reset
const requestPasswordReset = async (username, email) => {
  try {
    // Check if user exists with given username and email
    const [users] = await pool.query('SELECT * FROM users WHERE username = ? AND email = ?', [username, email]);
    if (users.length === 0) {
      throw new Error('No user found with that username and email');
    }

    const user = users[0];
    const resetToken = crypto.randomBytes(32).toString('hex');
    const resetTokenExpiry = Date.now() + 3600000; // 1 hour

    // Store reset token in database
    await pool.query('UPDATE users SET reset_token = ?, reset_token_expiry = ? WHERE id = ?', [resetToken, resetTokenExpiry, user.id]);

    // Send reset email
    const resetLink = `https://defconexpanded.com/changepassword?token=${resetToken}`;
    await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Password Reset',
      html: `Please click this link to reset your password: <a href="${resetLink}">${resetLink}</a>`
    });

    return { success: true, message: 'Password reset link sent to your email.' };
  } catch (error) {
    console.error('Password reset request error:', error);
    throw error;
  }
};

// Reset password
const resetPassword = async (token, password) => {
  try {
    // Check if token is valid
    const [users] = await pool.query('SELECT * FROM users WHERE reset_token = ? AND reset_token_expiry > ?', [token, Date.now()]);
    if (users.length === 0) {
      throw new Error('Invalid or expired password reset token');
    }

    const user = users[0];
    
    // Hash new password
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(password, salt);

    // Update password and clear reset token
    await pool.query('UPDATE users SET password = ?, reset_token = NULL, reset_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

    return { success: true, message: 'Password changed successfully.' };
  } catch (error) {
    console.error('Password reset error:', error);
    throw error;
  }
};

// Request password change (for logged in users)
const requestPasswordChange = async (email) => {
  try {
    // Check if user exists
    const [users] = await pool.query('SELECT * FROM users WHERE email = ?', [email]);
    if (users.length === 0) {
      throw new Error('No user found with that email');
    }

    const user = users[0];
    const changeToken = crypto.randomBytes(32).toString('hex');
    const changeTokenExpiry = Date.now() + 3600000; // 1 hour

    // Store change token in database
    await pool.query('UPDATE users SET change_password_token = ?, change_password_token_expiry = ? WHERE id = ?', [changeToken, changeTokenExpiry, user.id]);

    // Send email
    const changeLink = `https://defconexpanded.com/change-password?token=${changeToken}`;
    await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Password Change Request',
      html: `Please click this link to change your password: <a href="${changeLink}">${changeLink}</a>`
    });

    return { success: true, message: 'Password change link sent to your email.' };
  } catch (error) {
    console.error('Password change request error:', error);
    throw error;
  }
};

// Change password
const changePassword = async (token, newPassword, confirmPassword) => {
  try {
    if (!token || !newPassword || newPassword !== confirmPassword) {
      throw new Error('Invalid request or passwords do not match');
    }

    // Check if token is valid
    const [users] = await pool.query('SELECT * FROM users WHERE change_password_token = ? AND change_password_token_expiry > ?', [token, Date.now()]);
    if (users.length === 0) {
      throw new Error('Invalid or expired token');
    }

    const user = users[0];
    
    // Hash new password
    const salt = await bcrypt.genSalt(10);
    const hashedPassword = await bcrypt.hash(newPassword, salt);

    // Update password and clear token
    await pool.query('UPDATE users SET password = ?, change_password_token = NULL, change_password_token_expiry = NULL WHERE id = ?', [hashedPassword, user.id]);

    return { success: true, message: 'Password changed successfully.' };
  } catch (error) {
    console.error('Password change error:', error);
    throw error;
  }
};

// Clean up expired verification tokens
const cleanupExpiredVerifications = () => {
  const now = Date.now();
  for (let [email, { verificationToken }] of pendingVerifications) {
    try {
      jwt.verify(verificationToken, JWT_SECRET);
    } catch (error) {
      // Token is expired
      pendingVerifications.delete(email);
    }
  }
};

// Run cleanup every hour
setInterval(cleanupExpiredVerifications, 3600000);

module.exports = {
  registerUser,
  verifyUser,
  loginUser,
  requestPasswordReset,
  resetPassword,
  requestPasswordChange,
  changePassword,
  pendingVerifications
};