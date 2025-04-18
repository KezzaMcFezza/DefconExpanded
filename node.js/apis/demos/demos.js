//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 18-04-2025

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool,
    demoDir
} = require('../../constants');

router.get('/api/demos', async (req, res) => {
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

router.get('/api/search-players', async (req, res) => {
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

router.get('/api/download/:demoName', async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    if (rows.length === 0) {
      return res.status(404).send('Demo not found');
    }

    const demoPath = path.join(demoDir, rows[0].name);
    if (!fs.existsSync(demoPath)) {
      return res.status(404).send('Demo file not found');
    }

    await pool.query('UPDATE demos SET download_count = download_count + 1 WHERE name = ?', [req.params.demoName]);

    res.download(demoPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          console.log(`Demo download aborted by client with IP: ${clientIp}`);
        } else {
          console.error('Error during demo download:', err);

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

    if (!res.headersSent) {
      res.status(500).send('Error downloading demo');
    }
  }
});

module.exports = router;