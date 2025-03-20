const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');
const utils = require('../Utils/shared-utils');
const discordFunctions = require('../Functions/discord-functions');

// Middleware
const { authenticateToken, checkRole, upload } = middleware;

// Get all demos with pagination and filtering
router.get('/', async (req, res) => {
  const {
    page = 1,
    sortBy = 'latest',
    playerName,
    serverName,
    territories,
    players,
    scoreFilter,
    gameDuration,
    scoreDifference,
    startDate,
    endDate,
    gamesPlayed
  } = req.query;

  const limit = 9;
  const offset = (page - 1) * limit;

  try {
    let query = 'SELECT * FROM demos';
    let countQuery = 'SELECT COUNT(*) as total FROM demos';
    let params = [];
    let conditions = [];

    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                       OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                       OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = Array(10).fill(`%${playerName}%`);
    }

    if (serverName) {
      conditions.push('LOWER(game_type) = LOWER(?)');
      params.push(serverName);
    }

    if (territories) {
      const territoryList = territories.split(',');
      const combineMode = req.query.combineMode === 'true';

      if (combineMode) {
        const territoryChecks = territoryList.map(territory =>
          `players LIKE ?`
        ).join(' AND ');

        conditions.push(`(
              (${territoryChecks})
              AND
              JSON_LENGTH(JSON_EXTRACT(players, '$.players')) = ?
          )`);

        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });

        params.push(territoryList.length);
      } else {
        const territoryConditions = territoryList.map(() =>
          `players LIKE ?`
        );
        conditions.push(`(${territoryConditions.join(' OR ')})`);
        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });
      }
    }

    if (players) {
      const playerList = players.split(',').filter(p => p.trim());
      if (playerList.length > 0) {
        playerList.forEach(player => {
          conditions.push(
            `(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
              OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
              OR player9_name LIKE ? OR player10_name LIKE ?)`
          );
          params.push(...Array(10).fill(`%${player}%`));
        });
      }
    }

    if (gamesPlayed) {
      const minGames = parseInt(gamesPlayed);
      const playerColumns = ['player1_name', 'player2_name', 'player3_name', 'player4_name',
        'player5_name', 'player6_name', 'player7_name', 'player8_name',
        'player9_name', 'player10_name'];

      const gameCountSubqueries = playerColumns.map(col => `
        AND (${col} IS NULL OR ${col} IN (
          SELECT player_name 
          FROM (
            SELECT COALESCE(player1_name, player2_name, player3_name, 
                          player4_name, player5_name, player6_name,
                          player7_name, player8_name, player9_name, 
                          player10_name) as player_name,
                   COUNT(*) as game_count
            FROM demos
            GROUP BY player_name
            HAVING game_count >= ?
          ) as frequent_players
        ))
      `);

      conditions.push(`1=1 ${gameCountSubqueries.join(' ')}`);
      params.push(...Array(playerColumns.length).fill(minGames));
    }

    if (startDate && endDate) {
      conditions.push('date BETWEEN ? AND ?');
      params.push(startDate, endDate);
    } else if (startDate) {
      conditions.push('date >= ?');
      params.push(startDate);
    } else if (endDate) {
      conditions.push('date <= ?');
      params.push(endDate);
    }

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
      countQuery += ' WHERE ' + conditions.join(' AND ');
    }

    let demos;
    let totalDemos;
    let totalPages;

    if (scoreDifference) {
      const [allDemosWithDiffs] = await pool.query(query + ' ORDER BY date DESC', params);
      const processedDemos = allDemosWithDiffs.map(demo => {
        let groupScores = {};

        try {
          let parsedPlayers = [];
          if (demo.players) {
            const playerData = JSON.parse(demo.players);
            parsedPlayers = playerData.players || playerData;

            const usingAlliances = parsedPlayers.some(player => player.alliance !== undefined);

            parsedPlayers.forEach(player => {
              const groupId = usingAlliances ? player.alliance : player.team;
              if (!groupScores[groupId]) {
                groupScores[groupId] = 0;
              }
              groupScores[groupId] += player.score || 0;
            });
          }

          const scores = Object.values(groupScores);
          const scoreDiff = scores.length >= 2 ?
            Math.abs(Math.max(...scores) - Math.min(...scores)) : 0;

          return {
            ...demo,
            scoreDiff
          };

        } catch (error) {
          console.error('Error calculating score difference:', error);
          return {
            ...demo,
            scoreDiff: 0
          };
        }
      });

      processedDemos.sort((a, b) => {
        return scoreDifference === 'largest' ?
          b.scoreDiff - a.scoreDiff :
          a.scoreDiff - b.scoreDiff;
      });

      const start = offset;
      const end = offset + limit;
      demos = processedDemos.slice(start, end);
      totalDemos = processedDemos.length;
      totalPages = Math.ceil(totalDemos / limit);

    } else {
      if (scoreFilter) {
        query += ` ORDER BY GREATEST(
          COALESCE(player1_score, 0), COALESCE(player2_score, 0),
          COALESCE(player3_score, 0), COALESCE(player4_score, 0),
          COALESCE(player5_score, 0), COALESCE(player6_score, 0),
          COALESCE(player7_score, 0), COALESCE(player8_score, 0),
          COALESCE(player9_score, 0), COALESCE(player10_score, 0)
        ) ${scoreFilter === 'highest' ? 'DESC' : 'ASC'}`;
      } else if (gameDuration) {
        query += ` ORDER BY TIME_TO_SEC(duration) ${gameDuration === 'longest' ? 'DESC' : 'ASC'}`;
      } else {
        query += ` ORDER BY ${sortBy === 'most-downloaded' ? 'download_count DESC' : 'date DESC'}`;
      }

      query += ' LIMIT ? OFFSET ?';
      params.push(limit, offset);

      const [fetchedDemos] = await pool.query(query, params);
      const [countResult] = await pool.query(countQuery, params.slice(0, -2));

      demos = fetchedDemos;
      totalDemos = countResult[0].total;
      totalPages = Math.ceil(totalDemos / limit);
    }

    const updatedDemos = await Promise.all(demos.map(async (demo) => {
      let gameData = { players: [], spectators: [] };
      try {
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            if (parsedData.players && Array.isArray(parsedData.players)) {
              gameData.players = parsedData.players;
              if (parsedData.spectators && Array.isArray(parsedData.spectators)) {
                gameData.spectators = parsedData.spectators;
              }
            } else if (Array.isArray(parsedData)) {
              gameData.players = parsedData;
            }
          }
        }
      } catch (error) {
        console.error('Error parsing players data:', error);
        gameData = { players: [], spectators: [] };
      }

      const players = ['player1_name', 'player2_name', 'player3_name', 'player4_name',
        'player5_name', 'player6_name', 'player7_name', 'player8_name',
        'player9_name', 'player10_name'];

      for (const playerKey of players) {
        if (demo[playerKey]) {
          const [userProfile] = await pool.query(`
            SELECT u.username 
            FROM user_profiles up
            JOIN users u ON up.user_id = u.id
            WHERE up.defcon_username = ?
          `, [demo[playerKey]]);

          if (userProfile.length > 0) {
            demo[playerKey + '_profileUrl'] = `/profile/${userProfile[0].username}`;
          }
        }
      }

      return {
        ...demo,
        players: JSON.stringify(gameData.players),
        spectators: JSON.stringify(gameData.spectators)
      };
    }));

    res.json({
      demos: updatedDemos,
      currentPage: parseInt(page),
      totalPages,
      totalDemos
    });
  } catch (error) {
    console.error('Error fetching demos:', error);
    res.status(500).json({ error: 'Unable to fetch demos' });
  }
});

