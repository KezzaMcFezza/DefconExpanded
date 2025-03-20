const path = require('path');
const fs = require('fs');
const { pool } = require('./database-backup');
const utils = require('../Utils/shared-utils');

// Initialize admin functionality
const initializeAdmin = async () => {
  console.log('Initializing admin functionality');
  
  try {
    // Any admin initialization can go here
    console.log('Admin functionality initialized successfully');
  } catch (error) {
    console.error('Error initializing admin functionality:', error);
  }
};

// Get monitoring data for admin panel
const getMonitoringData = async () => {
  try {
    const startTime = global.startTime || Date.now();
    const uptime = Math.floor((Date.now() - startTime) / 1000);

    const [totalDemosResult] = await pool.query('SELECT COUNT(*) as count FROM demos');
    const totalDemos = totalDemosResult[0].count;

    const [pendingRequestsResult] = await pool.query(`
      SELECT 
        (SELECT COUNT(*) FROM demo_reports WHERE status = 'pending') +
        (SELECT COUNT(*) FROM mod_reports WHERE status = 'pending') as total_pending
    `);
    const userRequests = pendingRequestsResult[0].total_pending;

    return {
      uptime,
      totalDemos,
      userRequests,
    };
  } catch (error) {
    console.error('Error fetching monitoring data:', error);
    throw error;
  }
};

// Get configuration files - updated path to go up two directory levels
const getConfigFiles = async () => {
  try {
    const configPath = path.join(__dirname, '..', '..');
    console.log(`Current working directory: ${process.cwd()}`);
    console.log(`Searching for config files in: ${configPath}`);

    let existingFiles = [];
    for (const filename of utils.configFiles) {
      const filePath = path.join(configPath, filename);
      try {
        await fs.promises.access(filePath, fs.constants.R_OK);
        existingFiles.push(filename);
        console.log(`Found file: ${filePath}`);
      } catch (err) {
        console.log(`File not found: ${filePath}`);
      }
    }
    return existingFiles;
  } catch (error) {
    console.error('Error reading config files:', error);
    throw error;
  }
};

// Get content of a specific config file - updated path to go up two directory levels
const getConfigFileContent = async (filename) => {
  try {
    if (!utils.configFiles.includes(filename)) {
      throw new Error('Invalid file');
    }
    const filePath = path.join(__dirname, '..', '..', filename);
    const content = await fs.promises.readFile(filePath, 'utf8');
    return content;
  } catch (error) {
    console.error('Error reading file:', error);
    throw error;
  }
};

// Update content of a specific config file - updated path to go up two directory levels
const updateConfigFile = async (filename, content) => {
  try {
    if (!utils.configFiles.includes(filename)) {
      throw new Error('Invalid file');
    }
    const filePath = path.join(__dirname, '..', '..', filename);
    await fs.promises.writeFile(filePath, content);
    return true;
  } catch (error) {
    console.error('Error writing file:', error);
    throw error;
  }
};

// Get all pending reports for admin dashboard
const getPendingReports = async () => {
  try {
    const [reports] = await pool.query(`
      SELECT dr.*, d.game_type, d.players, u.username 
      FROM demo_reports dr 
      JOIN demos d ON dr.demo_id = d.id 
      JOIN users u ON dr.user_id = u.id 
      WHERE dr.status = 'pending'
    `);
    return reports;
  } catch (error) {
    console.error('Error fetching pending reports:', error);
    throw error;
  }
};

// Get all pending mod reports for admin dashboard
const getPendingModReports = async () => {
  try {
    const [reports] = await pool.query(`
      SELECT mr.*, m.name as mod_name, u.username 
      FROM mod_reports mr 
      JOIN modlist m ON mr.mod_id = m.id 
      JOIN users u ON mr.user_id = u.id 
      WHERE mr.status = 'pending'
    `);
    return reports;
  } catch (error) {
    console.error('Error fetching pending mod reports:', error);
    throw error;
  }
};

// Resolve a demo report
const resolveReport = async (reportId) => {
  try {
    await pool.query('UPDATE demo_reports SET status = "resolved" WHERE id = ?', [reportId]);
    return true;
  } catch (error) {
    console.error('Error resolving report:', error);
    throw error;
  }
};

// Resolve a mod report
const resolveModReport = async (reportId) => {
  try {
    await pool.query('UPDATE mod_reports SET status = "resolved" WHERE id = ?', [reportId]);
    return true;
  } catch (error) {
    console.error('Error resolving mod report:', error);
    throw error;
  }
};

// Get all pending user requests (name changes, blacklisting, etc.)
const getPendingUserRequests = async () => {
  try {
    const [nameChangeRequests] = await pool.query(`
      SELECT lnc.*, u.username
      FROM leaderboard_name_change_requests lnc
      JOIN users u ON u.id = lnc.user_id
      WHERE lnc.status = "pending"
    `);

    const [blacklistRequests] = await pool.query(`
      SELECT bl.*, u.username
      FROM blacklist_requests bl
      JOIN users u ON u.id = bl.user_id
      WHERE bl.status = "pending"
    `);

    const [deletionRequests] = await pool.query(`
      SELECT ad.*, u.username
      FROM account_deletion_requests ad
      JOIN users u ON u.id = ad.user_id
      WHERE ad.status = "pending"
    `);

    const [usernameChangeRequests] = await pool.query(`
      SELECT uc.*, u.username
      FROM username_change_requests uc
      JOIN users u ON u.id = uc.user_id
      WHERE uc.status = "pending"
    `);

    const [emailChangeRequests] = await pool.query(`
      SELECT ec.*, u.username
      FROM email_change_requests ec
      JOIN users u ON u.id = ec.user_id
      WHERE ec.status = "pending"
    `);

    const allRequests = [
      ...nameChangeRequests.map(r => ({ ...r, type: 'leaderboard_name_change' })),
      ...blacklistRequests.map(r => ({ ...r, type: 'blacklist' })),
      ...deletionRequests.map(r => ({ ...r, type: 'account_deletion' })),
      ...usernameChangeRequests.map(r => ({ ...r, type: 'username_change' })),
      ...emailChangeRequests.map(r => ({ ...r, type: 'email_change' }))
    ];

    return allRequests;
  } catch (error) {
    console.error('Error fetching pending user requests:', error);
    throw error;
  }
};

// Resolve a user request
const resolveUserRequest = async (requestId, requestType, status) => {
  try {
    let tableName;
    switch (requestType) {
      case 'leaderboard_name_change':
        tableName = 'leaderboard_name_change_requests';
        break;
      case 'blacklist':
        tableName = 'blacklist_requests';
        break;
      case 'account_deletion':
        tableName = 'account_deletion_requests';
        break;
      case 'username_change':
        tableName = 'username_change_requests';
        break;
      case 'email_change':
        tableName = 'email_change_requests';
        break;
      default:
        throw new Error('Invalid request type');
    }

    await pool.query(`UPDATE ${tableName} SET status = ? WHERE id = ?`, [status, requestId]);
    return true;
  } catch (error) {
    console.error('Error resolving user request:', error);
    throw error;
  }
};

module.exports = {
  initializeAdmin,
  getMonitoringData,
  getConfigFiles,
  getConfigFileContent,
  updateConfigFile,
  getPendingReports,
  getPendingModReports,
  resolveReport,
  resolveModReport,
  getPendingUserRequests,
  resolveUserRequest
};