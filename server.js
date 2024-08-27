const express = require('express');
const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const { JSDOM } = require('jsdom');
const multer = require('multer');
const nodemailer = require('nodemailer');
const axios = require('axios');

const app = express();
const port = 3000;

const dbConfig = {
  host: 'localhost',
  user: 'root',
  password: 'cca3d38e2b',
  database: 'defcon_demos'
};

const demoDir = path.join(__dirname, 'demo_recordings');
const resourcesDir = path.join(__dirname, 'public', 'Files');
const uploadDir = path.join(__dirname, 'public');
const gameLogsDir = path.join(__dirname, 'game_logs');

// Ensure directories exist
[demoDir, resourcesDir, uploadDir, gameLogsDir].forEach(dir => {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
});

// Configure multer for handling file uploads
const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    cb(null, demoDir);
  },
  filename: function (req, file, cb) {
    cb(null, file.originalname);
  }
});

const upload = multer({ storage: storage });

// Database connection pool
const pool = mysql.createPool(dbConfig);

const pendingDemoFiles = new Map();

// Watch for new demo files
console.log(`Watching demo directory: ${demoDir}`);
const demoWatcher = chokidar.watch(`${demoDir}/*.dcrec`, {
  ignored: /(^|[\/\\])\../,
  persistent: true
});

demoWatcher
  .on('add', async (demoPath) => {
    console.log(`New demo detected: ${demoPath}`);
    const demoFileName = path.basename(demoPath);
    
    // Check if the demo already exists in the database
    const exists = await demoExistsInDatabase(demoFileName);
    if (exists) {
      console.log(`Demo ${demoFileName} already exists in the database. Skipping.`);
      return;
    }

    const demoStats = await fs.promises.stat(demoPath);
    pendingDemoFiles.set(demoFileName, demoStats);
    console.log(`Demo ${demoFileName} added to pending list.`);
  })
  .on('error', error => console.error(`Demo watcher error: ${error}`));

// Watch for new log files
console.log(`Watching JSON logs directory: ${gameLogsDir}`);
const jsonWatcher = chokidar.watch(`${gameLogsDir}/*.json`, {
  ignored: /(^|[\/\\])\../,
  persistent: true
});

const TEN_MINUTES = 10 * 60 * 1000; // 10 minutes in milliseconds

jsonWatcher
  .on('add', async (jsonPath) => {
    console.log(`New JSON log file detected: ${jsonPath}`);
    
    // Wait for 10 seconds
    console.log("Waiting 10 seconds before processing the file...");
    await delay(10000);

    const logFileName = path.basename(jsonPath);
    const logTimestamp = logFileName.match(/game_(\d{4}-\d{2}-\d{2}-\d{2}-\d{2})_full\.json/);
    
    if (!logTimestamp) {
      console.log(`Unable to extract timestamp from log file: ${logFileName}`);
      return;
    }

    const logTime = new Date(logTimestamp[1].replace(/-/g, ':'));
    
    let matchingDemoFile = null;
    let smallestTimeDifference = Infinity;

    for (const [demoFileName, demoStats] of pendingDemoFiles.entries()) {
      const demoTimestamp = demoFileName.match(/game_(\d{4}-\d{2}-\d{2}-\d{2}-\d{2})\.dcrec/);
      if (demoTimestamp) {
        const demoTime = new Date(demoTimestamp[1].replace(/-/g, ':'));
        const timeDifference = Math.abs(logTime - demoTime);
        if (timeDifference <= TEN_MINUTES && timeDifference < smallestTimeDifference) {
          matchingDemoFile = demoFileName;
          smallestTimeDifference = timeDifference;
        }
      }
    }

    if (matchingDemoFile) {
      // Check if the demo already exists in the database
      const exists = await demoExistsInDatabase(matchingDemoFile);
      if (exists) {
        console.log(`Demo ${matchingDemoFile} already exists in the database. Skipping processing.`);
        pendingDemoFiles.delete(matchingDemoFile);
        return;
      }

      try {
        const logContent = await fs.promises.readFile(jsonPath, 'utf8');
        let logData;
        try {
          logData = JSON.parse(logContent);
        } catch (parseError) {
          console.error(`Error parsing JSON file ${jsonPath}:`, parseError);
          console.log('JSON content:', logContent);
          return; // Skip processing this file
        }
        const demoStats = pendingDemoFiles.get(matchingDemoFile);

        console.log(`Corresponding log file found for demo: ${matchingDemoFile}`);
        await processDemoFile(matchingDemoFile, demoStats.size, logData);
        pendingDemoFiles.delete(matchingDemoFile);
      } catch (error) {
        console.error(`Error processing log file ${jsonPath}:`, error);
      }
    } else {
      console.log(`No matching demo file found for log file: ${logFileName}`);
    }
  })
  .on('error', error => console.error(`JSON watcher error: ${error}`));

