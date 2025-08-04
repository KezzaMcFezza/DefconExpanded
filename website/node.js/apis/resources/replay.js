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
//Last Edited 04-08-2025

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool,
    replayBuildsDir
} = require('../../constants');


const debug = require('../../debug-helpers');

router.get('/api/replay-builds', async (req, res) => {
    const startTime = debug.enter('getReplayBuilds', [], 1);
    debug.level2('Replay builds list request');
    
    try {
        debug.dbQuery('SELECT * FROM replay_builds ORDER BY release_date DESC', [], 2);
        const [builds] = await pool.query('SELECT * FROM replay_builds ORDER BY release_date DESC');
        debug.dbResult(builds, 2);
        
        debug.level2('Returning replay builds:', builds.length);
        debug.exit('getReplayBuilds', startTime, { buildsCount: builds.length }, 1);
        res.json(builds);
    } catch (error) {
        debug.error('getReplayBuilds', error, 1);
        debug.exit('getReplayBuilds', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch replay builds' });
    }
});

router.get('/api/download-replay-build/:buildName', async (req, res) => {
    const startTime = debug.enter('downloadReplayBuild', [req.params.buildName], 1);
    debug.level2('Replay build download request:', req.params.buildName);
    
    try {
        debug.dbQuery('SELECT * FROM replay_builds WHERE name = ?', [req.params.buildName], 2);
        const [rows] = await pool.query('SELECT * FROM replay_builds WHERE name = ?', [req.params.buildName]);
        debug.dbResult(rows, 2);
        
        if (rows.length === 0) {
            debug.level1('Replay build not found in database:', req.params.buildName);
            debug.exit('downloadReplayBuild', startTime, 'not_found', 1);
            return res.status(404).send('Replay build not found');
        }

        const buildPath = path.join(replayBuildsDir, rows[0].name);
        debug.fileOp('check', buildPath, 3);

        if (!fs.existsSync(buildPath)) {
            debug.level1('Replay build file not found on disk:', buildPath);
            debug.exit('downloadReplayBuild', startTime, 'file_not_found', 1);
            return res.status(404).send('Replay build file not found');
        }

        debug.level3('Incrementing download count for replay build:', rows[0].name);
        await pool.query('UPDATE replay_builds SET download_count = download_count + 1 WHERE name = ?', [req.params.buildName]);

        debug.level2('Starting replay build download:', rows[0].name);
        res.download(buildPath, (err) => {
            const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;

            if (err) {
                if (err.code === 'ECONNABORTED') {
                    debug.level2(`Replay build download aborted by client with IP: ${clientIp}`);
                } else {
                    debug.error('downloadReplayBuild', err, 1);

                    if (!res.headersSent) {
                        return res.status(500).send('Error downloading replay build');
                    }
                }
            } else {
                debug.level2(`Replay build downloaded successfully by client with IP: ${clientIp}`);
                debug.exit('downloadReplayBuild', startTime, 'success', 1);
            }
        });
    } catch (error) {
        debug.error('downloadReplayBuild', error, 1);
        debug.exit('downloadReplayBuild', startTime, 'error', 1);
        res.status(500).send('Error downloading replay build');
    }
});

module.exports = router;