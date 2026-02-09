const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const debug = require('../../debug-helpers');
const debugUtils = require('../../debug-utils');
const { checkAuthToken } = require('../../authentication');
const { defconDir } = require('../../constants');
const { setSharedArrayBufferHeaders, findEmscriptenFiles } = require('../../shared-functions');

// static serving for the vanilla Defcon WebAssembly build
router.use('/defcon', (req, res, next) => {
    express.static(defconDir, {
        setHeaders: (res, filePath, stat) => {
            setSharedArrayBufferHeaders(res, filePath, stat);

            if (filePath.endsWith('.wasm') || filePath.endsWith('.wasm.map') || filePath.endsWith('.js') || filePath.endsWith('.data')) {
                res.set('Cache-Control', 'public, max-age=31536000, immutable'); // 1 year
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            }

            else if (filePath.endsWith('.html')) {
                res.set('Cache-Control', 'public, max-age=86400'); // 1 day
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            }

            else if (filePath.endsWith('defcon_service-worker.js')) {
                res.set('Cache-Control', 'public, max-age=3600'); // 1 hour
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            }
        }
    })(req, res, next);
});

// serve the vanilla Defcon app
router.get('/defcon', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveDefcon', [], 1);

    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Serving vanilla Defcon client');

    const defconFiles = findEmscriptenFiles(defconDir, 'defcon');
    debug.level2('Found defcon files:', defconFiles);

    if (!defconFiles.html) {
        debug.level2('No defcon HTML found');
        debug.apiResponse(404, 'Defcon client not found', 2);
        debug.exit('serveDefcon', startTime, 'html_not_found', 1);
        return res.status(404).send('Defcon client not found. Please build the WebAssembly vanilla Defcon first.');
    }

    const defconPath = path.join(defconDir, defconFiles.html);
    debug.fileOp('serve', defconPath, 2);

    try {
        await fs.promises.access(defconPath);
        debug.level2('Defcon HTML found, serving:', defconFiles.html);

        // set CORS headers for SharedArrayBuffer support
        res.set('Cross-Origin-Opener-Policy', 'same-origin');
        res.set('Cross-Origin-Embedder-Policy', 'require-corp');
        res.set('Cross-Origin-Resource-Policy', 'cross-origin');
        res.set('Content-Type', 'text/html');

        res.sendFile(defconPath);
        debug.apiResponse(200, 'Defcon client served', 2);
        debug.exit('serveDefcon', startTime, 'success', 1);

    } catch (error) {
        debug.level2('Defcon HTML not found:', defconPath);
        debug.apiResponse(404, 'Defcon client not found', 2);
        debug.exit('serveDefcon', startTime, 'html_not_found', 1);
        res.status(404).send('Defcon client not found. Please build the WebAssembly vanilla Defcon first.');
    }
});

module.exports = router;
