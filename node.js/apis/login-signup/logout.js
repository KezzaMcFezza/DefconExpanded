const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

router.post('/api/logout', (req, res) => {
    res.clearCookie('token');
    res.json({ message: 'Logged out successfully' });
});

module.exports = router;