// Watch for new resource files
console.log(`Watching resources directory: ${resourcesDir}`);
const resourceWatcher = chokidar.watch(`${resourcesDir}/*.zip`, {
  ignored: /(^|[\/\\])\../,
  persistent: true
});

resourceWatcher
  .on('add', async (path) => {
    console.log(`New resource detected: ${path}`);
    const fileName = path.split('\\').pop();
    const stats = fs.statSync(path);
    
    try {
      // Check if a resource with the same name already exists
      const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [fileName]);
      if (rows.length > 0) {
        console.log(`Resource ${fileName} already exists in the database. Skipping upload.`);
        return;
      }

      // If the resource doesn't exist, insert it into the database
      const [result] = await pool.query(
        'INSERT INTO resources (name, size, date, version) VALUES (?, ?, ?, ?)',
        [fileName, stats.size, new Date(), '1.0.0'] // Default version
      );
      console.log(`Resource ${fileName} added to database`);
    } catch (error) {
      console.error(`Error adding resource to database: ${error}`);
    }
  })
  .on('error', error => console.error(`Resource watcher error: ${error}`));

console.log('File watchers set up.');

// Middleware
app.use(express.static('public'));
app.use(express.json());
app.use(cookieParser());

// Set the correct MIME type for .webmanifest files
app.use((req, res, next) => {
  if (req.url.endsWith('.webmanifest')) {
    res.setHeader('Content-Type', 'application/manifest+json');
  }
  next();
});

app.use((req, res, next) => {
  if (req.headers['x-forwarded-proto'] === 'https') {
    res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
  }
  next();
});

// Helper functions
async function demoExistsInDatabase(demoFileName) {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFileName]);
    return rows.length > 0;
  } catch (error) {
    console.error(`Error checking if demo exists in database: ${error}`);
    return false;
  }
}

