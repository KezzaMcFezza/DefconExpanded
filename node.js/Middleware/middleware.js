const express = require('express');
const cookieParser = require('cookie-parser');
const session = require('express-session');
const timeout = require('connect-timeout');
const jwt = require('jsonwebtoken');
const path = require('path');
const multer = require('multer');
const fs = require('fs');
const crypto = require('crypto');

// Import utils
const utils = require('../Utils/shared-utils');

// JWT secret
const JWT_SECRET = 'a8001708554bfcb4bf707fece608fcce49c3ce0d88f5126b137d82b3c7aeab65';

// Directories setup for multer - adjusted paths to go up two directory levels
const demoDir = path.join(__dirname, '..', '..', 'demo_recordings');
const resourcesDir = path.join(__dirname, '..', '..', 'public', 'Files');
const dedconBuildsDir = path.join(__dirname, '..', '..', 'public', 'Files');
const uploadDir = path.join(__dirname, '..', '..', 'public');
const gameLogsDir = path.join(__dirname, '..', '..', 'game_logs');
const modlistDir = path.join(__dirname, '..', '..', 'public', 'modlist');
const modPreviewsDir = path.join(__dirname, '..', '..', 'public', 'modpreviews');

// Make sure the specified folders exist and create them if they don't
const ensureDirectoriesExist = () => {
  [demoDir, resourcesDir, dedconBuildsDir, uploadDir, gameLogsDir, modlistDir, modPreviewsDir].forEach(dir => {
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
  });
};

// Multer configuration
const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    if (file.fieldname === 'demoFile') {
      cb(null, demoDir);
    } else if (file.fieldname === 'jsonFile') {
      cb(null, gameLogsDir);
    } else if (file.fieldname === 'resourceFile') {
      cb(null, dedconBuildsDir);
    } else if (file.fieldname === 'dedconBuildsFile') {
      cb(null, resourcesDir);
    } else if (file.fieldname === 'modFile') {
      cb(null, modlistDir);
    } else if (file.fieldname === 'previewImage') {
      cb(null, modPreviewsDir);
    } else if (file.fieldname === 'image') {
      cb(null, path.join(__dirname, '..', '..', 'public', 'uploads'));
    } else {
      cb(new Error('Invalid file type'));
    }
  },
  filename: function (req, file, cb) {
    if (file.fieldname === 'image') {
      const userId = req.user ? req.user.id : 'unknown';
      const fileExtension = path.extname(file.originalname);
      const randomString = crypto.randomBytes(8).toString('hex');
      const newFilename = `${userId}_${randomString}${fileExtension}`;
      cb(null, newFilename);
    } else {
      cb(null, file.originalname);
    }
  }
});

const upload = multer({ storage: storage });

// Error handler middleware
const errorHandler = (err, req, res, next) => {
  console.error('Unhandled error:', err);
  res.status(500).json({ error: 'Internal server error', details: err.message });
};

// Check authentication token middleware
const checkAuthToken = (req, res, next) => {
  const token = req.cookies.token;
  if (token) {
    jwt.verify(token, JWT_SECRET, (err, user) => {
      if (err) {
        res.locals.user = null;
      } else {
        res.locals.user = { id: user.id, username: user.username };
      }
      next();
    });
  } else {
    res.locals.user = null;
    next();
  }
};

// Authenticate token middleware
const authenticateToken = async (req, res, next) => {
  const token = req.cookies.token;

  if (!token) {
    console.log('No token found');
    return res.status(401).json({ error: 'Not authenticated' });
  }

  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    const { pool } = require('../Functions/database-backup');

    // Check the database for the user
    const [users] = await pool.query('SELECT id, username, role FROM users WHERE id = ?', [decoded.id]);
    if (users.length === 0) {
      console.log('Authentication failed: User not found in database');
      return res.status(403).json({ error: 'Invalid token' });
    }

    req.user = users[0];
    next();
  } catch (err) {
    console.log('Authentication failed: Invalid token', err);
    return res.status(403).json({ error: 'Invalid token' });
  }
};

// Check role middleware
const checkRole = (requiredRole) => {
  return (req, res, next) => {
    if (!req.user) {
      return res.status(401).json({ error: 'Not authenticated' });
    }
    if (req.user.role <= requiredRole) {
      next();
    } else {
      return res.status(403).json({ error: 'Insufficient permissions' });
    }
  };
};

// Admin pages protection middleware
const adminPagesProtection = (req, res, next) => {
  const requestedFile = path.basename(req.url);
  if (utils.adminPages.includes(requestedFile)) {
    return res.status(404).sendFile(path.join(__dirname, '..', '..', 'public', '404.html'));
  }
  next();
};

// Middleware to remove timeout for specific routes
const removeTimeout = (req, res, next) => {
  req.setTimeout(0);
  res.setTimeout(0);
  next();
};

// Setup all middleware
const setupMiddleware = (app) => {
  ensureDirectoriesExist();
  
  app.use(cookieParser());
  app.use(express.json());
  app.use(express.urlencoded({ extended: true }));
  app.use('/uploads', express.static(path.join(__dirname, '..', '..', 'uploads')));
  app.use(adminPagesProtection);
  
  app.use(session({
    secret: JWT_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: { secure: process.env.NODE_ENV === 'true' }
  }));
  
  // Add timeout
  app.use(timeout('1h'));
  
  // Check for timedout requests
  app.use((req, res, next) => {
    if (!req.timedout) next();
  });
  
  // Site manifest for google console
  app.use((req, res, next) => {
    if (req.url.endsWith('.webmanifest')) {
      res.setHeader('Content-Type', 'application/manifest+json');
    }
    next();
  });
  
  // HTTPS enforcement
  app.use((req, res, next) => {
    if (req.headers['x-forwarded-proto'] === 'https') {
      res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
    }
    next();
  });
  
  // Session token logic
  app.use((req, res, next) => {
    if (req.cookies.token) {
      try {
        const decoded = jwt.verify(req.cookies.token, JWT_SECRET);
        req.user = decoded;
        if (!req.session.loginTime) {
          req.session.loginTime = Date.now();
        }
      } catch (err) {
        // clears the cookie when a user is logged out or their session has expired
        res.clearCookie('token');
      }
    }
    next();
  });
  
  // Set up static serving of public directory
  app.use(express.static(path.join(__dirname, '..', '..', 'public'), {
    setHeaders: (res, filePath, stat) => {
      if (filePath.endsWith('.html') && utils.adminPages.includes(path.basename(filePath))) {
        res.set('Content-Type', 'text/plain');
      }
    }
  }));
  
  // Error handler
  app.use(errorHandler);
};

module.exports = {
  setupMiddleware,
  authenticateToken,
  checkAuthToken,
  checkRole,
  removeTimeout,
  upload,
  errorHandler
};