// Get a single demo by ID
router.get('/:demoId', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
    if (rows.length === 0) {
      res.status(404).json({ error: 'Demo not found' });
    } else {
      res.json(rows[0]);
    }
  } catch (error) {
    console.error('Error fetching demo details:', error);
    res.status(500).json({ error: 'Unable to fetch demo details' });
  }
});

// Update a demo
router.put('/:demoId', authenticateToken, checkRole(5), async (req, res) => {
  const { demoId } = req.params;
  const { name, game_type, duration, players } = req.body;

  try {
    console.log('Starting database transaction');
    await pool.query('START TRANSACTION');

    const [oldData] = await pool.query('SELECT * FROM demos WHERE id = ?', [demoId]);
    console.log('Executing update query');
    const [updateResult] = await pool.query(
      'UPDATE demos SET name = ?, game_type = ?, duration = ?, players = ? WHERE id = ?',
      [name, game_type, duration, players, demoId]
    );

    console.log('Update result:', JSON.stringify(updateResult, null, 2));

    if (updateResult.affectedRows === 0) {
      console.log('No rows affected. Rolling back transaction.');
      await pool.query('ROLLBACK');
      return res.status(404).json({ error: 'Demo not found or no changes made' });
    }

    // sends a stringified message into the console for easier reading
    console.log('Committing transaction');
    await pool.query('COMMIT');
    console.log(`${req.user.username} updated demo:
      Demo ID: ${demoId}
      Old data: ${JSON.stringify(oldData[0], null, 2)}
      New data: ${JSON.stringify({ name, game_type, duration, players }, null, 2)}`);
    res.json({ message: 'Demo updated successfully' });
  } catch (error) {
    console.error('Error updating demo:', error.message);
    await pool.query('ROLLBACK');
    res.status(500).json({ error: 'Unable to update demo', details: error.message });
  }
});

