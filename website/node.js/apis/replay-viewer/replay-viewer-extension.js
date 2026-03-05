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
const https = require('https');
const http = require('http');
const AdmZip = require('adm-zip');

const {
    pool,
    demoDir
} = require('../../constants');

const {
    checkAuthToken
} = require('../../authentication');

const debug = require('../../debug-helpers');

// Add Group 6 integration routes
router.get('/api/replay-viewer/group6/:gameId', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('launchGroup6Replay', [req.params.gameId], 1);
    
    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Group 6 game ID parameter:', req.params.gameId);
    
    const gameId = req.params.gameId;
    
    // Validate game ID is numeric
    if (!/^\d+$/.test(gameId)) {
        debug.error('launchGroup6Replay', new Error(`Invalid game ID: ${gameId}`), 1);
        debug.exit('launchGroup6Replay', startTime, 'invalid_id', 1);
        return res.status(400).json({
            error: 'Invalid game ID',
            message: 'Game ID must be numeric',
            gameId: gameId
        });
    }

    try {
        const group6Url = `http://group6.info/dcrec/download/${gameId}`;
        debug.level2('Fetching from Group 6:', group6Url);
        
        // Check if we already have this file cached
        const cachedFileName = `group6_${gameId}.dcrec`;
        const cachedPath = path.join(demoDir, cachedFileName);
        
        if (fs.existsSync(cachedPath)) {
            debug.level2('Using cached Group 6 replay:', cachedPath);
            const replayViewerUrl = `/replay-viewer/${cachedFileName}`;
            
            debug.apiResponse(200, { success: true, replayUrl: replayViewerUrl }, 2);
            debug.exit('launchGroup6Replay', startTime, 'cached', 1);
            
            return res.json({
                success: true,
                replayUrl: replayViewerUrl,
                demoName: cachedFileName,
                gameId: gameId,
                source: 'group6',
                cached: true,
                message: 'Using cached Group 6 replay'
            });
        }
        
        // Download from Group 6
        debug.level2('Downloading Group 6 replay file...');
        
        const downloadPromise = new Promise((resolve, reject) => {
            const request = http.get(group6Url, (response) => {
                if (response.statusCode === 404) {
                    reject(new Error('Game not found on Group 6'));
                    return;
                }
                
                if (response.statusCode !== 200) {
                    reject(new Error(`Group 6 returned status: ${response.statusCode}`));
                    return;
                }
                
                const fileStream = fs.createWriteStream(cachedPath);
                response.pipe(fileStream);
                
                fileStream.on('finish', () => {
                    fileStream.close();
                    debug.level2('Successfully downloaded Group 6 replay:', cachedPath);
                    resolve(cachedFileName);
                });
                
                fileStream.on('error', (err) => {
                    fs.unlink(cachedPath, () => {}); // Clean up on error
                    reject(err);
                });
            });
            
            request.on('error', (err) => {
                reject(err);
            });
            
            request.setTimeout(30000, () => {
                request.destroy();
                reject(new Error('Download timeout'));
            });
        });
        
        const fileName = await downloadPromise;
        
        // Add to database for tracking
        try {
            debug.dbQuery('INSERT INTO demos (name, game_type, source, group6_id) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE accessed_at = NOW()', 
                         [fileName, 'group6', 'group6', gameId], 3);
            await pool.query('INSERT INTO demos (name, game_type, source, group6_id) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE accessed_at = NOW()', 
                           [fileName, 'group6', 'group6', gameId]);
        } catch (dbError) {
            debug.level3('Database insert failed (non-critical):', dbError.message);
        }
        
        const replayViewerUrl = `/replay-viewer/${fileName}`;
        
        debug.level2('Group 6 replay viewer URL constructed:', replayViewerUrl);
        debug.apiResponse(200, { success: true, replayUrl: replayViewerUrl }, 2);
        debug.exit('launchGroup6Replay', startTime, 'success', 1);

        res.json({
            success: true,
            replayUrl: replayViewerUrl,
            demoName: fileName,
            gameId: gameId,
            source: 'group6',
            cached: false,
            message: 'Group 6 replay downloaded and ready to view'
        });

    } catch (error) {
        debug.error('launchGroup6Replay', error, 1);
        
        let errorMessage = 'An error occurred while fetching the Group 6 replay.';
        let statusCode = 500;
        
        if (error.message.includes('Game not found')) {
            errorMessage = 'Game not found on Group 6. The game ID may be invalid or the recording may not be available.';
            statusCode = 404;
        } else if (error.message.includes('timeout')) {
            errorMessage = 'Timeout while downloading from Group 6. Please try again.';
            statusCode = 504;
        }
        
        debug.apiResponse(statusCode, { error: errorMessage }, 1);
        debug.exit('launchGroup6Replay', startTime, 'error', 1);
        res.status(statusCode).json({ 
            error: 'Group 6 fetch failed',
            message: errorMessage,
            gameId: gameId
        });
    }
});

