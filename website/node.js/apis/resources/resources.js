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
const path = require('path');
const fs = require('fs');

const {
    pool,
    resourcesDir
} = require('../../constants');


const debug = require('../../debug-helpers');

router.get('/api/resources', async (req, res) => {
    const startTime = debug.enter('getResources', [], 1);
    debug.level2('Resources list request');
    
    try {
        debug.dbQuery('SELECT * FROM resources ORDER BY date DESC', [], 2);
        const [resources] = await pool.query('SELECT * FROM resources ORDER BY date DESC');
        debug.dbResult(resources, 2);
        
        debug.level2('Returning resources:', resources.length);
        debug.exit('getResources', startTime, { resourcesCount: resources.length }, 1);
        res.json(resources);
    } catch (error) {
        debug.error('getResources', error, 1);
        debug.exit('getResources', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch resources' });
    }
});

router.get('/api/download-resource/:resourceName', async (req, res) => {
    const startTime = debug.enter('downloadResource', [req.params.resourceName], 1);
    debug.level2('Resource download request:', req.params.resourceName);
    
    try {
        debug.dbQuery('SELECT * FROM resources WHERE name = ?', [req.params.resourceName], 2);
        const [rows] = await pool.query('SELECT * FROM resources WHERE name = ?', [req.params.resourceName]);
        debug.dbResult(rows, 2);
        
        if (rows.length === 0) {
            debug.level1('Resource not found in database:', req.params.resourceName);
            debug.exit('downloadResource', startTime, 'not_found', 1);
            return res.status(404).send('Resource not found');
        }

        const resourcePath = path.join(resourcesDir, rows[0].name);
        debug.fileOp('check', resourcePath, 3);

        if (!fs.existsSync(resourcePath)) {
            debug.level1('Resource file not found on disk:', resourcePath);
            debug.exit('downloadResource', startTime, 'file_not_found', 1);
            return res.status(404).send('Resource file not found');
        }

        debug.level2('Starting resource download:', rows[0].name);
        res.download(resourcePath, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    debug.level2(`Resource download aborted by client with IP: ${clientIp}`);
                } else {
                    debug.error('downloadResource', err, 1);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading resource');
                    }
                }
            } else {
                debug.level2(`Resource downloaded successfully by client with IP: ${clientIp}`);
                debug.exit('downloadResource', startTime, 'success', 1);
            }
        });
    } catch (error) {
        debug.error('downloadResource', error, 1);
        debug.exit('downloadResource', startTime, 'error', 1);
        res.status(500).send('Error downloading resource');
    }
});

module.exports = router;
