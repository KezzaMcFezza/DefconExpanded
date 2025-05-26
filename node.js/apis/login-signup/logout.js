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


const debug = require('../../debug-helpers');

router.post('/api/logout', (req, res) => {
    const startTime = debug.enter('userLogout', [], 1);
    debug.level2('User logout request');
    
    res.clearCookie('token');
    debug.level2('Token cookie cleared');
    debug.exit('userLogout', startTime, 'success', 1);
    res.json({ message: 'Logged out successfully' });
});

module.exports = router;