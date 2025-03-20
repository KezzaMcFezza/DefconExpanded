const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');
const utils = require('../Utils/shared-utils');

// Middleware
const { authenticateToken, checkRole, upload, removeTimeout } = middleware;

// Get all mods with filtering and sorting
router.get('/', async (req, res) => {
  try {
    const { type, sort, search } = req.query;

    let query = `
      SELECT m.*, 
             m.download_count, 
             COUNT(DISTINCT ml.user_id) as likes_count, 
             COUNT(DISTINCT mf.user_id) as favorites_count
    `;
    const params = [];
    const conditions = [];

    if (type) {
      conditions.push('m.type = ?');
      params.push(type);
    }

    query += ' FROM modlist m';
    query += ' LEFT JOIN mod_likes ml ON m.id = ml.mod_id';
    query += ' LEFT JOIN mod_favorites mf ON m.id = mf.mod_id';

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    query += ' GROUP BY m.id';

    // Updated sorting logic
    switch (sort) {
      case 'most-downloaded':
        query += ' ORDER BY m.download_count DESC';
        break;
      case 'latest':
        query += ' ORDER BY m.release_date DESC';
        break;
      case 'most-liked':
        query += ' ORDER BY likes_count DESC';
        break;
      case 'most-favorited':
        query += ' ORDER BY favorites_count DESC';
        break;
      default:
        query += ' ORDER BY m.download_count DESC';  // Default to most-downloaded
    }

    const [rows] = await pool.query(query, params);

    // If a user is logged in, fetch their likes and favorites
    let userLikesAndFavorites = { likes: [], favorites: [] };
    if (req.user) {
      userLikesAndFavorites = await getUserLikesAndFavorites(req.user.id);
    }

    // Fuzzy search logic
    let results;
    if (search) {
      const searchTerms = search.toLowerCase().split(' ');
      results = rows.filter(mod =>
        searchTerms.every(term =>
          utils.fuzzyMatch(term, mod.name || '') ||
          utils.fuzzyMatch(term, mod.creator || '') ||
          utils.fuzzyMatch(term, mod.description || '')
        )
      );
      console.log('Number of fuzzy results:', results.length);
    } else {
      results = rows;
    }

    const modsWithUserInfo = results.map(mod => ({
      ...mod,
      isLiked: userLikesAndFavorites.likes.includes(mod.id),
      isFavorited: userLikesAndFavorites.favorites.includes(mod.id)
    }));

    res.json(modsWithUserInfo);
  } catch (error) {
    console.error('Error fetching mods:', error);
    res.status(500).json({ error: 'Unable to fetch mods', details: error.message });
  }
});

// Get a specific mod
router.get('/:id', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Mod not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching mod details:', error);
    res.status(500).json({ error: 'Unable to fetch mod details' });
  }
});

// Like a mod
router.post('/:id/like', authenticateToken, async (req, res) => {
  const modId = req.params.id;
  const userId = req.user.id;

  try {
    // first check if the user has already liked the mod
    const [existingLike] = await pool.query('SELECT * FROM mod_likes WHERE mod_id = ? AND user_id = ?', [modId, userId]);
    if (existingLike.length > 0) {
      return res.status(400).json({ error: 'User has already liked this mod.' });
    }
    // if not continue
    await pool.query('INSERT INTO mod_likes (mod_id, user_id) VALUES (?, ?)', [modId, userId]);

    // update the likes count in the modlist table
    await pool.query('UPDATE modlist SET likes_count = likes_count + 1 WHERE id = ?', [modId]);

    // let the server know that a user liked a mod
    console.log(`User ${req.user.username} (ID: ${userId}) liked mod ${modId}`);

    res.status(200).json({ message: 'Mod liked!' });
  } catch (error) {
    console.error('Error liking mod:', error);
    res.status(500).json({ error: 'Unable to like mod' });
  }
});

