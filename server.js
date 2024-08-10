const express = require('express');
const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');

const app = express();
const port = 3000;

const dbConfig = {
  host: 'localhost',
  user: 'root',
  password: '', // Make sure to set your actual MySQL password here
  database: 'defcon_demos'
};

const demoDir = path.join(__dirname, 'demos');
const uploadDir = path.join(__dirname, 'public', 'uploads');

// Ensure directories exist
[demoDir, uploadDir].forEach(dir => {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
});

// Database connection pool
const pool = mysql.createPool(dbConfig);

// Watch for new demo files
console.log(`Watching directory: ${demoDir}`);
const watcher = chokidar.watch(demoDir, {
  ignored: /(^|[\/\\])\../,
  persistent: true
});

watcher
  .on('add', async (path) => {
    console.log(`New demo detected: ${path}`);
    const fileName = path.split('\\').pop();
    const stats = fs.statSync(path);
    
    try {
      const [result] = await pool.query(
        'INSERT INTO demos (name, size, date) VALUES (?, ?, ?)',
        [fileName, stats.size, new Date()]
      );
      console.log(`Demo ${fileName} added to database`);
      
      fs.copyFile(path, `${uploadDir}/${fileName}`, err => {
        if (err) {
          console.error(`Error copying file: ${err}`);
        } else {
          console.log(`${fileName} was copied to uploads`);
        }
      });
    } catch (error) {
      console.error(`Error adding demo to database: ${error}`);
    }
  })
  .on('error', error => console.error(`Watcher error: ${error}`));

console.log('File watcher set up.');

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
app.get('/', (req, res) => {
  sendHtml(res, 'index.html');
});

// Login route
app.post('/api/login', async (req, res) => {
  const { username, password } = req.body;
  try {
    const [rows] = await pool.query('SELECT * FROM admin_users WHERE username = ?', [username]);
    if (rows.length > 0) {
      const user = rows[0];
      if (password === user.password) { // Plain text comparison
        const token = jwt.sign({ username: user.username }, JWT_SECRET, { expiresIn: '1h' });
        res.cookie('token', token, { httpOnly: true, maxAge: 3600000 }); // 1 hour
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

// List all demos with associated users
app.get('/api/demos', async (req, res) => {
  try {
    const [demos] = await pool.query(`
      SELECT d.*, GROUP_CONCAT(u.username) as usernames
      FROM demos d
      LEFT JOIN demo_users du ON d.id = du.demo_id
      LEFT JOIN users u ON du.user_id = u.id
      GROUP BY d.id
      ORDER BY d.date DESC
    `);
    
    // Parse the usernames string into an array
    demos.forEach(demo => {
      demo.users = demo.usernames ? demo.usernames.split(',') : [];
      delete demo.usernames;
    });

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

// Add a user to a demo
app.post('/api/demo/:demoId/users', authenticateToken, async (req, res) => {
  const { username, password } = req.body;
  const demoId = req.params.demoId;

  try {
    // First, insert the user
    const [userResult] = await pool.query('INSERT INTO users (username, password) VALUES (?, ?)', [username, password]);
    const userId = userResult.insertId;

    // Then, associate the user with the demo
    await pool.query('INSERT INTO demo_users (demo_id, user_id) VALUES (?, ?)', [demoId, userId]);

    res.status(201).json({ message: `User ${username} added to demo ${demoId}` });
  } catch (error) {
    console.error('Error adding user to demo:', error);
    res.status(500).json({ error: 'Unable to add user to demo' });
  }
});

// Serve additional HTML pages
app.get('/about', (req, res) => sendHtml(res, 'about.html'));
app.get('/news', (req, res) => sendHtml(res, 'news.html'));
app.get('/media', (req, res) => sendHtml(res, 'media.html'));
app.get('/resources', (req, res) => sendHtml(res, 'resources.html'));
app.get('/laikasdefcon', (req, res) => sendHtml(res, 'laikasdefcon.html'));
app.get('/homepage/matchroom', (req, res) => sendHtml(res, 'matchroom.html'));
app.get('/homepage', (req, res) => sendHtml(res, 'index.html'));
app.get('/cmd', (req, res) => sendHtml(res, 'secret.html'));

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

// 404 handler (this should be placed after all other routes)
app.use((req, res, next) => {
  res.status(404).sendFile(path.join(__dirname, 'public', '404.html'));
});

app.listen(port, () => {
  console.log(`Octocon Demo Server Listening at http://localhost:${port}`);
});