const path = require('path');
const fs = require('fs');

// Admin pages list
const adminPages = [
  'admin-panel.html',
  'blacklist.html',
  'demo-manage.html',
  'leaderboard-manage.html',
  'account-manage.html',
  'modmanagment.html',
  'resourcemanagment.html',
  'server-console.html',
  'playerlookup.html',
  'dedconmanagment.html',
  'servermanagment.html'
];

// Config files
const configFiles = [
  '1v1config.txt',
  '1v1configbest2.txt',
  '1v1configbest.txt',
  '1v1configdefault.txt',
  '1v1configtest.txt',
  '2v2config.txt',
  '2v2tournament.txt',
  '6playerffaconfig.txt',
  'noobfriendly.txt',
  '3v3ffaconfig.txt',
  'hawhaw1v1config.txt',
  'hawhaw2v2config.txt',
  '1v1muricon.txt',
  '4v4config.txt',
  '5v5config.txt',
  '8playerdiplo.txt',
  '10playerdiplo.txt',
  '16playerconfig.txt'
];

// Format timestamp for logs
function formatTimestamp(date) {
  const pad = (num) => (num < 10 ? '0' + num : num);

  const year = date.getFullYear();
  const month = pad(date.getMonth() + 1);
  const day = pad(date.getDate());
  const hours = pad(date.getHours());
  const minutes = pad(date.getMinutes());
  const seconds = pad(date.getSeconds());
  const milliseconds = pad(Math.floor(date.getMilliseconds() / 10));

  return `${year}-${month}-${day}-${hours}:${minutes}:${seconds}.${milliseconds}`;
}

// Get client IP address
function getClientIp(req) {
  const forwardedFor = req.headers['x-forwarded-for'];
  if (forwardedFor) {
    return forwardedFor.split(',')[0].trim();
  }
  return (req.ip || req.connection.remoteAddress).replace(/^::ffff:/, '');
}

// Levenshtein distance for fuzzy search
const levenshteinDistance = (a, b) => {
  if (a.length === 0) return b.length;
  if (b.length === 0) return a.length;

  const matrix = [];

  for (let i = 0; i <= b.length; i++) {
    matrix[i] = [i];
  }

  for (let j = 0; j <= a.length; j++) {
    matrix[0][j] = j;
  }

  for (let i = 1; i <= b.length; i++) {
    for (let j = 1; j <= a.length; j++) {
      if (b.charAt(i - 1) === a.charAt(j - 1)) {
        matrix[i][j] = matrix[i - 1][j - 1];
      } else {
        matrix[i][j] = Math.min(
          matrix[i - 1][j - 1] + 1,
          Math.min(
            matrix[i][j - 1] + 1,
            matrix[i - 1][j] + 1
          )
        );
      }
    }
  }

  return matrix[b.length][a.length];
};

// Fuzzy matching for search functionality
const fuzzyMatch = (needle, haystack, threshold = 0.3) => {
  const needleLower = needle.toLowerCase();
  const haystackLower = haystack.toLowerCase();

  if (haystackLower.includes(needleLower)) return true;

  const distance = levenshteinDistance(needleLower, haystackLower);
  const maxLength = Math.max(needleLower.length, haystackLower.length);
  return distance / maxLength <= threshold;
};

// Serve HTML file or 404 - updated paths to go up two directory levels
function sendHtml(res, fileName) {
  res.sendFile(path.join(__dirname, '..', '..', 'public', fileName), (err) => {
    if (err) {
      res.status(404).sendFile(path.join(__dirname, '..', '..', 'public', '404.html'));
    }
  });
}

// Delay helper function
const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// Check if all players have zero score
function allPlayersHaveZeroScore(logData) {
  if (!logData.players || !Array.isArray(logData.players) || logData.players.length === 0) {
    return false;
  }
  return logData.players.every(player => player.score === 0);
}

// Serve admin page - updated paths to go up two directory levels
function serveAdminPage(pageName, minRole) {
  return (req, res) => {
    console.log(`Accessing /${pageName}. User:`, JSON.stringify(req.user, null, 2));
    fs.readFile(path.join(__dirname, '..', '..', 'public', `${pageName}.html`), 'utf8', (err, data) => {
      if (err) {
        console.error('Error reading file:', err);
        return res.status(500).send('Error loading page');
      }
      const modifiedHtml = data.replace('</head>', `<script>window.userRole = ${req.user.role};</script></head>`);
      res.send(modifiedHtml);
    });
  };
}

module.exports = {
  adminPages,
  configFiles,
  formatTimestamp,
  getClientIp,
  levenshteinDistance,
  fuzzyMatch,
  sendHtml,
  delay,
  allPlayersHaveZeroScore,
  serveAdminPage
};