// Favorite a mod
router.post('/:id/favorite', authenticateToken, async (req, res) => {
  const modId = req.params.id;
  const userId = req.user.id;

  try {
    // first check if the user has already favorited the mod
    const [existingFavorite] = await pool.query('SELECT * FROM mod_favorites WHERE mod_id = ? AND user_id = ?', [modId, userId]);
    if (existingFavorite.length > 0) {
      return res.status(400).json({ error: 'User has already favorited this mod.' });
    }

    // if they have not, continue
    await pool.query('INSERT INTO mod_favorites (mod_id, user_id) VALUES (?, ?)', [modId, userId]);

    // update the favorites count in the modlist table
    await pool.query('UPDATE modlist SET favorites_count = favorites_count + 1 WHERE id = ?', [modId]);

    // fetches the users favourite mods from the database, then displays them on their user profile page
    const [userProfile] = await pool.query('SELECT favorites FROM user_profiles WHERE user_id = ?', [userId]);
    let currentFavorites = userProfile[0].favorites ? userProfile[0].favorites.split(',') : [];

    currentFavorites.push(modId);

    await pool.query('UPDATE user_profiles SET favorites = ? WHERE user_id = ?', [currentFavorites.join(','), userId]);
    console.log(`User ${req.user.username} (ID: ${userId}) favorited mod ${modId}`);

    res.status(200).json({ message: 'Mod favorited and user profile updated!' });
  } catch (error) {
    console.error('Error favoriting mod:', error);
    res.status(500).json({ error: 'Unable to favorite mod' });
  }
});

// Download a mod
router.get('/download/:id', removeTimeout, async (req, res) => {
  try {
    const [mod] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
    if (mod.length === 0) {
      console.log(`Mod not found: ID ${req.params.id}`);
      return res.status(404).json({ error: 'Mod not found' });
    }

    const modPath = path.join(__dirname, '..', mod[0].file_path);
    console.log(`Attempting to download mod: ${modPath}`);

    if (!fs.existsSync(modPath)) {
      console.error(`Mod file not found: ${modPath}`);
      return res.status(404).json({ error: 'Mod file not found' });
    }

    // update the download count in the modlist table
    await pool.query('UPDATE modlist SET download_count = download_count + 1 WHERE id = ?', [req.params.id]);

    // preserve the original file name on download
    const downloadFilename = path.basename(mod[0].file_path);

    res.download(modPath, downloadFilename, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Mod download aborted by client with IP: ${clientIp}`);
        } else {
          console.error(`Error downloading mod: ${err.message}`);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading mod');
          }
        }
      } else {
        console.log(`Mod downloaded successfully (${downloadFilename}) by client with IP: ${clientIp}`);
      }
    });

  } catch (error) {
    console.error('Error in download-mod route:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Create a new mod
router.post('/', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  try {
    const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
    const modFile = req.files['modFile'] ? req.files['modFile'][0] : null;
    const previewImage = req.files['previewImage'] ? req.files['previewImage'][0] : null;
    const modFilePath = modFile ? path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/') : null;
    const previewImagePath = previewImage ? path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/') : null;
    const fileSize = modFile ? modFile.size : 0;

    // now insert the mod into the database with the provided information from the admin
    const [result] = await pool.query(
      'INSERT INTO modlist (name, type, creator, release_date, description, file_path, preview_image_path, compatibility, version, size) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
      [name, type, creator, releaseDate, description, modFilePath, previewImagePath, compatibility, version, fileSize]
    );

    res.json({ id: result.insertId, message: 'Mod added successfully' });
  } catch (error) {
    console.error('Error adding mod:', error);
    res.status(500).json({ error: 'Unable to add mod', details: error.message });
  }
});

// Update a mod
router.put('/:id', upload.fields([
  { name: 'modFile', maxCount: 1 },
  { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
  const { id } = req.params;
  const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
  const clientIp = utils.getClientIp(req);

  console.log(`Admin action initiated: Mod update by ${req.user.username} from IP ${clientIp}`);

  try {
    // fetch the old data of the mod before updating
    const [oldData] = await pool.query('SELECT * FROM modlist WHERE id = ?', [id]);
    if (oldData.length === 0) {
      console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: Mod not found (ID: ${id})`);
      return res.status(404).json({ error: 'Mod not found' });
    }

    let query = 'UPDATE modlist SET name = ?, type = ?, creator = ?, release_date = ?, description = ?, compatibility = ?, version = ?';
    let params = [name, type, creator, releaseDate, description, compatibility, version];
    let newFilePath = oldData[0].file_path;
    let newFileSize = oldData[0].size;
    let newPreviewPath = oldData[0].preview_image_path;

    if (req.files['modFile']) {
      const modFile = req.files['modFile'][0];
      newFilePath = path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/');
      newFileSize = modFile.size;
      query += ', file_path = ?, size = ?';
      params.push(newFilePath, newFileSize);
    }

    if (req.files['previewImage']) {
      const previewImage = req.files['previewImage'][0];
      newPreviewPath = path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/');
      query += ', preview_image_path = ?';
      params.push(newPreviewPath);
    }

    query += ' WHERE id = ?';
    params.push(id);

    const [result] = await pool.query(query, params);

    if (result.affectedRows === 0) {
      console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: No changes made (ID: ${id})`);
      return res.status(404).json({ error: 'Mod not found or no changes made' });
    }

    // if all these checks pass proceed to inserting the new information to the database
    console.log(`Mod successfully updated by ${req.user.username} from IP ${clientIp}:`);
    console.log(`Mod ID: ${id}`);
    console.log('Old data:', JSON.stringify(oldData[0], null, 2));
    console.log('New data:', JSON.stringify({
      name, type, creator, release_date: releaseDate, description, compatibility, version,
      file_path: newFilePath,
      size: newFileSize,
      preview_image_path: newPreviewPath
    }, null, 2));

    res.json({ message: 'Mod updated successfully' });
  } catch (error) {
    console.error(`Error updating mod by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to update mod', details: error.message });
  }
});