async function processDemoFile(demoFileName, fileSize, logData) {
  console.log(`Processing demo file: ${demoFileName}`);

  try {
    // Check if the demo already exists in the database
    const [existingDemo] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFileName]);
    
    if (existingDemo.length > 0) {
      console.log(`Demo ${demoFileName} already exists in the database. Skipping upload.`);
      return;
    }

    const playerCount = logData.players ? logData.players.filter(p => p.score !== undefined).length : 0;
    const gameType = logData.gameType || `${playerCount} Players`;
    const duration = logData.duration || null;

    const playerColumns = ['player1', 'player2', 'player3', 'player4', 'player5', 'player6', 'player7', 'player8'];
    const playerData = {};

    // Create a map of team alliances
    const teamAlliances = {};
    let lastAlliance = null;
    if (logData.notes) {
      logData.notes.forEach(note => {
        const match = note.match(/TEAM_ALLIANCE (\d+) (\d+)/);
        if (match) {
          teamAlliances[match[1]] = match[2];
          lastAlliance = [match[1], match[2]];
        }
      });
    }

    // Filter and process players
    const participatingPlayers = logData.players.filter(p => p.score !== undefined);
    participatingPlayers.forEach((player, index) => {
      if (index < 8) {
        const prefix = playerColumns[index];
        playerData[`${prefix}_name`] = player.name || null;
        playerData[`${prefix}_team`] = player.team;
        playerData[`${prefix}_score`] = player.score || null;
        playerData[`${prefix}_territory`] = player.territory || null;
      }
    });

    const columns = [
      'name', 'size', 'date', 'game_type', 'duration', 'players', 
      ...Object.keys(playerData), 
      'alliances', 'last_alliance'
    ];
    const values = [
      demoFileName, fileSize, new Date(logData.start_time), gameType, duration, 
      JSON.stringify(participatingPlayers), 
      ...Object.values(playerData),
      JSON.stringify(teamAlliances),
      JSON.stringify(lastAlliance)
    ];

    const placeholders = columns.map(() => '?').join(', ');
    const query = `INSERT INTO demos (${columns.join(', ')}) VALUES (${placeholders})`;

    const [result] = await pool.query(query, values);

    console.log(`Demo ${demoFileName} processed and added to database with player data`);
    console.log(`Inserted row ID: ${result.insertId}`);
  } catch (error) {
    console.error(`Error processing demo ${demoFileName}:`, error);
  }
}

async function checkExistingDemos() {
  const files = await fs.promises.readdir(demoDir);
  for (const file of files) {
    if (file.endsWith('.dcrec')) {
      const exists = await demoExistsInDatabase(file);
      if (!exists) {
        const demoStats = await fs.promises.stat(path.join(demoDir, file));
        pendingDemoFiles.set(file, demoStats);
        console.log(`Existing demo ${file} added to pending list.`);
      }
    }
  }
}

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// JWT secret key (change this to a secure random string in production!)
const JWT_SECRET = 'your-secret-key';

// Middleware to verify JWT token
const authenticateToken = (req, res, next) => {
  const token = req.cookies.token;
  if (token == null) return res.sendStatus(401);

  jwt.verify(token, JWT_SECRET, (err, user) => {
    if (err) return res.sendStatus(403);
    req.user = user;
    next();
  });
};

// Middleware to check for the presence of a valid token in the cookie on each request
const checkAuthToken = (req, res, next) => {
  const token = req.cookies.token;
  if (token) {
    jwt.verify(token, JWT_SECRET, (err, user) => {
      if (err) {
        res.locals.user = null;
      } else {
        res.locals.user = user;
      }
      next();
    });
  } else {
    res.locals.user = null;
    next();
  }
};

// Helper function to send HTML file
function sendHtml(res, fileName) {
  res.sendFile(path.join(__dirname, 'public', fileName), (err) => {
    if (err) {
      res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
    }
  });
}

// Routes

// Home page
app.get('/', checkAuthToken, (req, res) => {
  sendHtml(res, 'index.html');
});

