const express = require('express');
const router = express.Router();
const axios = require('axios');
const path = require('path');
const fs = require('fs');

router.get('/api/discord-widget', async (req, res) => {
    try {
        const discordResponse = await axios.get('https://discord.com/api/guilds/244276153517342720/widget.json');
        res.json(discordResponse.data);
    } catch (error) {
        console.error('Error fetching Discord widget:', error);
        res.status(500).json({ error: 'Failed to fetch Discord widget data' });
    }
});

module.exports = router;