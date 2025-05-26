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
//Last Edited 25-05-2025

const express = require('express');
const router = express.Router();

const {
    pool,
} = require('../../constants');

const {
    authenticateToken,
    checkPermission
} = require('../../authentication')

const permissions = require('../../permission-index');

const debug = require('../../debug-helpers');

router.get('/api/whitelist', authenticateToken, checkPermission(permissions.BLACKLIST_VIEW), async (req, res) => {
    const startTime = debug.enter('getWhitelist', [req.user.username], 1);
    debug.level2('Whitelist request by:', req.user.username);
    
    try {
        debug.dbQuery('SELECT * FROM leaderboard_whitelist', [], 2);
        const [rows] = await pool.query('SELECT * FROM leaderboard_whitelist');
        debug.dbResult(rows, 2);
        
        debug.level2('Returning whitelist entries:', rows.length);
        debug.exit('getWhitelist', startTime, { entriesCount: rows.length }, 1);
        res.json(rows);
    } catch (error) {
        debug.error('getWhitelist', error, 1);
        debug.exit('getWhitelist', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch whitelist' });
    }
});

router.post('/api/whitelist', authenticateToken, checkPermission(permissions.BLACKLIST_ADD), async (req, res) => {
    const startTime = debug.enter('addToWhitelist', [req.body.playerName, req.user.username], 1);
    const { playerName, reason } = req.body;

    debug.level2('Adding player to whitelist:', { playerName, reason, by: req.user.username });

    try {
        debug.dbQuery('INSERT INTO leaderboard_whitelist', [playerName, reason], 2);
        const [result] = await pool.query(
            'INSERT INTO leaderboard_whitelist (player_name, reason) VALUES (?, ?)',
            [playerName, reason]
        );
        
        debug.level2(`${req.user.username} added player to whitelist: ${JSON.stringify({ playerName, reason }, null, 2)}`);
        debug.exit('addToWhitelist', startTime, 'success', 1);
        res.json({ message: 'Player added to whitelist', id: result.insertId });
    } catch (error) {
        debug.error('addToWhitelist', error, 1);
        debug.exit('addToWhitelist', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to add player to whitelist', details: error.message });
    }
});

router.delete('/api/whitelist/:playerName', authenticateToken, checkPermission(permissions.BLACKLIST_REMOVE), async (req, res) => {
    const startTime = debug.enter('removeFromWhitelist', [req.params.playerName, req.user.username], 1);
    const { playerName } = req.params;
    
    debug.level2('Removing player from whitelist:', { playerName, by: req.user.username });
    
    try {
        debug.dbQuery('DELETE FROM leaderboard_whitelist WHERE player_name = ?', [playerName], 2);
        await pool.query('DELETE FROM leaderboard_whitelist WHERE player_name = ?', [playerName]);
        
        debug.level2(`${req.user.username} removed player from whitelist: ${playerName}`);
        debug.exit('removeFromWhitelist', startTime, 'success', 1);
        res.json({ message: 'Player removed from whitelist' });
    } catch (error) {
        debug.error('removeFromWhitelist', error, 1);
        debug.exit('removeFromWhitelist', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to remove player from whitelist' });
    }
});

module.exports = router;