// Login route
app.post('/api/login', async (req, res) => {
  const { username, password } = req.body;
  try {
    const [rows] = await pool.query('SELECT * FROM admin_users WHERE username = ?', [username]);
    if (rows.length > 0) {
      const user = rows[0];
      if (password === user.password) {
        const token = jwt.sign({ username: user.username }, JWT_SECRET, { expiresIn: '1d' });
        res.cookie('token', token, { httpOnly: true, maxAge: 86400000 });
        res.json({ message: 'Logged in successfully' });
      } else {
        res.status(400).json({ error: 'Invalid credentials' });
      }
    } else {
      res.status(400).json({ error: 'User not found' });
    }
  } catch (error) {
    console.error('Login error:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Logout route
app.post('/api/logout', (req, res) => {
  res.clearCookie('token');
  res.json({ message: 'Logged out successfully' });
});

// Check authentication status
app.get('/api/checkAuth', (req, res) => {
  const token = req.cookies.token;
  if (token) {
    jwt.verify(token, JWT_SECRET, (err, user) => {
      if (err) {
        res.json({ isAdmin: false });
      } else {
        res.json({ isAdmin: true });
      }
    });
  } else {
    res.json({ isAdmin: false });
  }
});

app.get('/api/discord-widget', async (req, res) => {
  try {
    const discordResponse = await axios.get('https://discord.com/api/guilds/1256842522215579668/widget.json');
    res.json(discordResponse.data);
  } catch (error) {
    console.error('Error fetching Discord widget:', error);
    res.status(500).json({ error: 'Failed to fetch Discord widget data' });
  }
});

// List all demos with associated users
app.get('/api/demos', async (req, res) => {
  try {
    const [demos] = await pool.query('SELECT * FROM demos ORDER BY date DESC');
    res.json(demos);
  } catch (error) {
    console.error('Error fetching demos:', error);
    res.status(500).json({ error: 'Unable to fetch demos' });
  }
});

// Download a demo
app.get('/api/download/:demoName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    if (rows.length === 0) {
      res.status(404).send('Demo not found');
    } else {
      const demoPath = path.join(demoDir, rows[0].name);
      res.download(demoPath, (err) => {
        if (err) {
          res.status(404).send('Demo file not found');
        }
      });
    }
  } catch (error) {
    console.error('Error downloading demo:', error);
    res.status(500).send('Error downloading demo');
  }
});

// Upload a demo (protected route)
app.post('/api/upload', authenticateToken, upload.single('demoFile'), async (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: 'No file uploaded' });
  }

  try {
    const demoFile = req.file;
    const logFileName = path.parse(demoFile.originalname).name + '_full.json';
    const logFilePath = path.join(gameLogsDir, logFileName);

    // Check if log file exists
    if (await fs.promises.access(logFilePath).then(() => true).catch(() => false)) {
      const logContent = await fs.promises.readFile(logFilePath, 'utf8');
      const logData = JSON.parse(logContent);
      await processDemoFile(demoFile.originalname, demoFile.size, logData);
      res.json({ 
        message: 'Demo uploaded and processed successfully', 
        demoName: demoFile.originalname
      });
    } else {
      res.status(400).json({ error: 'Corresponding log file not found' });
      // Delete the uploaded demo file if there was an error
      await fs.promises.unlink(req.file.path).catch(console.error);
    }
  } catch (error) {
    console.error('Upload error:', error);
    res.status(500).json({ error: 'Failed to process upload' });
    // Delete the uploaded demo file if there was an error
    await fs.promises.unlink(req.file.path).catch(console.error);
  }
});

// Delete a demo (protected route)
app.delete('/api/demo/:demoId', authenticateToken, async (req, res) => {
  try {
    const [result] = await pool.query('DELETE FROM demos WHERE id = ?', [req.params.demoId]);
    if (result.affectedRows === 0) {
      res.status(404).json({ error: 'Demo not found' });
    } else {
      res.json({ message: 'Demo deleted successfully' });
    }
  } catch (error) {
    console.error('Error deleting demo:', error);
    res.status(500).json({ error: 'Unable to delete demo' });
  }
});

// Update layout route (protected)
app.post('/api/updateLayout', authenticateToken, async (req, res) => {
  if (!req.user) {
    return res.status(401).json({ error: 'Unauthorized' });
  }

  try {
    const { layout, pageName } = req.body;
    const filePath = path.join(__dirname, 'public', pageName);

    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ error: 'Page not found' });
    }

    let pageContent = await fs.promises.readFile(filePath, 'utf8');
    const dom = new JSDOM(pageContent);
    const document = dom.window.document;

    // Find or create the main content container
    let mainContent = document.querySelector('main');
    if (!mainContent) {
      mainContent = document.createElement('main');
      document.body.appendChild(mainContent);
    }

    // Clear existing content in main
    mainContent.innerHTML = '';

    // Recreate the layout
    layout.forEach(rowData => {
      const row = document.createElement('div');
      row.className = 'grid-row';
      
      rowData.columns.forEach(columnData => {
        const column = document.createElement('div');
        column.className = 'grid-column';
        
        columnData.cards.forEach(cardData => {
          const card = document.createElement('div');
          card.className = 'card';
          card.innerHTML = cardData.content;
          card.style.width = cardData.style.width;
          card.style.height = cardData.style.height;
          column.appendChild(card);
        });
        
        row.appendChild(column);
      });
      
      mainContent.appendChild(row);
    });

    // Serialize the updated DOM back to HTML
    pageContent = dom.serialize();

    // Write the updated content back to the file
    await fs.promises.writeFile(filePath, pageContent, 'utf8');
    res.json({ message: 'Layout updated successfully' });
  } catch (error) {
    console.error('Error updating layout:', error);
    res.status(500).json({ error: 'Unable to update layout' });
  }
});

