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
//Last Edited 18-09-2025

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool,
    demoDir
} = require('../../constants');


const debug = require('../../debug-helpers');

router.get('/api/demos', async (req, res) => {
    const startTime = debug.enter('getDemos', [req.query], 1);
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
      gamesPlayed,
      includeNewPlayers = 'true'
    } = req.query;

    debug.level2('Demo query parameters:', {
      page, sortBy, playerName, serverName, territories, players,
      scoreFilter, gameDuration, scoreDifference, startDate, endDate, gamesPlayed, includeNewPlayers
    });

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

    if (includeNewPlayers === 'false') {
      conditions.push(`(
        (player1_name NOT LIKE 'NewPlayer%' OR player1_name IS NULL) AND
        (player2_name NOT LIKE 'NewPlayer%' OR player2_name IS NULL) AND
        (player3_name NOT LIKE 'NewPlayer%' OR player3_name IS NULL) AND
        (player4_name NOT LIKE 'NewPlayer%' OR player4_name IS NULL) AND
        (player5_name NOT LIKE 'NewPlayer%' OR player5_name IS NULL) AND
        (player6_name NOT LIKE 'NewPlayer%' OR player6_name IS NULL) AND
        (player7_name NOT LIKE 'NewPlayer%' OR player7_name IS NULL) AND
        (player8_name NOT LIKE 'NewPlayer%' OR player8_name IS NULL) AND
        (player9_name NOT LIKE 'NewPlayer%' OR player9_name IS NULL) AND
        (player10_name NOT LIKE 'NewPlayer%' OR player10_name IS NULL)
      )`);
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
        query += ` ORDER BY ${sortBy === 'most-downloaded' ? 'download_count DESC' : sortBy === 'most-watched' ? 'watch_count DESC' : 'date DESC'}`;
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

    debug.level2(`Returning ${updatedDemos.length} demos, page ${page}/${totalPages}`);
    debug.exit('getDemos', startTime, { demosCount: updatedDemos.length, totalPages }, 1);
    
    res.json({
      demos: updatedDemos,
      currentPage: parseInt(page),
      totalPages,
      totalDemos
    });
  } catch (error) {
    debug.error('getDemos', error, 1);
    debug.exit('getDemos', startTime, 'error', 1);
    res.status(500).json({ error: 'Unable to fetch demos' });
  }
});

router.get('/api/search-players', async (req, res) => {
  const startTime = debug.enter('searchPlayers', [req.query.playerName], 1);
  const { playerName } = req.query;

  debug.level2('Player search request:', { playerName });

  if (!playerName) {
    debug.level1('Player search missing player name');
    debug.exit('searchPlayers', startTime, 'missing_player_name', 1);
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
    debug.dbQuery(query, [searchPattern], 2);
    const [demos] = await pool.query(query, Array(10).fill(searchPattern));
    debug.dbResult(demos, 2);

    debug.level2('Player search complete:', { demosFound: demos.length });
    debug.exit('searchPlayers', startTime, { demosFound: demos.length }, 1);
    res.json(demos);
  } catch (error) {
    debug.error('searchPlayers', error, 1);
    debug.exit('searchPlayers', startTime, 'error', 1);
    res.status(500).json({ error: 'Unable to search for players' });
  }
});

router.get('/api/download/:demoName', async (req, res) => {
  const startTime = debug.enter('downloadDemo', [req.params.demoName], 1);
  debug.level2('Demo download request:', { demoName: req.params.demoName });

  try {
    debug.dbQuery('SELECT * FROM demos WHERE name = ?', [req.params.demoName], 2);
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    debug.dbResult(rows, 2);

    if (rows.length === 0) {
      debug.level1('Demo not found in database:', req.params.demoName);
      debug.exit('downloadDemo', startTime, 'not_found', 1);
      return res.status(404).send('Demo not found');
    }

    const demoPath = path.join(demoDir, rows[0].name);
    debug.fileOp('check', demoPath, 3);
    if (!fs.existsSync(demoPath)) {
      debug.level1('Demo file not found on disk:', demoPath);
      debug.exit('downloadDemo', startTime, 'file_not_found', 1);
      return res.status(404).send('Demo file not found');
    }

    debug.level3('Incrementing download count for demo:', req.params.demoName);
    await pool.query('UPDATE demos SET download_count = download_count + 1 WHERE name = ?', [req.params.demoName]);

    debug.level2('Starting demo download:', req.params.demoName);
    res.download(demoPath, (err) => {
      const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

      if (err) {
        if (err.code === 'ECONNABORTED') {
          debug.level2(`Demo download aborted by client with IP: ${clientIp}`);
        } else {
          debug.error('downloadDemo', err, 1);

          if (!res.headersSent) {
            return res.status(500).send('Error downloading demo');
          }
        }
      } else {
        debug.level2(`Demo downloaded successfully by client with IP: ${clientIp}`);
        debug.exit('downloadDemo', startTime, 'success', 1);
      }
    });

  } catch (error) {
    debug.error('downloadDemo', error, 1);
    debug.exit('downloadDemo', startTime, 'error', 1);

    if (!res.headersSent) {
      res.status(500).send('Error downloading demo');
    }
  }
});

router.post('/api/watch/:demoName', async (req, res) => {
  const startTime = debug.enter('watchDemo', [req.params.demoName], 1);
  debug.level2('Demo watch request:', { demoName: req.params.demoName });

  try {
    debug.dbQuery('SELECT * FROM demos WHERE name = ?', [req.params.demoName], 2);
    const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
    debug.dbResult(rows, 2);

    if (rows.length === 0) {
      debug.level1('Demo not found in database:', req.params.demoName);
      debug.exit('watchDemo', startTime, 'not_found', 1);
      return res.status(404).json({ error: 'Demo not found' });
    }

    debug.level3('Incrementing watch count for demo:', req.params.demoName);
    await pool.query('UPDATE demos SET watch_count = watch_count + 1 WHERE name = ?', [req.params.demoName]);

    debug.level2('Watch count incremented successfully for demo:', req.params.demoName);
    debug.exit('watchDemo', startTime, 'success', 1);
    
    res.json({ success: true, message: 'Watch count incremented' });

  } catch (error) {
    debug.error('watchDemo', error, 1);
    debug.exit('watchDemo', startTime, 'error', 1);
    res.status(500).json({ error: 'Unable to increment watch count' });
  }
});

module.exports = router;