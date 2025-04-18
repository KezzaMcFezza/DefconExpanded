const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const {
    pool
} = require('../../constants');

const {
    authenticateToken
}   = require('../../authentication')

router.post('/api/report-demo', authenticateToken, async (req, res) => {
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

module.exports = router;