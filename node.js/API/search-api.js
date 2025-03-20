const express = require('express');
const router = express.Router();
const { pool } = require('../Functions/database-backup');
const utils = require('../Utils/shared-utils');

// Search for players
router.get('/players', async (req, res) => {
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

// Search for mods
router.get('/mods', async (req, res) => {
  const searchTerm = req.query.term;
  try {
    const [rows] = await pool.query('SELECT * FROM modlist WHERE name LIKE ? OR description LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`]);
    res.json(rows);
  } catch (error) {
    console.error('Error searching mods:', error);
    res.status(500).json({ error: 'Unable to search mods' });
  }
});

// Search for resources
router.get('/resources', async (req, res) => {
  const searchTerm = req.query.term;
  try {
    const [rows] = await pool.query('SELECT * FROM resources WHERE name LIKE ? OR version LIKE ? OR platform LIKE ?', [`%${searchTerm}%`, `%${searchTerm}%`, `%${searchTerm}%`]);
    res.json(rows);
  } catch (error) {
    console.error('Error searching resources:', error);
    res.status(500).json({ error: 'Unable to search resources' });
  }
});

// Search for users
router.get('/users', async (req, res) => {
  const searchTerm = req.query.term;
  try {
    const [rows] = await pool.query('SELECT id, username FROM users WHERE username LIKE ?', [`%${searchTerm}%`]);
    res.json(rows);
  } catch (error) {
    console.error('Error searching users:', error);
    res.status(500).json({ error: 'Unable to search users' });
  }
});

// Combined search
router.get('/all', async (req, res) => {
  const searchTerm = req.query.term;
  if (!searchTerm || searchTerm.trim().length < 2) {
    return res.status(400).json({ error: 'Search term must be at least 2 characters' });
  }

  try {
    // Search for players
    const playerQuery = `
      SELECT DISTINCT 
        COALESCE(player1_name, player2_name, player3_name, player4_name, player5_name, 
                player6_name, player7_name, player8_name, player9_name, player10_name) as name,
        'player' as type
      FROM demos
      WHERE player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? 
        OR player4_name LIKE ? OR player5_name LIKE ? OR player6_name LIKE ? 
        OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? 
        OR player10_name LIKE ?
      LIMIT 5
    `;
    const searchPattern = `%${searchTerm}%`;
    const [players] = await pool.query(playerQuery, Array(10).fill(searchPattern));

    // Search for mods
    const [mods] = await pool.query(`
      SELECT id, name, 'mod' as type FROM modlist 
      WHERE name LIKE ? OR description LIKE ? 
      LIMIT 5
    `, [`%${searchTerm}%`, `%${searchTerm}%`]);

    // Search for resources
    const [resources] = await pool.query(`
      SELECT id, name, 'resource' as type FROM resources 
      WHERE name LIKE ? OR version LIKE ? OR platform LIKE ? 
      LIMIT 5
    `, [`%${searchTerm}%`, `%${searchTerm}%`, `%${searchTerm}%`]);

    // Search for users
    const [users] = await pool.query(`
      SELECT id, username as name, 'user' as type FROM users 
      WHERE username LIKE ? 
      LIMIT 5
    `, [`%${searchTerm}%`]);

    // Combine and return results
    const results = [
      ...players,
      ...mods,
      ...resources,
      ...users
    ];

    res.json(results);
  } catch (error) {
    console.error('Error performing combined search:', error);
    res.status(500).json({ error: 'Error performing search' });
  }
});

// Fuzzy search
router.get('/fuzzy', async (req, res) => {
  const { term, type } = req.query;
  
  if (!term || term.trim().length < 2) {
    return res.status(400).json({ error: 'Search term must be at least 2 characters' });
  }

  try {
    let results = [];
    
    switch(type) {
      case 'mods':
        const [mods] = await pool.query('SELECT * FROM modlist');
        results = mods.filter(mod => 
          utils.fuzzyMatch(term, mod.name) || 
          utils.fuzzyMatch(term, mod.description) ||
          utils.fuzzyMatch(term, mod.creator)
        );
        break;
        
      case 'players':
        const [demos] = await pool.query('SELECT * FROM demos');
        const playerSet = new Set();
        
        demos.forEach(demo => {
          for (let i = 1; i <= 10; i++) {
            const playerName = demo[`player${i}_name`];
            if (playerName && utils.fuzzyMatch(term, playerName)) {
              playerSet.add(playerName);
            }
          }
        });
        
        results = Array.from(playerSet).map(name => ({ name, type: 'player' }));
        break;
        
      case 'resources':
        const [resources] = await pool.query('SELECT * FROM resources');
        results = resources.filter(resource => 
          utils.fuzzyMatch(term, resource.name) || 
          utils.fuzzyMatch(term, resource.version) ||
          utils.fuzzyMatch(term, resource.platform)
        );
        break;
        
      case 'users':
        const [users] = await pool.query('SELECT id, username FROM users');
        results = users.filter(user => utils.fuzzyMatch(term, user.username));
        break;
        
      default:
        // Perform fuzzy search across all types
        return res.status(400).json({ error: 'Please specify a valid search type' });
    }
    
    res.json({ results });
  } catch (error) {
    console.error('Error performing fuzzy search:', error);
    res.status(500).json({ error: 'Error performing search' });
  }
});

module.exports = router;