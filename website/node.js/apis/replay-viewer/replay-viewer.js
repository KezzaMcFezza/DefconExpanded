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
//Last Edited 25-06-2025

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool,
    demoDir
} = require('../../constants');

const {
    checkAuthToken
} = require('../../authentication');

const debug = require('../../debug-helpers');

router.get('/api/replay-viewer/launch/:demoName', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('launchReplayViewer', [req.params.demoName], 1);
    
    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Demo name parameter:', req.params.demoName);
    debug.level3('Request headers:', req.headers);
    
    if (req.params.demoName.includes('{{') || req.params.demoName.includes('}}')) {
        debug.error('launchReplayViewer', new Error(`Demo name contains template placeholders: ${req.params.demoName}`), 1);
        debug.exit('launchReplayViewer', startTime, 'template_error', 1);
        return res.status(400).json({
            error: 'Invalid demo name',
            message: 'Demo name contains template placeholders - frontend template replacement failed',
            demoName: req.params.demoName
        });
    }

    try {
        debug.dbQuery('SELECT * FROM demos WHERE name = ?', [req.params.demoName], 2);
        const [rows] = await pool.query('SELECT * FROM demos WHERE name = ?', [req.params.demoName]);
        debug.dbResult(rows, 2);

        if (rows.length === 0) {
            debug.level2('Demo not found in database:', req.params.demoName);
            
            debug.level3('Fetching sample demos for debugging...');
            const [allDemos] = await pool.query('SELECT name FROM demos LIMIT 10');
            debug.level3('Sample demos in database:', allDemos.map(d => d.name));
            
            debug.apiResponse(404, { error: 'Demo not found' }, 2);
            debug.exit('launchReplayViewer', startTime, 'not_found', 1);
            return res.status(404).json({ 
                error: 'Demo not found', 
                message: 'The requested demo could not be found in the database.',
                searchedFor: req.params.demoName,
                sampleDemos: allDemos.map(d => d.name).slice(0, 5)
            });
        }

        const demo = rows[0];
        const demoPath = path.join(demoDir, demo.name);
        
        debug.fileOp('check', demoPath, 3);
        if (!fs.existsSync(demoPath)) {
            debug.level2('Demo file not found on disk:', demoPath);
            debug.apiResponse(404, { error: 'Demo file not found' }, 2);
            debug.exit('launchReplayViewer', startTime, 'file_not_found', 1);
            return res.status(404).json({ 
                error: 'Demo file not found', 
                message: 'The demo file exists in the database but could not be found on disk.' 
            });
        }

        const replayViewerUrl = `/replay-viewer/${demo.name}`;
        
        debug.level2('Replay viewer URL constructed:', replayViewerUrl);
        debug.apiResponse(200, { success: true, replayUrl: replayViewerUrl }, 2);
        debug.exit('launchReplayViewer', startTime, 'success', 1);

        res.json({
            success: true,
            replayUrl: replayViewerUrl,
            demoName: demo.name,
            gameType: demo.game_type,
            message: 'Replay viewer URL generated successfully'
        });

    } catch (error) {
        debug.error('launchReplayViewer', error, 1);
        debug.apiResponse(500, { error: 'Internal server error' }, 1);
        debug.exit('launchReplayViewer', startTime, 'error', 1);
        res.status(500).json({ 
            error: 'Internal server error', 
            message: 'An error occurred while launching the replay viewer.' 
        });
    }
});

router.get('/api/replay-viewer/validate/:demoName', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('validateDemo', [req.params.demoName], 1);
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level3('Demo validation request for:', req.params.demoName);

    try {
        debug.dbQuery('SELECT name, game_type FROM demos WHERE name = ?', [req.params.demoName], 3);
        const [rows] = await pool.query('SELECT name, game_type FROM demos WHERE name = ?', [req.params.demoName]);
        debug.dbResult(rows, 3);
        
        if (rows.length === 0) {
            debug.level2('Demo validation failed - not found in database:', req.params.demoName);
            debug.apiResponse(200, { canView: false }, 2);
            debug.exit('validateDemo', startTime, 'not_found', 1);
            return res.json({ 
                canView: false, 
                reason: 'Demo not found in database' 
            });
        }

        const demo = rows[0];
        const demoPath = path.join(demoDir, demo.name);
        
        debug.fileOp('check', demoPath, 3);
        if (!fs.existsSync(demoPath)) {
            debug.level2('Demo validation failed - file not found on disk:', demoPath);
            debug.apiResponse(200, { canView: false }, 2);
            debug.exit('validateDemo', startTime, 'file_not_found', 1);
            return res.json({ 
                canView: false, 
                reason: 'Demo file not found on disk' 
            });
        }

        debug.level2('Demo validation successful for:', req.params.demoName);
        debug.apiResponse(200, { canView: true }, 2);
        debug.exit('validateDemo', startTime, 'success', 1);
        
        res.json({
            canView: true,
            demoName: demo.name,
            gameType: demo.game_type
        });

    } catch (error) {
        debug.error('validateDemo', error, 1);
        debug.apiResponse(500, { canView: false }, 1);
        debug.exit('validateDemo', startTime, 'error', 1);
        res.status(500).json({ 
            canView: false, 
            reason: 'Internal server error' 
        });
    }
});

module.exports = router; 