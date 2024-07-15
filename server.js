const express = require('express');
const path = require('path');
const fs = require('fs');
const chokidar = require('chokidar');

const app = express();
const port = 3000;

const demoDir = path.join(__dirname, 'demos');
const uploadDir = path.join(__dirname, 'public', 'uploads');

// Ensure upload directory exists
if (!fs.existsSync(uploadDir)) {
    fs.mkdirSync(uploadDir, { recursive: true });
}

// Ensure demo directory exists
if (!fs.existsSync(demoDir)) {
    fs.mkdirSync(demoDir);
}

// Watch for new demo files
console.log(`Watching directory: ${demoDir}`);
const watcher = chokidar.watch(demoDir, {
    ignored: /(^|[\/\\])\../,
    persistent: true
});

watcher
    .on('add', path => {
        console.log(`New demo detected: ${path}`);
        const fileName = path.split('\\').pop();
        console.log(`Attempting to copy ${fileName} to uploads`);
        fs.copyFile(path, `${uploadDir}/${fileName}`, err => {
            if (err) {
                console.error(`Error copying file: ${err}`);
            } else {
                console.log(`${fileName} was copied to uploads`);
            }
        });
    })
    .on('error', error => console.error(`Watcher error: ${error}`));

console.log('File watcher set up.');

// Serve static files from the 'public' directory
app.use(express.static('public'));

// Route for the home page
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Route to list available demos
app.get('/demos', (req, res) => {
  fs.readdir(demoDir, (err, files) => {
    if (err) {
      return res.status(500).json({ error: 'Unable to scan demos directory' });
    }
    const demos = files.map(file => ({
      name: file,
      size: fs.statSync(path.join(demoDir, file)).size,
      date: fs.statSync(path.join(demoDir, file)).mtime
    }));
    res.json(demos);
  });
});

// Route to download a specific demo
app.get('/download/:demoName', (req, res) => {
  const demoPath = path.join(demoDir, req.params.demoName);
  res.download(demoPath, (err) => {
    if (err) {
      res.status(404).send('Demo not found');
    }
  });
});

// Serve additional HTML pages
app.get('/about', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'about.html'));
});

app.get('/news', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'news.html'));
});

app.get('/media', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'media.html'));
});

app.get('/resources', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'resources.html'));
});

app.get('/10player', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', '10player.html'));
});

app.listen(port, () => {
  console.log(`Octocon Demo Server Listening at http://localhost:${port}`);
});