// Delete a demo
router.delete('/:demoId', authenticateToken, checkRole(1), async (req, res) => {
  const clientIp = utils.getClientIp(req);
  console.log(`Admin action initiated: Demo deletion by ${req.user.username} from IP ${clientIp}`);

  try {
    await pool.query('START TRANSACTION');

    const [demoData] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
    if (demoData.length === 0) {
      await pool.query('ROLLBACK');
      return res.status(404).json({ error: 'Demo not found' });
    }

    if (demoData[0].game_type && demoData[0].game_type.toLowerCase().includes('1v1')) {
      try {
        const playersData = JSON.parse(demoData[0].players);
        const players = Array.isArray(playersData) ? playersData : (playersData.players || []);

        if (players.length === 2) {
          const winner = players.reduce((a, b) => (a.score > b.score ? a : b));
          const loser = players.find(p => p !== winner);

          if (winner && loser) {
            await pool.query(`
              UPDATE leaderboard 
              SET games_played = games_played - 1,
                  wins = GREATEST(0, wins - 1),
                  total_score = GREATEST(0, total_score - ?)
              WHERE player_name = ?
            `, [winner.score > 0 ? winner.score : 0, winner.name]);

            await pool.query(`
              UPDATE leaderboard 
              SET games_played = games_played - 1,
                  losses = GREATEST(0, losses - 1),
                  total_score = GREATEST(0, total_score - ?)
              WHERE player_name = ?
            `, [loser.score > 0 ? loser.score : 0, loser.name]);

            console.log(`Leaderboard entries updated for ${winner.name} and ${loser.name}`);
          }
        }
      } catch (error) {
        console.error('Error processing players data for leaderboard update:', error);
      }
    }

    await pool.query(
      'INSERT INTO deleted_demos (demo_name, deleted_by) VALUES (?, ?)',
      [demoData[0].name, req.user.id]
    );

    await pool.query('DELETE FROM demo_reports WHERE demo_id = ?', [req.params.demoId]);

    const [result] = await pool.query('DELETE FROM demos WHERE id = ?', [req.params.demoId]);

    await pool.query('COMMIT');

    console.log(`Demo successfully deleted by ${req.user.username} from IP ${clientIp}:`);
    console.log(JSON.stringify(demoData[0], null, 2));

    if (demoData[0].game_type && demoData[0].game_type.toLowerCase().includes('1v1')) {
      console.log(`Leaderboard entries have been adjusted for demo deletion (ID: ${req.params.demoId})`);
    }

    res.json({ message: 'Demo and associated data deleted successfully' });
  } catch (error) {
    await pool.query('ROLLBACK');
    console.error(`Error deleting demo by ${req.user.username} from IP ${clientIp}:`, error.message);
    res.status(500).json({ error: 'Unable to delete demo' });
  }
});

// Download a demo
router.get('/download/:demoName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    if (rows.length === 0) {
      return res.status(404).send('Demo not found');
    }

    const demoPath = path.join(__dirname, '..', 'demo_recordings', rows[0].name);
    if (!fs.existsSync(demoPath)) {
      return res.status(404).send('Demo file not found');
    }

    // Update the download count on the database for the frontend to display
    await pool.query('UPDATE demos SET download_count = download_count + 1 WHERE name = ?', [req.params.demoName]);

    // Initiate file download
    res.download(demoPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Demo download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during demo download:', err);

          // Check if the headers are already sent before trying to send a new response
          if (!res.headersSent) {
            return res.status(500).send('Error downloading demo');
          }
        }
      } else {
        console.log(`Demo downloaded successfully by client with IP: ${clientIp}`);
      }
    });

  } catch (error) {
    console.error('Error downloading demo:', error);

    // Ensure no duplicate response is sent
    if (!res.headersSent) {
      res.status(500).send('Error downloading demo');
    }
  }
});

