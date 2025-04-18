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
const path = require('path');
const fs = require('fs');

const {
    pool,
    dedconBuildsDir
} = require('../../constants');

router.get('/api/dedcon-builds', async (req, res) => {
    try {
        const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
        res.json(builds);
    } catch (error) {
        console.error('Error fetching builds:', error);
        res.status(500).json({ error: 'Unable to fetch builds' });
    }
});

router.get('/api/download-dedcon-build/:buildName', async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE name = ?', [req.params.buildName]);
        if (rows.length === 0) {
            return res.status(404).send('Build not found');
        }

        const buildPath = path.join(dedconBuildsDir, rows[0].name);

        if (!fs.existsSync(buildPath)) {
            return res.status(404).send('Build file not found');
        }

        await pool.query('UPDATE dedcon_builds SET download_count = download_count + 1 WHERE name = ?', [req.params.buildName]);

        res.download(buildPath, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    console.log(`Build download aborted by client with IP: ${clientIp}`);
                } else {
                    console.error('Error during build download:', err);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading build');
                    }
                }
            } else {
                console.log(`Build downloaded successfully by client with IP: ${clientIp}`);
            }
        });
    } catch (error) {
        console.error('Error downloading build:', error);
        res.status(500).send('Error downloading build');
    }
});

module.exports = router;