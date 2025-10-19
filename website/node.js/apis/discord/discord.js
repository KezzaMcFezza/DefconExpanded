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
const axios = require('axios');

const debug = require('../../debug-helpers');

router.get('/api/discord-widget', async (req, res) => {
    const startTime = debug.enter('getDiscordWidget', [], 1);
    
    try {
        const discordResponse = await axios.get('https://discordapp.com/api/guilds/1256842522215579668/widget.json');
        
        res.json(discordResponse.data);
    } catch (error) {
        debug.error('getDiscordWidget', error, 1);
        debug.exit('getDiscordWidget', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to fetch Discord widget data' });
    }
});

module.exports = router;