const express = require('express');
const router = express.Router();
const path = require('path');
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');

// Middleware
const { authenticateToken, upload } = middleware;

// Get user profile
router.get('/:username', async (req, res) => {
  const username = req.params.username;
  const mode = req.query.mode || 'vanilla';

  try {
    const query = `
          SELECT users.username, user_profiles.*
          FROM users
          JOIN user_profiles ON users.id = user_profiles.user_id
          WHERE users.username = ?
      `;

    const [rows] = await pool.query(query, [username]);

    if (rows.length === 0) {
      return res.status(404).json({ error: 'Profile not found' });
    }

    const userProfile = rows[0];
    const territories = {
      vanilla: ['North America', 'South America', 'Europe', 'Africa', 'Asia', 'Russia'],
      '8player': ['North America', 'South America', 'Europe', 'Africa', 'East Asia', 'West Asia', 'Russia', 'Australasia'],
      '10player': ['North America', 'South America', 'Europe', 'North Africa', 'South Africa', 'Russia', 'East Asia', 'South Asia', 'Australasia', 'Antartica']
    };

    const validTerritories = territories[mode] || territories.vanilla;

    // Initialize response data
    const responseData = {
      ...userProfile,
      winLossRatio: 'Not enough data',
      totalGames: 0,
      wins: 0,
      losses: 0,
      favoriteMods: [],
      main_contributions: userProfile.main_contributions ? userProfile.main_contributions.split(',') : [],
      guides_and_mods: userProfile.guides_and_mods ? userProfile.guides_and_mods.split(',') : [],
      record_score: userProfile.record_score || 0,
      territoryStats: {
        bestTerritory: 'N/A',
        worstTerritory: 'N/A'
      }
    };

    if (userProfile.defcon_username) {
      const gamesQuery = `SELECT players FROM demos WHERE players LIKE ?`;
      const [games] = await pool.query(gamesQuery, [`%${userProfile.defcon_username}%`]);

      let territoryStats = {};
      let highestScore = userProfile.record_score || 0;

      games.forEach(game => {
        try {
          // Parse and normalize the player data
          const parsedData = JSON.parse(game.players);
          const playersArray = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);

          const userPlayer = playersArray.find(p => p.name === userProfile.defcon_username);

          if (userPlayer && validTerritories.includes(userPlayer.territory)) {
            const usingAlliances = userPlayer.alliance !== undefined;
            const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;

            // Calculate scores
            const scores = {};
            playersArray.forEach(player => {
              const groupId = usingAlliances ? player.alliance : player.team;
              scores[groupId] = (scores[groupId] || 0) + player.score;
            });

            const userGroupScore = scores[userGroupId] || 0;
            const userGroupIsWinner = Object.entries(scores)
              .every(([groupId, score]) => groupId === userGroupId.toString() || score <= userGroupScore);

            const territory = userPlayer.territory;
            territoryStats[territory] = territoryStats[territory] || { wins: 0, losses: 0 };

            if (userGroupIsWinner) {
              responseData.wins++;
              territoryStats[territory].wins++;
            } else {
              responseData.losses++;
              territoryStats[territory].losses++;
            }

            responseData.totalGames++;
            highestScore = Math.max(highestScore, userPlayer.score);
          }
        } catch (err) {
          console.error('Error processing game data:', err);
        }
      });

      // Calculate win/loss ratio
      if (responseData.totalGames > 0) {
        responseData.winLossRatio = (responseData.wins / responseData.totalGames).toFixed(2);
      }

      // Find best and worst territories
      let bestRatio = -1;
      let worstRatio = 2;

      Object.entries(territoryStats).forEach(([territory, stats]) => {
        const total = stats.wins + stats.losses;
        if (total >= 3) { // Minimum threshold of 3 games for territory statistics
          const ratio = stats.wins / total;

          if (ratio > bestRatio) {
            bestRatio = ratio;
            responseData.territoryStats.bestTerritory = territory;
          }

          if (ratio < worstRatio) {
            worstRatio = ratio;
            responseData.territoryStats.worstTerritory = territory;
          }
        }
      });

      // Update record score if necessary
      if (highestScore > responseData.record_score) {
        await pool.query(
          'UPDATE user_profiles SET record_score = ? WHERE user_id = ?',
          [highestScore, userProfile.user_id]
        );
        responseData.record_score = highestScore;
      }
    }

    // Fetch favorite mods
    if (userProfile.favorites) {
      const favoriteModIds = userProfile.favorites.split(',').filter(id => id);
      if (favoriteModIds.length > 0) {
        const [mods] = await pool.query(
          'SELECT * FROM modlist WHERE id IN (?)',
          [favoriteModIds]
        );
        responseData.favoriteMods = mods;
      }
    }

    // Send the response
    res.json(responseData);

  } catch (error) {
    console.error('Error fetching profile:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Update user profile
router.post('/update', authenticateToken, async (req, res) => {
  const userId = req.user.id;
  const {
    discord_username,
    steam_id,
    bio,
    defcon_username,
    years_played,
    main_contributions,
    guides_and_mods
  } = req.body;

  try {
    await pool.query(`
          UPDATE user_profiles 
          SET discord_username = ?, 
              steam_id = ?,
              bio = ?,
              defcon_username = ?, 
              years_played = ?,
              main_contributions = ?,
              guides_and_mods = ?
          WHERE user_id = ?
      `, [
      discord_username,
      steam_id,
      bio,
      defcon_username,
      years_played,
      main_contributions.join(','),
      guides_and_mods.join(','),
      userId
    ]);

    res.json({ success: true, message: 'Profile updated successfully' });
  } catch (error) {
    console.error('Error updating profile:', error);
    res.status(500).json({ success: false, message: 'Failed to update profile' });
  }
});

// Upload profile image
router.post('/upload-image', upload.single('image'), async (req, res) => {
  if (!req.file) {
    console.error('No file uploaded');
    return res.status(400).json({ success: false, error: 'No file uploaded' });
  }

  const userId = req.user.id;
  const imageType = req.body.type;

  try {
    const fileExtension = path.extname(req.file.originalname);
    const newFileName = `${userId}_${imageType}${fileExtension}`;
    const newFilePath = path.join('public', 'uploads', newFileName);

    fs.renameSync(req.file.path, newFilePath);

    const imageUrl = `/uploads/${newFileName}`;

    const updateField = imageType === 'profile' ? 'profile_picture' : 'banner_image';
    await pool.query(`UPDATE user_profiles SET ${updateField} = ? WHERE user_id = ?`, [imageUrl, userId]);

    console.log(`Image uploaded and database updated for user ${userId}, type: ${imageType}`);
    res.json({ success: true, imageUrl: imageUrl });
  } catch (error) {
    console.error('Error updating profile image:', error);
    res.status(500).json({ success: false, error: 'Failed to update profile image' });
  }
});

module.exports = router;