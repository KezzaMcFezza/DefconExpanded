const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const {
    pool
} = require('../../constants');

const {
    authenticateToken,
}   = require('../../authentication')

router.post('/api/report-mod', authenticateToken, async (req, res) => {
    const { modId, reportType } = req.body;
    const userId = req.user.id;

    try {
        await pool.query(
            'INSERT INTO mod_reports (mod_id, user_id, report_type, report_date) VALUES (?, ?, ?, NOW())',
            [modId, userId, reportType]
        );
        res.json({ message: 'Mod report submitted successfully' });
    } catch (error) {
        console.error('Error submitting mod report:', error);
        res.status(500).json({ error: 'Failed to submit mod report' });
    }
});

module.exports = router;