// Upload demo
router.post('/upload', authenticateToken, upload.fields([
  { name: 'demoFile', maxCount: 1 },
  { name: 'jsonFile', maxCount: 1 }
]), checkRole(4), async (req, res) => {
  const clientIp = utils.getClientIp(req);
  console.log(`Admin action initiated: Demo upload by ${req.user.username} from IP ${clientIp}`);

  if (!req.files || !req.files.demoFile || !req.files.jsonFile) {
    console.log(`Failed demo upload attempt by ${req.user.username} from IP ${clientIp}: Missing required files`);
    return res.status(400).json({ error: 'Both demo and JSON files are required' });
  }

  const demoFile = req.files.demoFile[0];
  const jsonFile = req.files.jsonFile[0];

  console.log(`Received files for upload by ${req.user.username} from IP ${clientIp}:`,
    JSON.stringify({ demo: demoFile.originalname, json: jsonFile.originalname }, null, 2));
  // checks if the demo already exists in the database
  try {
    const [existingDemos] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFile.originalname]);
    if (existingDemos.length > 0) {
      console.log('Demo already exists:', demoFile.originalname);
      return res.status(400).json({ error: 'A demo with this name already exists' });
    }

    const jsonContent = await fs.promises.readFile(jsonFile.path, 'utf8');
    const logData = JSON.parse(jsonContent);

    await discordFunctions.processDemoFile(demoFile.originalname, demoFile.size, logData, jsonFile.originalname);

    console.log(`Demo successfully uploaded and processed by ${req.user.username} from IP ${clientIp}: ${demoFile.originalname}`);
    res.json({ message: 'Demo uploaded and processed successfully' });
  } catch (error) {
    console.error(`Error processing uploaded demo by ${req.user.username} from IP ${clientIp}:`, error);
    res.status(500).json({ error: 'Unable to process uploaded demo', details: error.message });
  }
});

// Report a demo
router.post('/report', authenticateToken, async (req, res) => {
  const { demoId, reportType } = req.body;
  const userId = req.user.id;

  try {
    await pool.query(
      'INSERT INTO demo_reports (demo_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
      [demoId, userId, reportType]
    );
    res.json({ message: 'Report submitted successfully' });
  } catch (error) {
    console.error('Error submitting report:', error);
    res.status(500).json({ error: 'Failed to submit report' });
  }
});

// Get demo for profile panel
router.get('/profile-panel', async (req, res) => {
  const { playerName } = req.query;

  try {
    let query = 'SELECT * FROM demos';
    let params = [];
    let conditions = [];

    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                       OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                       OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = Array(10).fill(`%${playerName}%`);
    }

    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    query += ' ORDER BY date DESC';

    const [demos] = await pool.query(query, params);

    const processedDemos = await Promise.all(demos.map(async (demo) => {
      let gameData = { players: [], spectators: [] };
      try {
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            if (parsedData.players && Array.isArray(parsedData.players)) {
              gameData.players = parsedData.players;
            }
            if (parsedData.spectators && Array.isArray(parsedData.spectators)) {
              gameData.spectators = parsedData.spectators;
            }
          }
        }
      } catch (error) {
        console.error('Error parsing players data:', error);
      }

      const updatedDemo = {
        ...demo,
        players: JSON.stringify(gameData.players),
        spectators: JSON.stringify(gameData.spectators)
      };

      return updatedDemo;
    }));

    res.json({ demos: processedDemos });

  } catch (error) {
    console.error('Error fetching demos for profile/admin panel:', error);
    res.status(500).json({ error: 'Unable to fetch demos for profile/admin panel' });
  }
});

// Get all demos for admin
router.get('/admin/all', authenticateToken, checkRole(5), async (req, res) => {
  try {
    const [demos] = await pool.query('SELECT * FROM demos ORDER BY date DESC');
    res.json(demos);
  } catch (error) {
    console.error('Error fetching all demos:', error);
    res.status(500).json({ error: 'Unable to fetch all demos' });
  }
});

// Search players in demos
router.get('/search/players', async (req, res) => {
  const { playerName } = req.query;

  if (!playerName) {
    return res.status(400).json({ error: 'Player name is required' });
  }

  try {
    const query = `
      SELECT * FROM demos
      WHERE player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
      OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
      OR player9_name LIKE ? OR player10_name LIKE ? ORDER BY date DESC
    `;
    const searchPattern = `%${playerName}%`;
    const [demos] = await pool.query(query, Array(10).fill(searchPattern));

    res.json(demos);
  } catch (error) {
    console.error('Error searching for players:', error);
    res.status(500).json({ error: 'Unable to search for players' });
  }
});

// Get recent game for a player
router.get('/recent/:username', async (req, res) => {
  const { username } = req.params;

  try {
    const query = `
      SELECT * FROM demos
      WHERE player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ?
            OR player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ?
            OR player9_name = ? OR player10_name = ?
      ORDER BY date DESC
      LIMIT 1
    `;
    const [recentGame] = await pool.query(query, Array(10).fill(username));

    if (recentGame.length === 0) {
      return res.status(404).json({ error: 'No recent games found' });
    }

    res.json(recentGame[0]);
  } catch (error) {
    console.error('Error fetching recent game:', error);
    res.status(500).json({ error: 'Unable to fetch recent game' });
  }
});

module.exports = router;