// Update content (protected route)
app.post('/api/updateContent', authenticateToken, async (req, res) => {
  if (!req.user) {
    return res.status(401).json({ error: 'Unauthorized' });
  }

  try {
    const { cardIndex, content, pageName } = req.body;
    const filePath = path.join(__dirname, 'public', pageName);

    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ error: 'Page not found' });
    }

    let pageContent = await fs.promises.readFile(filePath, 'utf8');
    const dom = new JSDOM(pageContent);
    const document = dom.window.document;

    const cards = document.querySelectorAll('.card, .cardcredits, .cardproject');
    if (cardIndex < cards.length) {
      const card = cards[cardIndex];

      // Preserve media block if it exists
      const existingMediaBlock = card.querySelector('.media-block');

      // Clear existing content except media block
      Array.from(card.childNodes).forEach(child => {
        if (!child.classList || !child.classList.contains('media-block')) {
          child.remove();
        }
      });

      // Add new content
      Object.entries(content).forEach(([key, value]) => {
        if (key === 'media_block' && existingMediaBlock) {
          // Keep existing media block
          return;
        }
        const tempDiv = document.createElement('div');
        if (value.isHTML || value.isLink) {
          tempDiv.innerHTML = value.outerHTML;
        } else {
          tempDiv.textContent = value.text;
        }
        const newElement = tempDiv.firstChild;
        if (value.isLink && value.href) {
          newElement.setAttribute('href', value.href);
        }
        card.appendChild(newElement);
      });

      // Ensure media block is at the end of the card
      if (existingMediaBlock) {
        card.appendChild(existingMediaBlock);
      }

      pageContent = dom.serialize();
      await fs.promises.writeFile(filePath, pageContent, 'utf8');
      res.json({ message: 'Content updated successfully' });
    } else {
      res.status(400).json({ error: 'Card not found' });
    }
  } catch (error) {
    console.error('Error updating content:', error);
    res.status(500).json({ error: 'Unable to update content' });
  }
});

// New routes for resources
app.get('/api/resources', async (req, res) => {
  try {
    const [resources] = await pool.query('SELECT * FROM resources ORDER BY date DESC');
    res.json(resources);
  } catch (error) {
    console.error('Error fetching resources:', error);
    res.status(500).json({ error: 'Unable to fetch resources' });
  }
});

app.post('/api/upload-resource', authenticateToken, upload.single('resourceFile'), async (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: 'No file uploaded' });
  }

  try {
    const originalName = req.file.originalname;
    const filePath = path.join(resourcesDir, originalName);
    const { version, releaseDate } = req.body;

    // Move the uploaded file to the resources directory
    fs.renameSync(req.file.path, filePath);

    const stats = fs.statSync(filePath);

    const [result] = await pool.query(
      'INSERT INTO resources (name, size, date, version) VALUES (?, ?, ?, ?)',
      [originalName, stats.size, releaseDate || new Date(), version || '1.0.0']
    );

    res.json({ message: 'Resource uploaded successfully', resourceName: originalName });
  } catch (error) {
    console.error('Error uploading resource:', error);
    res.status(500).json({ error: 'Unable to upload resource' });
  }
});

