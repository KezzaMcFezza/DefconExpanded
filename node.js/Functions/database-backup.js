const mysql = require('mysql2/promise');
const nodemailer = require('nodemailer');

// Database connection
const pool = mysql.createPool({
  host: 'localhost',
  user: 'root',
  password: 'cca3d38e2b',
  database: 'defcon_demos',
  connectionLimit: 20,
  queueLimit: 0,
  waitForConnections: true,
  enableKeepAlive: true,
  keepAliveInitialDelay: 10000,
  connectTimeout: 10000,
});

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

// Check database connection
const checkDatabaseConnection = () => {
  pool.getConnection((err, connection) => {
    if (err) {
      console.error('Error connecting to the database:', err);
    } else {
      console.log('Successfully connected to the database');
      connection.release();
    }
  });
};

// Create backup
const createBackup = async () => {
  // Implementation for database backup functionality
  console.log('Database backup function called');
  // Additional backup logic would go here
};

module.exports = {
  pool,
  transporter,
  checkDatabaseConnection,
  createBackup
};