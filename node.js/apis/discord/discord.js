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
    debug.level2('Discord widget request');
    
    try {
        debug.level3('Fetching Discord widget data from API');
        const discordResponse = await axios.get('https://discord.com/api/guilds/244276153517342720/widget.json');
        debug.level3('Discord API response received:', { status: discordResponse.status });
        
        debug.level2('Returning Discord widget data');
        debug.exit('getDiscordWidget', startTime, 'success', 1);
        res.json(discordResponse.data);
    } catch (error) {
        debug.error('getDiscordWidget', error, 1);
        debug.exit('getDiscordWidget', startTime, 'error', 1);
        res.status(500).json({ error: 'Failed to fetch Discord widget data' });
    }
});

module.exports = router;