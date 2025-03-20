const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');
const utils = require('../Utils/shared-utils');

// Middleware
const { authenticateToken, checkRole, upload } = middleware;

// Get all builds
router.get('/', async (req, res) => {
  try {
    const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
    res.json(builds);
  } catch (error) {
    console.error('Error fetching builds:', error);
    res.status(500).json({ error: 'Unable to fetch builds' });
  }
});

// Get all builds for admin panel
router.get('/admin', authenticateToken, checkRole(2), async (req, res) => {
  try {
    const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
    res.json(builds);
  } catch (error) {
    console.error('Error fetching builds for admin:', error);
    res.status(500).json({ error: 'Unable to fetch builds' });
  }
});

// Upload new build
router.post('/upload', authenticateToken, upload.single('dedconBuildsFile'), checkRole(2), async (req, res) => {
  const clientIp = utils.getClientIp(req);
  console.log(`Admin action initiated: Build upload by ${req.user.username} from IP ${clientIp}`);

  if (!req.file) {
    console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
    return res.status(400).json({ error: 'No file uploaded' });
  }

  try {
    const { version, releaseDate, platform, playerCount } = req.body;
    const originalName = req.file.originalname;
    // Use the dedconBuildsDir instead of resourcesDir
    const filePath = path.join(__dirname, '..', 'public', 'Files', originalName);

    if (!version || !platform || !playerCount) {
      console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
      return res.status(400).json({ error: 'Version, platform, and player count are required' });
    }

    console.log(`Processing build upload by ${req.user.username} from IP ${clientIp}:`,
      JSON.stringify({ name: originalName, version, releaseDate, platform, playerCount }, null, 2));

    // Move the uploaded file to the correct directory
    fs.renameSync(req.file.path, filePath);
    const stats = fs.statSync(filePath);
    const uploadDate = releaseDate ? new Date(releaseDate) : new Date();

    const [result] = await pool.query(
      'INSERT INTO dedcon_builds (name, size, release_date, version, platform, player_count, file_path) VALUES (?, ?, ?, ?, ?, ?, ?)',
      [originalName, stats.size, uploadDate, version, platform, playerCount, filePath]
    );

    console.log(`Build successfully uploaded by ${req.user.username} from IP ${clientIp}`);
    res.json({
      message: 'Build uploaded successfully',
      buildName: originalName,
      version: version,
      platform: platform,
      playerCount: playerCount
    });
  } catch (error) {
    console.error(`Error uploading build by ${req.user.username} from IP ${clientIp}:`, error.message);

    if (req.file && req.file.path) {
      try {
        fs.unlinkSync(req.file.path);
      } catch (unlinkError) {
        console.error('Error deleting uploaded file:', unlinkError.message);
      }
    }

    res.status(500).json({ error: 'Unable to upload build', details: error.message });
  }
});

// Download build route
router.get('/download/:buildName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE name = ?', [req.params.buildName]);
    if (rows.length === 0) {
      return res.status(404).send('Build not found');
    }

    const buildPath = rows[0].file_path;

    if (!fs.existsSync(buildPath)) {
      return res.status(404).send('Build file not found');
    }

    await pool.query('UPDATE dedcon_builds SET download_count = download_count + 1 WHERE name = ?', [req.params.buildName]);

    res.download(buildPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Build download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during build download:', err);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading build');
          }
        }
      } else {
        console.log(`Build downloaded successfully by client with IP: ${clientIp}`);
      }
    });
  } catch (error) {
    console.error('Error downloading build:', error);
    res.status(500).send('Error downloading build');
  }
});

// Delete build route
router.delete('/:buildId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = utils.getClientIp(req);
  console.log(`Admin action initiated: Build deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    const [buildData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
    const [result] = await pool.query('DELETE FROM dedcon_builds WHERE id = ?', [req.params.buildId]);

    if (result.affectedRows === 0) {
      console.log(`Failed build deletion attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${req.params.buildId})`);
      res.status(404).json({ error: 'Build not found' });
    } else {
      console.log(`Build successfully deleted by ${req.user.username} from IP ${clientIp}:`, JSON.stringify(buildData[0], null, 2));
      res.json({ message: 'Build deleted successfully' });
    }
  } catch (error) {
    console.error(`Error deleting build by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to delete build' });
  }
});

// Get single build details
router.get('/:buildId', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Build not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching build details:', error);
    res.status(500).json({ error: 'Unable to fetch build details' });
  }
});

// Update build details
router.put('/:buildId', authenticateToken, checkRole(2), async (req, res) => {
  const clientIp = utils.getClientIp(req);
  const { buildId } = req.params;
  const { name, version, release_date, platform, player_count } = req.body;

  console.log(`Admin action initiated: Build edit by ${req.user.username} from IP ${clientIp}`);
  console.log(`Editing build ID ${buildId}:`, JSON.stringify({ name, version, release_date, platform, player_count }, null, 2));

  try {
    const [oldData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [buildId]);
    const [result] = await pool.query(
      'UPDATE dedcon_builds SET name = ?, version = ?, release_date = ?, platform = ?, player_count = ? WHERE id = ?',
      [name, version, new Date(release_date), platform, player_count, buildId]
    );

    if (result.affectedRows === 0) {
      console.log(`Failed build edit attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${buildId})`);
      res.status(404).json({ error: 'Build not found' });
    } else {
      console.log(`Build successfully edited by ${req.user.username} from IP ${clientIp}:`);
      console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
      console.log(`New data:`, JSON.stringify({ name, version, release_date, platform, player_count }, null, 2));
      res.json({ message: 'Build updated successfully' });
    }
  } catch (error) {
    console.error(`Error updating build by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to update build' });
  }
});

module.exports = router;