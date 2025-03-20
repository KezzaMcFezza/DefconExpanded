const nodemailer = require('nodemailer');

// Email transporter setup
const transporter = nodemailer.createTransport({
  host: "smtp.gmail.com",
  port: 587,
  secure: false,
  auth: {
    user: "keiron.mcphee1@gmail.com",
    pass: "ueiz tkqy uqwj lwht"
  }
});

// Function to send verification email
const sendVerificationEmail = async (email, verificationLink) => {
  try {
    const info = await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Verify Your Email',
      html: `
        <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; border: 1px solid #eee; border-radius: 5px;">
          <h2 style="color: #333;">Welcome to Defcon Expanded!</h2>
          <p>Thank you for registering. Please click the button below to verify your email address:</p>
          <div style="text-align: center; margin: 30px 0;">
            <a href="${verificationLink}" style="background-color: #4CAF50; color: white; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold;">Verify Email</a>
          </div>
          <p>If the button doesn't work, you can also click this link:</p>
          <p><a href="${verificationLink}">${verificationLink}</a></p>
          <p>This link will expire in 24 hours.</p>
          <p>If you didn't sign up for Defcon Expanded, you can safely ignore this email.</p>
        </div>
      `
    });

    console.log('Verification email sent:', info.messageId);
    return true;
  } catch (error) {
    console.error('Error sending verification email:', error);
    return false;
  }
};

// Function to send password reset email
const sendPasswordResetEmail = async (email, resetLink) => {
  try {
    const info = await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Password Reset',
      html: `
        <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; border: 1px solid #eee; border-radius: 5px;">
          <h2 style="color: #333;">Password Reset Request</h2>
          <p>We received a request to reset your password. Please click the button below to create a new password:</p>
          <div style="text-align: center; margin: 30px 0;">
            <a href="${resetLink}" style="background-color: #4CAF50; color: white; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold;">Reset Password</a>
          </div>
          <p>If the button doesn't work, you can also click this link:</p>
          <p><a href="${resetLink}">${resetLink}</a></p>
          <p>This link will expire in 1 hour.</p>
          <p>If you didn't request a password reset, you can safely ignore this email.</p>
        </div>
      `
    });

    console.log('Password reset email sent:', info.messageId);
    return true;
  } catch (error) {
    console.error('Error sending password reset email:', error);
    return false;
  }
};

// Function to send password change confirmation
const sendPasswordChangeConfirmation = async (email) => {
  try {
    const info = await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: 'Password Changed Successfully',
      html: `
        <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; border: 1px solid #eee; border-radius: 5px;">
          <h2 style="color: #333;">Password Changed Successfully</h2>
          <p>Your password has been changed successfully. If you did not make this change, please contact us immediately.</p>
          <p>Thank you for using Defcon Expanded!</p>
        </div>
      `
    });

    console.log('Password change confirmation email sent:', info.messageId);
    return true;
  } catch (error) {
    console.error('Error sending password change confirmation email:', error);
    return false;
  }
};

// Function to send notification email (e.g., for admin alerts)
const sendNotificationEmail = async (email, subject, message) => {
  try {
    const info = await transporter.sendMail({
      from: '"Defcon Expanded" <keiron.mcphee1@gmail.com>',
      to: email,
      subject: subject,
      html: `
        <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; border: 1px solid #eee; border-radius: 5px;">
          <h2 style="color: #333;">${subject}</h2>
          <p>${message}</p>
          <p>Thank you for using Defcon Expanded!</p>
        </div>
      `
    });

    console.log('Notification email sent:', info.messageId);
    return true;
  } catch (error) {
    console.error('Error sending notification email:', error);
    return false;
  }
};

module.exports = {
  transporter,
  sendVerificationEmail,
  sendPasswordResetEmail,
  sendPasswordChangeConfirmation,
  sendNotificationEmail
};