// Validate Group 6 game availability
router.get('/api/replay-viewer/group6/:gameId/validate', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('validateGroup6Game', [req.params.gameId], 1);
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    
    const gameId = req.params.gameId;
    
    if (!/^\d+$/.test(gameId)) {
        debug.exit('validateGroup6Game', startTime, 'invalid_id', 1);
        return res.json({ 
            canView: false, 
            reason: 'Invalid game ID format' 
        });
    }
    
    try {
        // Check if we have it cached first
        const cachedFileName = `group6_${gameId}.dcrec`;
        const cachedPath = path.join(demoDir, cachedFileName);
        
        if (fs.existsSync(cachedPath)) {
            debug.level2('Group 6 game validation: cached file exists');
            debug.exit('validateGroup6Game', startTime, 'cached', 1);
            return res.json({
                canView: true,
                gameId: gameId,
                source: 'group6',
                cached: true
            });
        }
        
        // Quick check if the game exists on Group 6 (HEAD request)
        const group6Url = `http://group6.info/dcrec/download/${gameId}`;
        
        const headPromise = new Promise((resolve, reject) => {
            const request = http.request(group6Url, { method: 'HEAD' }, (response) => {
                resolve(response.statusCode === 200);
            });
            
            request.on('error', () => resolve(false));
            request.setTimeout(10000, () => {
                request.destroy();
                resolve(false);
            });
            
            request.end();
        });
        
        const exists = await headPromise;
        
        debug.level2('Group 6 game validation result:', exists);
        debug.exit('validateGroup6Game', startTime, exists ? 'success' : 'not_found', 1);
        
        res.json({
            canView: exists,
            gameId: gameId,
            source: 'group6',
            cached: false,
            reason: exists ? null : 'Game not found on Group 6'
        });
        
    } catch (error) {
        debug.error('validateGroup6Game', error, 1);
        debug.exit('validateGroup6Game', startTime, 'error', 1);
        res.json({ 
            canView: false, 
            reason: 'Error checking Group 6 availability' 
        });
    }
});

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

        const replayViewerUrl = `/replay-viewer/${encodeURIComponent(demo.name)}`;
        
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

