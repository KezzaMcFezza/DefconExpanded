const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool
} = require('../../constants');

router.get('/api/earliest-game-date', async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT MIN(date) as earliestDate FROM demos');

        if (rows.length > 0 && rows[0].earliestDate) {
            res.json({ earliestDate: rows[0].earliestDate });
        } else {
            res.json({ earliestDate: null });
        }
    } catch (error) {
        console.error('Error fetching earliest game date:', error);
        res.status(500).json({ error: 'Unable to fetch earliest game date' });
    }
});

module.exports = router;