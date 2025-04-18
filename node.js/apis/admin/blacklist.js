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

const {
    pool,
} = require('../../constants');

const {
    authenticateToken,
    checkRole
}   = require('../../authentication')

router.get('/api/whitelist', authenticateToken, async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM leaderboard_whitelist');
        res.json(rows);
    } catch (error) {
        console.error('Error fetching whitelist:', error);
        res.status(500).json({ error: 'Unable to fetch whitelist' });
    }
});

router.post('/api/whitelist', authenticateToken, checkRole(5), async (req, res) => {
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


router.delete('/api/whitelist/:playerName', authenticateToken, checkRole(5), async (req, res) => {
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

module.exports = router;