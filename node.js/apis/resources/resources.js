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
    resourcesDir
} = require('../../constants');

router.get('/api/resources', async (req, res) => {
    try {
        const [resources] = await pool.query('SELECT * FROM resources ORDER BY date DESC');
        res.json(resources);
    } catch (error) {
        console.error('Error fetching resources:', error);
        res.status(500).json({ error: 'Unable to fetch resources' });
    }
});


router.get('/api/download-resource/:resourceName', async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [req.params.resourceName]);
        if (rows.length === 0) {
            return res.status(404).send('Resource not found');
        }

        const resourcePath = path.join(resourcesDir, rows[0].name);

        // make sure the file exists
        if (!fs.existsSync(resourcePath)) {
            return res.status(404).send('Resource file not found');
        }

        res.download(resourcePath, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    console.log(`Resource download aborted by client with IP: ${clientIp}`);
                } else {
                    console.error('Error during resource download:', err);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading resource');
                    }
                }
            } else {
                console.log(`Resource downloaded successfully by client with IP: ${clientIp}`);
            }
        });
    } catch (error) {
        console.error('Error downloading resource:', error);
        res.status(500).send('Error downloading resource');
    }
});

module.exports = router;