app.delete('/api/resource/:resourceId', authenticateToken, async (req, res) => {
  try {
    const [result] = await pool.query('DELETE FROM resources WHERE id = ?', [req.params.resourceId]);
    if (result.affectedRows === 0) {
      res.status(404).json({ error: 'Resource not found' });
    } else {
      res.json({ message: 'Resource deleted successfully' });
    }
  } catch (error) {
    console.error('Error deleting resource:', error);
    res.status(500).json({ error: 'Unable to delete resource' });
  }
});

// Download a resource
app.get('/api/download-resource/:resourceName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [req.params.resourceName]);
    if (rows.length === 0) {
      res.status(404).send('Resource not found');
    } else {
      const resourcePath = path.join(resourcesDir, rows[0].name);
      res.download(resourcePath, (err) => {
        if (err) {
          res.status(404).send('Resource file not found');
        }
      });
    }
  } catch (error) {
    console.error('Error downloading resource:', error);
    res.status(500).send('Error downloading resource');
  }
});

// New route for handling bug reports
app.post('/api/report-bug', async (req, res) => {
  const { bugTitle, bugDescription } = req.body;

  // Create a transporter using SMTP
  let transporter = nodemailer.createTransport({
    host: "smtp.gmail.com",
    port: 587,
    secure: false, // Use TLS
    auth: {
      user: "keiron.mcphee1@gmail.com",
      pass: "ueiz tkqy uqwj lwht"
    }
  });

  // Email options
  let mailOptions = {
    from: '"DEFCON Expanded" <keiron.mcphee1@gmail.com>',
    to: "keiron.mcphee1@gmail.com", // You can change this if you want to send to a different email
    subject: `New Bug Report: ${bugTitle}`,
    text: `A new bug has been reported:

Title: ${bugTitle}

Description:
${bugDescription}`,
    html: `<h2>A new bug has been reported:</h2>
<p><strong>Title:</strong> ${bugTitle}</p>
<p><strong>Description:</strong></p>
<p>${bugDescription}</p>`
  };

  try {
    // Send the email
    let info = await transporter.sendMail(mailOptions);
    console.log("Bug report email sent: %s", info.messageId);
    res.json({ message: 'Bug report submitted successfully' });
  } catch (error) {
    console.error('Error sending bug report email:', error);
    res.status(500).json({ error: 'Failed to submit bug report' });
  }
});

// Serve additional HTML pages
app.get('/about', checkAuthToken, (req, res) => sendHtml(res, 'about.html'));
app.get('/articles-and-events', checkAuthToken, (req, res) => sendHtml(res, 'news.html'));
app.get('/media', checkAuthToken, (req, res) => sendHtml(res, 'media.html'));
app.get('/resources', checkAuthToken, (req, res) => sendHtml(res, 'resources.html'));
app.get('/laikasdefcon', checkAuthToken, (req, res) => sendHtml(res, 'laikasdefcon.html'));
app.get('/homepage/matchroom', checkAuthToken, (req, res) => sendHtml(res, 'matchroom.html'));
app.get('/homepage', checkAuthToken, (req, res) => sendHtml(res, 'index.html'));
app.get('/patchnotes', checkAuthToken, (req, res) => sendHtml(res, 'patchnotes.html'));
app.get('/bug-report', checkAuthToken, (req, res) => sendHtml(res, 'bugreport.html'));

// Serve special files
app.get('/sitemap', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'sitemap.xml'));
});

app.get('/site.webmanifest', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'site.webmanifest'));
});

app.get('/browserconfig.xml', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'browserconfig.xml'));
});

app.get('/adminlogin', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'adminlogin.html'));
});

// 404 handler (this should be placed after all other routes)
app.use((req, res, next) => {
  res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
});

// Error handling middleware
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).send('Something broke!');
});

// Start the server
app.listen(port, async () => {
  console.log(`Defcon Expanded Demo and File Server Listening at http://localhost:${port}`);
  await checkExistingDemos();
});