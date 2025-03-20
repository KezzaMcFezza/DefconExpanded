/**
 * Centralizes all route registration to keep server.js clean
 */
const path = require('path');
const fs = require('fs');
const utils = require('./shared-utils');
const { errorMiddleware } = require('./error-handler');

// Import routes
const adminApi = require('../API/admin-api');
const dedconBuildsApi = require('../API/dedcon-builds-api');
const demosApi = require('../API/demos-api');
const expandedBuildsApi = require('../API/expanded-builds-api');
const leaderboardApi = require('../API/leaderboard-api');
const loginSignupApi = require('../API/login-signup-api');
const modlistApi = require('../API/modlist-api');
const profileApi = require('../API/profile-api');
const searchApi = require('../API/search-api');
const uniqueApi = require('../API/unique-api');
const userReportsApi = require('../API/user-reports-api');

// Setup all API routes
const setupRoutes = (app) => {
  // Apply API routes
  app.use('/api/admin', adminApi);
  app.use('/api/dedcon-builds', dedconBuildsApi);
  app.use('/api/demos', demosApi);
  app.use('/api/expanded-builds', expandedBuildsApi);
  app.use('/api/leaderboard', leaderboardApi);
  app.use('/api', loginSignupApi);
  app.use('/api/modlist', modlistApi);
  app.use('/api/profile', profileApi);
  app.use('/api/search', searchApi);
  app.use('/api', uniqueApi);
  app.use('/api/user-reports', userReportsApi);

  // Set up page routes
  setupPageRoutes(app);
  
  // 404 handler for API routes
  app.use('/api/*', (req, res) => {
    res.status(404).json({ error: 'API endpoint not found' });
  });

  // Global error handler
  app.use(errorMiddleware);

  // 404 fallback for all other routes
  app.get('*', (req, res) => {
    const requestedPath = path.join(__dirname, '..', '..', 'public', req.path);
    if (fs.existsSync(requestedPath) && fs.statSync(requestedPath).isFile()) {
      res.sendFile(requestedPath);
    } else {
      res.status(404).sendFile(path.join(__dirname, '..', '..', 'public', '404.html'));
    }
  });
};

// Setup all page routes - updated paths to go up two directory levels
const setupPageRoutes = (app) => {
  // Static pages
  app.get('/about', (req, res) => utils.sendHtml(res, 'about.html'));
  app.get('/about/combined-servers', (req, res) => utils.sendHtml(res, 'combinedservers.html'));
  app.get('/about/hours-played', (req, res) => utils.sendHtml(res, 'totalhoursgraph.html'));
  app.get('/about/popular-territories', (req, res) => utils.sendHtml(res, 'popularterritories.html'));
  app.get('/about/1v1-setup-statistics', (req, res) => utils.sendHtml(res, '1v1setupstatistics.html'));
  app.get('/guides', (req, res) => utils.sendHtml(res, 'guides.html'));
  app.get('/resources', (req, res) => utils.sendHtml(res, 'resources.html'));
  app.get('/laikasdefcon', (req, res) => utils.sendHtml(res, 'laikasdefcon.html'));
  app.get('/homepage/matchroom', (req, res) => utils.sendHtml(res, 'matchroom.html'));
  app.get('/homepage', (req, res) => utils.sendHtml(res, 'index.html'));
  app.get('/dedcon-builds', (req, res) => utils.sendHtml(res, 'dedconbuilds.html'));
  app.get('/patchnotes', (req, res) => utils.sendHtml(res, 'patchnotes.html'));
  app.get('/issue-report', (req, res) => utils.sendHtml(res, 'bugreport.html'));
  app.get('/phpmyadmin', (req, res) => utils.sendHtml(res, 'idiot.html')); // for the memes
  app.get('/leaderboard', (req, res) => utils.sendHtml(res, 'leaderboard.html'));
  app.get('/modlist', (req, res) => utils.sendHtml(res, 'modlist.html'));
  app.get('/privacypolicy', (req, res) => utils.sendHtml(res, 'privacypolicy.html'));
  app.get('/modlist/maps', (req, res) => utils.sendHtml(res, 'mapmods.html'));
  app.get('/modlist/graphics', (req, res) => utils.sendHtml(res, 'graphicmods.html'));
  app.get('/modlist/overhauls', (req, res) => utils.sendHtml(res, 'overhaulmods.html'));
  app.get('/modlist/moddingtools', (req, res) => utils.sendHtml(res, 'moddingtools.html'));
  app.get('/modlist/ai', (req, res) => utils.sendHtml(res, 'aimods.html'));
  app.get('/signup', (req, res) => utils.sendHtml(res, 'signuppage.html'));
  app.get('/forgotpassword', (req, res) => utils.sendHtml(res, 'forgotpasswordfor816788487.html'));
  app.get('/changepassword', (req, res) => utils.sendHtml(res, 'changepassword248723424.html'));

  // Special routes
  app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'index.html'));
  });

  app.get('/demos/page/:pageNumber', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'index.html'));
  });

  app.get('/account-settings', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'profilesettings.html'));
  });

  app.get('/change-password', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'profilesettings.html'));
  });

  app.get('/sitemap', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'sitemap.xml'));
  });

  app.get('/site.webmanifest', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'site.webmanifest'));
  });

  app.get('/browserconfig.xml', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'browserconfig.xml'));
  });

  app.get('/login', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'loginpage.html'));
  });

  app.get('/profile/:username', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'public', 'profile.html'));
  });
};

module.exports = {
  setupRoutes
};