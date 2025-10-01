const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const debug = require('../../debug-helpers');
const debugUtils = require('../../debug-utils');
const { checkAuthToken } = require('../../authentication');
const { syncPracticeDir } = require('../../constants');
const { setSharedArrayBufferHeaders, findEmscriptenFiles } = require('../../shared-functions');

// static serving for the silo practice client
router.use('/sync-practice', express.static(syncPracticeDir, {
    setHeaders: (res, path, stat) => {
        setSharedArrayBufferHeaders(res, path, stat);
        

        if (path.endsWith('.wasm') || path.endsWith('.js') || path.endsWith('.data')) {
            res.set('Cache-Control', 'public, max-age=31536000, immutable'); // 1 year
            res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
        } 

        else if (path.endsWith('.html')) {
            res.set('Cache-Control', 'public, max-age=86400'); // 1 day
            res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
        }

        else if (path.endsWith('sync-practice-service-worker.js')) {
            res.set('Cache-Control', 'public, max-age=3600'); // 1 hour
            res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
        }
    }
}));

// serve the silo practice client
router.get('/sync-practice', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveSyncPractice', [], 1);
    
    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Serving sync practice client');
    
    const syncFiles = findEmscriptenFiles(syncPracticeDir, 'sync_practice');
    debug.level2('Found sync practice files:', syncFiles);
    
    if (!syncFiles.html) {
        debug.level2('No sync practice HTML found');
        debug.apiResponse(404, 'Sync practice client not found', 2);
        debug.exit('serveSyncPractice', startTime, 'html_not_found', 1);
        return res.status(404).send('Sync practice client not found. Please build the WebAssembly sync practice client first.');
    }
    
    const syncPracticePath = path.join(syncPracticeDir, syncFiles.html);
    debug.fileOp('serve', syncPracticePath, 2);
    
    try {
        await fs.promises.access(syncPracticePath);
        debug.level2('Sync practice HTML found, serving:', syncFiles.html);
        
        // set CORS headers for SharedArrayBuffer support
        res.set('Cross-Origin-Opener-Policy', 'same-origin');
        res.set('Cross-Origin-Embedder-Policy', 'require-corp');
        res.set('Cross-Origin-Resource-Policy', 'cross-origin');
        res.set('Content-Type', 'text/html');
        
        res.sendFile(syncPracticePath);
        debug.apiResponse(200, 'Sync practice client served', 2);
        debug.exit('serveSyncPractice', startTime, 'success', 1);
        
    } catch (error) {
        debug.level2('Sync practice HTML not found:', syncPracticePath);
        debug.apiResponse(404, 'Sync practice client not found', 2);
        debug.exit('serveSyncPractice', startTime, 'html_not_found', 1);
        res.status(404).send('Sync practice client not found. Please build the WebAssembly sync practice client first.');
    }
});

module.exports = router;