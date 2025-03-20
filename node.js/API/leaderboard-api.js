const express = require('express');
const router = express.Router();
const { pool } = require('../Functions/database-backup');
const middleware = require('../Middleware/middleware');
const graphFunctions = require('../Functions/graph-functions');

// Middleware
const { authenticateToken, checkRole } = middleware;

// Get leaderboard data with filtering
router.get('/', async (req, res) => {
  try {
    // Extract query parameters
    const {
      serverName,
      playerName,
      sortBy = 'wins',
      startDate,
      endDate,
      territories,
      combineMode,
      scoreFilter,
      gameDuration,
      scoreDifference,
      gamesPlayed,
      minGames = 1,
      excludeNames = ''  // New parameter for names to exclude
    } = req.query;

    // Parse excluded names
    const namesToExclude = excludeNames ? excludeNames.split(',') : [];

    // Build the query based on filters (similar to demos endpoint)
    let query = 'SELECT * FROM demos';
    let params = [];
    let conditions = [];

    // Server filter
    if (serverName) {
      conditions.push('game_type LIKE ?');
      params.push(`%${serverName}%`);
    }

    // Player name filter
    if (playerName) {
      conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                     OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                     OR player9_name LIKE ? OR player10_name LIKE ?)`);
      params = [...params, ...Array(10).fill(`%${playerName}%`)];
    }

    // Territory filter
    if (territories) {
      const territoryList = territories.split(',');

      if (combineMode === 'true') {
        const territoryChecks = territoryList.map(() => 'players LIKE ?').join(' AND ');
        conditions.push(`(${territoryChecks}) AND JSON_LENGTH(JSON_EXTRACT(players, '$.players')) = ?`);
        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });
        params.push(territoryList.length);
      } else {
        const territoryConditions = territoryList.map(() => 'players LIKE ?');
        conditions.push(`(${territoryConditions.join(' OR ')})`);
        territoryList.forEach(territory => {
          params.push(`%"territory":"${territory}"%`);
        });
      }
    }

    // Date range filter
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

    // Combine conditions
    if (conditions.length > 0) {
      query += ' WHERE ' + conditions.join(' AND ');
    }

    // Order by date to process chronologically
    query += ' ORDER BY date ASC';

    console.log('Leaderboard query:', query);
    console.log('Parameters:', params);

    // Execute the query
    const [demos] = await pool.query(query, params);
    console.log(`Processing ${demos.length} demos for leaderboard`);

    // Get blacklisted players
    const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));

    // Player stats calculation
    const playerStats = {};
    const alliances = {};

    // Process each demo to calculate player statistics
    for (const demo of demos) {
      try {
        // Parse player data
        let playersData = [];
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            // Handle different structures
            if (Array.isArray(parsedData.players)) {
              playersData = parsedData.players;
            } else if (Array.isArray(parsedData)) {
              playersData = parsedData;
            }
          }
        }

        if (playersData.length < 2) continue; // Skip invalid data

        // Check if using alliances
        const usingAlliances = playersData.some(player => player.alliance !== undefined);

        // Calculate team/alliance scores
        const groupScores = {};
        playersData.forEach(player => {
          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) return;

          if (!groupScores[groupId]) {
            groupScores[groupId] = 0;
          }
          groupScores[groupId] += player.score || 0;
        });

        // Find the winning team/alliance
        const sortedGroups = Object.entries(groupScores)
          .sort((a, b) => b[1] - a[1]);

        const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];
        const winningGroupId = isTie ? null : Number(sortedGroups[0][0]);

        // Process players
        for (const player of playersData) {
          // Skip players without names
          if (!player.name) continue;

          // Skip blacklisted players
          if (blacklistedPlayers.has(player.name.toLowerCase())) continue;

          // Get group ID (alliance or team)
          const groupId = usingAlliances ? player.alliance : player.team;
          if (groupId === undefined) continue;

          // Determine if the player is on the winning team
          const isWinner = !isTie && groupId === winningGroupId;
          const isTieGame = isTie;

          // Create player entry if it doesn't exist
          if (!playerStats[player.name]) {
            playerStats[player.name] = {
              player_name: player.name,
              key_id: player.key_id || 'DEMO',
              games_played: 0,
              wins: 0,
              losses: 0,
              ties: 0,
              total_score: 0,
              highest_score: 0,
              avg_score: 0,
              games_by_server: {},
              territories: {},
              last_game_date: null
            };
          }

          // Update stats
          const stats = playerStats[player.name];
          stats.games_played++;

          if (isTieGame) {
            stats.ties++;
          } else if (isWinner) {
            stats.wins++;
          } else {
            stats.losses++;
          }

          stats.total_score += player.score || 0;
          stats.highest_score = Math.max(stats.highest_score, player.score || 0);
          stats.avg_score = stats.total_score / stats.games_played;

          // Track server stats
          const serverType = demo.game_type || 'Unknown';
          stats.games_by_server[serverType] = (stats.games_by_server[serverType] || 0) + 1;

          // Track territory stats
          if (player.territory) {
            if (!stats.territories[player.territory]) {
              stats.territories[player.territory] = {
                games: 0,
                wins: 0,
                losses: 0,
                ties: 0,
                score: 0
              };
            }

            const terrStats = stats.territories[player.territory];
            terrStats.games++;
            terrStats.score += player.score || 0;

            if (isTieGame) {
              terrStats.ties++;
            } else if (isWinner) {
              terrStats.wins++;
            } else {
              terrStats.losses++;
            }
          }

          // Track most recent game
          const gameDate = new Date(demo.date);
          if (!stats.last_game_date || gameDate > new Date(stats.last_game_date)) {
            stats.last_game_date = demo.date;
          }

          // Track alliance information
          if (usingAlliances) {
            if (!alliances[groupId]) {
              alliances[groupId] = {
                id: groupId,
                total_games: 0,
                total_wins: 0,
                players: new Set()
              };
            }

            alliances[groupId].players.add(player.name);
            alliances[groupId].total_games++;

            if (isWinner) {
              alliances[groupId].total_wins++;
            }
          }
        }
      } catch (error) {
        console.error('Error processing demo data for leaderboard:', error);
      }
    }

    // Calculate additional stats
    Object.values(playerStats).forEach(player => {
      // Win ratio
      player.win_ratio = (player.wins / player.games_played) * 100 || 0;

      // Tournament style weighted score (used in the tournament leaderboard)
      player.weighted_score = player.wins * (player.wins / (player.games_played || 1));

      // Calculate best and worst territory
      if (Object.keys(player.territories).length > 0) {
        let bestTerritoryRatio = -1;
        let worstTerritoryRatio = 101;
        let bestTerritory = null;
        let worstTerritory = null;

        Object.entries(player.territories).forEach(([territory, stats]) => {
          if (stats.games >= 3) { // Minimum threshold for significance
            const winRatio = (stats.wins / stats.games) * 100;

            if (winRatio > bestTerritoryRatio) {
              bestTerritoryRatio = winRatio;
              bestTerritory = territory;
            }

            if (winRatio < worstTerritoryRatio) {
              worstTerritoryRatio = winRatio;
              worstTerritory = territory;
            }
          }
        });

        player.best_territory = bestTerritory;
        player.worst_territory = worstTerritory;
      }
    });

    // Convert to array
    let leaderboardData = Object.values(playerStats);

    // Filter by minimum games played
    if (minGames > 1) {
      leaderboardData = leaderboardData.filter(player => player.games_played >= minGames);
    }

    // Apply additional filters from the query
    if (scoreFilter) {
      // Sort by highest player score in a game
      leaderboardData.sort((a, b) => scoreFilter === 'highest'
        ? b.highest_score - a.highest_score
        : a.highest_score - b.highest_score);
    } else if (gameDuration) {
      // This would need more logic to track game durations per player
      console.log('Game duration filter not implemented for leaderboard');
    } else {
      // Default sorting based on requested sort parameter
      switch (sortBy) {
        case 'wins':
          leaderboardData.sort((a, b) => b.wins - a.wins || b.total_score - a.total_score || a.player_name.localeCompare(b.player_name));
          break;
        case 'gamesPlayed':
          leaderboardData.sort((a, b) => b.games_played - a.games_played || b.wins - a.wins || a.player_name.localeCompare(b.player_name));
          break;
        case 'totalScore':
          leaderboardData.sort((a, b) => b.total_score - a.total_score || b.wins - a.wins || a.player_name.localeCompare(b.player_name));
          break;
        case 'highestScore':
          leaderboardData.sort((a, b) => b.highest_score - a.highest_score || a.player_name.localeCompare(b.player_name));
          break;
        case 'avgScore':
          leaderboardData.sort((a, b) => b.avg_score - a.avg_score || a.player_name.localeCompare(b.player_name));
          break;
        case 'winRatio':
          leaderboardData.sort((a, b) => b.win_ratio - a.win_ratio || a.player_name.localeCompare(b.player_name));
          break;
        case 'weightedScore':
          leaderboardData.sort((a, b) => b.weighted_score - a.weighted_score || a.player_name.localeCompare(b.player_name));
          break;
        case 'recent':
          leaderboardData.sort((a, b) => new Date(b.last_game_date) - new Date(a.last_game_date));
          break;
        default:
          leaderboardData.sort((a, b) => b.wins - a.wins || b.total_score - a.total_score || a.player_name.localeCompare(b.player_name));
      }
    }

    // Add rank
    leaderboardData.forEach((player, index) => {
      player.absolute_rank = index + 1;
    });

    // Add profile URLs
    const rankedLeaderboard = await Promise.all(leaderboardData.map(async (player) => {
      if (player.player_name) {
        const [userProfile] = await pool.query(`
          SELECT u.username 
          FROM user_profiles up
          JOIN users u ON up.user_id = u.id
          WHERE up.defcon_username = ?
        `, [player.player_name]);

        if (userProfile.length > 0) {
          player.profileUrl = `/profile/${userProfile[0].username}`;
        }
      }
      return player;
    }));

    // Get total player count for pagination/stats
    const totalPlayers = rankedLeaderboard.length;

    // Filter out excluded names before returning
    if (namesToExclude.length > 0) {
      leaderboardData = leaderboardData.filter(player => 
        !namesToExclude.includes(player.player_name)
      );
    }
    
    // Return the filtered data
    res.json({
      leaderboard: leaderboardData,
      totalPlayers: leaderboardData.length,
      filters: {
        serverName,
        playerName,
        territories,
        startDate,
        endDate,
        minGames
      }
    });
  } catch (error) {
    console.error('Error generating leaderboard from demos:', error);
    res.status(500).json({ error: 'Unable to generate leaderboard' });
  }
});

// Get whitelist
router.get('/whitelist', authenticateToken, async (req, res) => {
  try {
    const [rows] = await pool.query('SELECT * FROM leaderboard_whitelist');
    res.json(rows);
  } catch (error) {
    console.error('Error fetching whitelist:', error);
    res.status(500).json({ error: 'Unable to fetch whitelist' });
  }
});

// Add player to whitelist
router.post('/whitelist', authenticateToken, checkRole(5), async (req, res) => {
  const { playerName, reason } = req.body;

  try {
    const [result] = await pool.query(
      'INSERT INTO leaderboard_whitelist (player_name, reason) VALUES (?, ?)',
      [playerName, reason]
    );
    console.log(`${req.user.username} added player to whitelist: ${JSON.stringify({ playerName, reason }, null, 2)}`);
    res.json({ message: 'Player added to whitelist', id: result.insertId });
  } catch (error) {
    console.error('Error adding player to whitelist:', error.message);
    res.status(500).json({ error: 'Unable to add player to whitelist', details: error.message });
  }
});

// Remove player from whitelist
router.delete('/whitelist/:playerName', authenticateToken, checkRole(5), async (req, res) => {
  const { playerName } = req.params;
  try {
    await pool.query('DELETE FROM leaderboard_whitelist WHERE player_name = ?', [playerName]);
    console.log(`${req.user.username} removed player from whitelist: ${playerName}`);
    res.json({ message: 'Player removed from whitelist' });
  } catch (error) {
    console.error('Error removing player from whitelist:', error);
    res.status(500).json({ error: 'Unable to remove player from whitelist' });
  }
});

// Get most active players
router.get('/most-active-players', async (req, res) => {
  try {
    const { serverName, limit = 5 } = req.query;
    const activePlayers = await graphFunctions.getMostActivePlayers(serverName, limit);
    res.json(activePlayers);
  } catch (error) {
    console.error('Error calculating most active players from demos:', error);
    res.status(500).json({ error: 'Unable to calculate most active players' });
  }
});

// Request leaderboard name change
router.post('/request-name-change', authenticateToken, async (req, res) => {
  const { newName } = req.body;
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO leaderboard_name_change_requests (user_id, requested_name) VALUES (?, ?)',
      [userId, newName]);
    res.json({ message: 'Leaderboard name change request submitted successfully' });
  } catch (error) {
    console.error('Error submitting leaderboard name change request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

// Request blacklist
router.post('/request-blacklist', authenticateToken, async (req, res) => {
  const userId = req.user.id;

  try {
    await pool.query('INSERT INTO blacklist_requests (user_id) VALUES (?)', [userId]);
    res.json({ message: 'Blacklist request submitted successfully' });
  } catch (error) {
    console.error('Error submitting blacklist request:', error);
    res.status(500).json({ error: 'Server error' });
  }
});

module.exports = router;