// Group 6 replay viewer route - downloads file if needed and serves replay viewer
router.get('/replay-viewer/group6/:gameId', checkAuthToken, async (req, res) => {
    const gameId = req.params.gameId;
    
    console.log(`Group 6 replay viewer request for game ${gameId}`);
    
    // Validate game ID is numeric
    if (!/^\d+$/.test(gameId)) {
        return res.status(400).send('Invalid game ID');
    }

    try {
        // Create a simple mapping file to track downloaded Group 6 games
        const mappingFile = path.join(demoDir, 'group6_cache.json');
        let gameMapping = {};
        
        // Load existing mapping
        if (fs.existsSync(mappingFile)) {
            try {
                gameMapping = JSON.parse(fs.readFileSync(mappingFile, 'utf8'));
            } catch (e) {
                gameMapping = {};
            }
        }
        
        let actualFileName;
        
        // Check if we have this game cached
        if (gameMapping[gameId] && fs.existsSync(path.join(demoDir, gameMapping[gameId]))) {
            // Use existing cached file
            actualFileName = gameMapping[gameId];
            console.log(`Using cached Group 6 game ${gameId}: ${actualFileName}`);
        } else {
            // Download from Group 6 and get the real filename
            console.log(`Downloading Group 6 game ${gameId}...`);
            
            const group6Url = `http://group6.info/dcrec/download/${gameId}`;
            
            // Download the file and extract real filename
            const downloadResult = await new Promise((resolve, reject) => {
                const request = http.get(group6Url, (response) => {
                    if (response.statusCode === 404) {
                        reject(new Error('Game not found on Group 6'));
                        return;
                    }
                    
                    if (response.statusCode !== 200) {
                        reject(new Error(`Group 6 returned status: ${response.statusCode}`));
                        return;
                    }
                    
                    // Extract filename from Content-Disposition header or URL
                    let originalFileName = null;
                    
                    const contentDisposition = response.headers['content-disposition'];
                    if (contentDisposition) {
                        const filenameMatch = contentDisposition.match(/filename[^;=\n]*=((['"]).*?\2|[^;\n]*)/);
                        if (filenameMatch && filenameMatch[1]) {
                            originalFileName = filenameMatch[1].replace(/['"]/g, '');
                        }
                    }
                    
                    // If no filename in headers, use a default based on game ID
                    if (!originalFileName || !originalFileName.endsWith('.dcrec')) {
                        originalFileName = `game_${gameId}.dcrec`;
                    }
                    
                    // Use original filename without any modifications
                    const cachedPath = path.join(demoDir, originalFileName);
                    
                    const fileStream = fs.createWriteStream(cachedPath);
                    response.pipe(fileStream);
                    
                    fileStream.on('finish', () => {
                        fileStream.close();
                        console.log(`Successfully downloaded Group 6 game ${gameId} as: ${originalFileName}`);
                        
                        // Update cache mapping
                        gameMapping[gameId] = originalFileName;
                        fs.writeFileSync(mappingFile, JSON.stringify(gameMapping, null, 2));
                        
                        resolve(originalFileName);
                    });
                    
                    fileStream.on('error', (err) => {
                        fs.unlink(cachedPath, () => {}); // Clean up on error
                        reject(err);
                    });
                });
                
                request.on('error', (err) => {
                    reject(err);
                });
                
                request.setTimeout(30000, () => {
                    request.destroy();
                    reject(new Error('Download timeout'));
                });
            });
            
            actualFileName = downloadResult;
        }
        
        // Now redirect to the replay viewer with the actual filename
        res.redirect(`/replay-viewer/${actualFileName}`);
        
    } catch (error) {
        console.error(`Failed to load Group 6 game ${gameId}:`, error);
        
        if (error.message.includes('Game not found')) {
            res.status(404).send('Game not found on Group 6');
        } else if (error.message.includes('timeout')) {
            res.status(504).send('Timeout downloading from Group 6');
        } else {
            res.status(500).send('Failed to load replay');
        }
    }
});

// SFCON replay viewer route - downloads ZIP file, extracts .dcrec, and serves replay viewer
router.get(/^\/replay-viewer\/sfcon\/(.+)$/, checkAuthToken, async (req, res) => {
    const gameUrl = req.params.gameUrl;
    
    console.log(`SFCON replay viewer request for URL: ${gameUrl}`);
    
    // Validate URL format (should be like "2025-07/SFCON-1v1_2025-07-09_03.03.zip")
    if (!gameUrl || !gameUrl.endsWith('.zip')) {
        return res.status(400).send('Invalid SFCON game URL');
    }

    try {
        // Create a simple mapping file to track downloaded SFCON games
        const mappingFile = path.join(demoDir, 'sfcon_cache.json');
        let gameMapping = {};
        
        // Load existing mapping
        if (fs.existsSync(mappingFile)) {
            try {
                gameMapping = JSON.parse(fs.readFileSync(mappingFile, 'utf8'));
            } catch (e) {
                gameMapping = {};
            }
        }
        
        let actualFileName;
        
        // Check if we have this game cached
        if (gameMapping[gameUrl] && fs.existsSync(path.join(demoDir, gameMapping[gameUrl]))) {
            // Use existing cached file
            actualFileName = gameMapping[gameUrl];
            console.log(`Using cached SFCON game ${gameUrl}: ${actualFileName}`);
        } else {
            // Download from SFCON and extract the .dcrec file
            console.log(`Downloading SFCON game ${gameUrl}...`);
            
            const sfconUrl = `http://sfcon.demoszenen.de/dcrec/${gameUrl}`;
            
            // Download and extract the ZIP file
            const extractResult = await new Promise((resolve, reject) => {
                const request = http.get(sfconUrl, (response) => {
                    if (response.statusCode === 404) {
                        reject(new Error('Game not found on SFCON'));
                        return;
                    }
                    
                    if (response.statusCode !== 200) {
                        reject(new Error(`SFCON returned status: ${response.statusCode}`));
                        return;
                    }
                    
                    const chunks = [];
                    response.on('data', (chunk) => {
                        chunks.push(chunk);
                    });
                    
                    response.on('end', async () => {
                        try {
                            const zipBuffer = Buffer.concat(chunks);
                            const zip = new AdmZip(zipBuffer);
                            const zipEntries = zip.getEntries();
                            
                            // Find the .dcrec file in the ZIP
                            const dcrecEntry = zipEntries.find(entry => entry.entryName.endsWith('.dcrec'));
                            
                            if (!dcrecEntry) {
                                reject(new Error('No .dcrec file found in SFCON ZIP'));
                                return;
                            }
                            
                            // Extract original filename from the ZIP entry
                            const originalFileName = path.basename(dcrecEntry.entryName);
                            const extractedPath = path.join(demoDir, originalFileName);
                            
                            // Extract the .dcrec file to the demo directory
                            const dcrecData = dcrecEntry.getData();
                            fs.writeFileSync(extractedPath, dcrecData);
                            
                            console.log(`Successfully extracted SFCON game ${gameUrl} as: ${originalFileName}`);
                            
                            // Update cache mapping
                            gameMapping[gameUrl] = originalFileName;
                            fs.writeFileSync(mappingFile, JSON.stringify(gameMapping, null, 2));
                            
                            resolve(originalFileName);
                            
                        } catch (extractError) {
                            reject(new Error(`Failed to extract ZIP: ${extractError.message}`));
                        }
                    });
                });
                
                request.on('error', (err) => {
                    reject(err);
                });
                
                request.setTimeout(30000, () => {
                    request.destroy();
                    reject(new Error('Download timeout'));
                });
            });
            
            actualFileName = extractResult;
        }
        
        // Now redirect to the replay viewer with the actual filename
        res.redirect(`/replay-viewer/${actualFileName}`);
        
    } catch (error) {
        console.error(`Failed to load SFCON game ${gameUrl}:`, error);
        
        if (error.message.includes('Game not found')) {
            res.status(404).send('Game not found on SFCON');
        } else if (error.message.includes('timeout')) {
            res.status(504).send('Timeout downloading from SFCON');
        } else if (error.message.includes('No .dcrec file found')) {
            res.status(422).send('Invalid SFCON archive - no .dcrec file found');
        } else {
            res.status(500).send('Failed to load replay');
        }
    }
});

module.exports = router; 