// Delete a mod
router.delete('/:id', checkRole(1), async (req, res) => {
  const { id } = req.params;

  try {
    await pool.query('DELETE FROM modlist WHERE id = ?', [id]);
    res.json({ message: 'Mod deleted successfully' });
  } catch (error) {
    console.error('Error deleting mod:', error);
    res.status(500).json({ error: 'Unable to delete mod' });
  }
});

// Report a mod
router.post('/:id/report', authenticateToken, async (req, res) => {
  const { reportType } = req.body;
  const modId = req.params.id;
  const userId = req.user.id;

  try {
    await pool.query(
      'INSERT INTO mod_reports (mod_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
      [modId, userId, reportType]
    );
    res.json({ message: 'Mod report submitted successfully' });
  } catch (error) {
    console.error('Error submitting mod report:', error);
    res.status(500).json({ error: 'Failed to submit mod report' });
  }
});

// Search mods
router.get('/search', async (req, res) => {
  const searchTerm = req.query.term;
  try {
    const [rows] = await pool.query('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`]);
    res.json(rows);
  } catch (error) {
    console.error('Error searching mods:', error);
    res.status(500).json({ error: 'Unable to search mods' });
  }
});

// Get user likes and favorites
async function getUserLikesAndFavorites(userId) {
  try {
    const [likes] = await pool.query('SELECT mod_id FROM mod_likes WHERE user_id = ?', [userId]);
    const [favorites] = await pool.query('SELECT mod_id FROM mod_favorites WHERE user_id = ?', [userId]);

    return {
      likes: likes.map(like => like.mod_id),
      favorites: favorites.map(favorite => favorite.mod_id)
    };
  } catch (error) {
    console.error('Error fetching user likes and favorites:', error);
    return { likes: [], favorites: [] };
  }
}

module.exports = router;