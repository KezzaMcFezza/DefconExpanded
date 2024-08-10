const express = require('express');
const mysql = require('mysql2/promise');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const chokidar = require('chokidar');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');
const { JSDOM } = require('jsdom');

const app = express();
const port = 3000;

const dbConfig = {
  host: 'localhost',
  user: 'root',
  password: 'cca3d38e2b', // Make sure to set your actual MySQL password here
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
      // Check if a demo with the same name already exists
      const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [fileName]);
      if (rows.length > 0) {
        console.log(`Demo ${fileName} already exists in the database. Skipping upload.`);
        return;
      }

      // If the demo doesn't exist, insert it into the database
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
      if (password === user.password) { // Plain text comparison
        const token = jwt.sign({ username: user.username }, JWT_SECRET, { expiresIn: '1d' }); // Set expiration to 1 day
        res.cookie('token', token, { httpOnly: true, maxAge: 86400000 }); // Set cookie expiration to 1 day (86400000 ms)
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

// Update about page content (protected route)
app.put('/api/about', authenticateToken, async (req, res) => {
  try {
    const { heading, content } = req.body;

    // Read the existing about.html file
    const aboutFilePath = path.join(__dirname, 'public', 'about.html');
    let aboutContent = await fsPromises.readFile(aboutFilePath, 'utf8');

    // Update the heading
    aboutContent = aboutContent.replace(/<h2 id="welcome-to-defcon-expanded"[^>]*>.*?<\/h2>/s, `<h2 id="welcome-to-defcon-expanded">${heading}</h2>`);

    // Update the content
    aboutContent = aboutContent.replace(/<p>.*?<\/p>/s, `<p>${content}</p>`);

    // Write the updated content back to the file
    await fsPromises.writeFile(aboutFilePath, aboutContent, 'utf8');

    res.json({ message: 'About page updated successfully' });
  } catch (error) {
    console.error('Error updating about page:', error);
    res.status(500).json({ error: 'Unable to update about page' });
  }
});

// New route to update content (protected route)
app.post('/api/updateContent', authenticateToken, async (req, res) => {
  if (!req.user) {
    return res.status(401).json({ error: 'Unauthorized' });
  }

  try {
    const { cardIndex, content } = req.body;
    const aboutFilePath = path.join(__dirname, 'public', 'about.html');
    let aboutContent = await fsPromises.readFile(aboutFilePath, 'utf8');

    // Parse the HTML content
    const dom = new JSDOM(aboutContent);
    const document = dom.window.document;

    // Find the card and update its content
    const cards = document.querySelectorAll('.card');
    if (cardIndex < cards.length) {
      const card = cards[cardIndex];
      
      // Clear existing content
      card.innerHTML = '';

      // Add new content
      Object.entries(content).forEach(([key, value]) => {
        const tempDiv = document.createElement('div');
        tempDiv.innerHTML = value.outerHTML;
        const newElement = tempDiv.firstChild;
        newElement.textContent = value.text;
        if (newElement.tagName.toLowerCase() === 'a' && value.href) {
          newElement.setAttribute('href', value.href);
        }
        card.appendChild(newElement);
      });

      // Save the updated content
      aboutContent = dom.serialize();
      await fsPromises.writeFile(aboutFilePath, aboutContent, 'utf8');
      res.json({ message: 'Content updated successfully' });
    } else {
      res.status(400).json({ error: 'Card not found' });
    }
  } catch (error) {
    console.error('Error updating content:', error);
    res.status(500).json({ error: 'Unable to update content' });
  }
});

// Serve additional HTML pages
app.get('/about', checkAuthToken, (req, res) => sendHtml(res, 'about.html'));
app.get('/news', checkAuthToken, (req, res) => sendHtml(res, 'news.html'));
app.get('/media', checkAuthToken, (req, res) => sendHtml(res, 'media.html'));
app.get('/resources', checkAuthToken, (req, res) => sendHtml(res, 'resources.html'));
app.get('/laikasdefcon', checkAuthToken, (req, res) => sendHtml(res, 'laikasdefcon.html'));
app.get('/homepage/matchroom', checkAuthToken, (req, res) => sendHtml(res, 'matchroom.html'));
app.get('/homepage', checkAuthToken, (req, res) => sendHtml(res, 'index.html'));
app.get('/cmd', checkAuthToken, (req, res) => sendHtml(res, 'secret.html'));

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