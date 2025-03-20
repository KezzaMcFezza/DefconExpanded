const express = require('express');
const router = express.Router();
const axios = require('axios');
const { pool } = require('../Functions/database-backup');
const graphFunctions = require('../Functions/graph-functions');
const middleware = require('../Middleware/middleware');

// Get Discord widget data
router.get('/discord-widget', async (req, res) => {
  try {
    const discordResponse = await axios.get('https://discord.com/api/guilds/244276153517342720/widget.json');
    res.json(discordResponse.data);
  } catch (error) {
    console.error('Error fetching Discord widget:', error);
    res.status(500).json({ error: 'Failed to fetch Discord widget data' });
  }
});

// Get earliest game date
router.get('/earliest-game-date', async (req, res) => {
  try {
    const earliestDate = await graphFunctions.getEarliestGameDate();
    res.json({ earliestDate });
  } catch (error) {
    console.error('Error fetching earliest game date:', error);
    res.status(500).json({ error: 'Unable to fetch earliest game date' });
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

// Get games timeline data
router.get('/games-timeline', async (req, res) => {
  try {
    const { graphType = 'individualServers', playerName } = req.query;
    const chartData = await graphFunctions.getGamesTimelineData(graphType, playerName);
    res.json(chartData);
  } catch (error) {
    console.error('Error fetching games timeline data:', error);
    res.status(500).json({ error: 'Unable to fetch games timeline data' });
  }
});

// Get pending reports count
router.get('/pending-reports-count', middleware.authenticateToken, async (req, res) => {
  try {
    const [result] = await pool.query('SELECT COUNT(*) as count FROM demo_reports WHERE status = "pending"');
    console.log('Pending reports count from database:', result[0].count);
    res.json({ count: result[0].count });
  } catch (error) {
    console.error('Error fetching pending reports count:', error);
    res.status(500).json({ error: 'Failed to fetch pending reports count' });
  }
});

// Get server status
router.get('/server-status', async (req, res) => {
  try {
    // Define your server status endpoints
    const serverEndpoints = {
      vanilla: 'https://defconexpanded.com:32271/status',
      '1v1': 'https://defconexpanded.com:32272/status',
      '2v2': 'https://defconexpanded.com:32273/status',
      '8players': 'https://defconexpanded.com:32274/status'
      // Add more server endpoints as needed
    };

    const serverStatus = {};

    // Query each server endpoint
    for (const [serverType, endpoint] of Object.entries(serverEndpoints)) {
      try {
        const response = await axios.get(endpoint, { timeout: 5000 });
        serverStatus[serverType] = {
          online: true,
          playerCount: response.data.playerCount || 0,
          serverName: response.data.serverName || serverType,
          gameMode: response.data.gameMode || 'Unknown',
          maxPlayers: response.data.maxPlayers || 'Unknown'
        };
      } catch (error) {
        serverStatus[serverType] = {
          online: false,
          error: error.code || 'Connection error'
        };
      }
    }

    res.json({ servers: serverStatus });
  } catch (error) {
    console.error('Error fetching server status:', error);
    res.status(500).json({ error: 'Failed to fetch server status' });
  }
});

// Get overall site statistics
router.get('/site-stats', async (req, res) => {
  try {
    // Get total demos
    const [totalDemosResult] = await pool.query('SELECT COUNT(*) as count FROM demos');
    const totalDemos = totalDemosResult[0].count;

    // Get total users
    const [totalUsersResult] = await pool.query('SELECT COUNT(*) as count FROM users');
    const totalUsers = totalUsersResult[0].count;

    // Get total mods
    const [totalModsResult] = await pool.query('SELECT COUNT(*) as count FROM modlist');
    const totalMods = totalModsResult[0].count;

    // Get total resources
    const [totalResourcesResult] = await pool.query('SELECT COUNT(*) as count FROM resources');
    const totalResources = totalResourcesResult[0].count;

    // Get total matches played (sum of all player games)
    const [gamesPlayedResult] = await pool.query('SELECT SUM(games_played) as total FROM leaderboard');
    const totalGamesPlayed = gamesPlayedResult[0].total || 0;

    // Get site start date (earliest demo date)
    const [startDateResult] = await pool.query('SELECT MIN(date) as date FROM demos');
    const startDate = startDateResult[0].date;

    // Get most recent activity (latest demo date)
    const [lastActivityResult] = await pool.query('SELECT MAX(date) as date FROM demos');
    const lastActivity = lastActivityResult[0].date;

    res.json({
      totalDemos,
      totalUsers,
      totalMods,
      totalResources,
      totalGamesPlayed,
      startDate,
      lastActivity,
      daysActive: Math.floor((new Date() - new Date(startDate)) / (1000 * 60 * 60 * 24)),
      avgGamesPerDay: totalDemos / Math.max(1, Math.floor((new Date() - new Date(startDate)) / (1000 * 60 * 60 * 24)))
    });
  } catch (error) {
    console.error('Error fetching site statistics:', error);
    res.status(500).json({ error: 'Failed to fetch site statistics' });
  }
});

module.exports = router;