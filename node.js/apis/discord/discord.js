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
const axios = require('axios');

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