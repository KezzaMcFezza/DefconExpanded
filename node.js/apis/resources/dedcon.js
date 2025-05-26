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
    dedconBuildsDir
} = require('../../constants');


const debug = require('../../debug-helpers');

router.get('/api/dedcon-builds', async (req, res) => {
    const startTime = debug.enter('getDedconBuilds', [], 1);
    debug.level2('Dedcon builds list request');
    
    try {
        debug.dbQuery('SELECT * FROM dedcon_builds ORDER BY release_date DESC', [], 2);
        const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
        debug.dbResult(builds, 2);
        
        debug.level2('Returning dedcon builds:', builds.length);
        debug.exit('getDedconBuilds', startTime, { buildsCount: builds.length }, 1);
        res.json(builds);
    } catch (error) {
        debug.error('getDedconBuilds', error, 1);
        debug.exit('getDedconBuilds', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch builds' });
    }
});

router.get('/api/download-dedcon-build/:buildName', async (req, res) => {
    const startTime = debug.enter('downloadDedconBuild', [req.params.buildName], 1);
    debug.level2('Dedcon build download request:', req.params.buildName);
    
    try {
        debug.dbQuery('SELECT * FROM dedcon_builds WHERE name = ?', [req.params.buildName], 2);
        const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE name = ?', [req.params.buildName]);
        debug.dbResult(rows, 2);
        
        if (rows.length === 0) {
            debug.level1('Build not found in database:', req.params.buildName);
            debug.exit('downloadDedconBuild', startTime, 'not_found', 1);
            return res.status(404).send('Build not found');
        }

        const buildPath = path.join(dedconBuildsDir, rows[0].name);
        debug.fileOp('check', buildPath, 3);

        if (!fs.existsSync(buildPath)) {
            debug.level1('Build file not found on disk:', buildPath);
            debug.exit('downloadDedconBuild', startTime, 'file_not_found', 1);
            return res.status(404).send('Build file not found');
        }

        debug.level3('Incrementing download count for build:', rows[0].name);
        await pool.query('UPDATE dedcon_builds SET download_count = download_count + 1 WHERE name = ?', [req.params.buildName]);

        debug.level2('Starting build download:', rows[0].name);
        res.download(buildPath, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    debug.level2(`Build download aborted by client with IP: ${clientIp}`);
                } else {
                    debug.error('downloadDedconBuild', err, 1);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading build');
                    }
                }
            } else {
                debug.level2(`Build downloaded successfully by client with IP: ${clientIp}`);
                debug.exit('downloadDedconBuild', startTime, 'success', 1);
            }
        });
    } catch (error) {
        debug.error('downloadDedconBuild', error, 1);
        debug.exit('downloadDedconBuild', startTime, 'error', 1);
        res.status(500).send('Error downloading build');
    }
});

module.exports = router;