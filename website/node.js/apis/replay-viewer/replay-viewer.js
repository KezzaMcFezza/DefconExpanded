const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const debug = require('../../debug-helpers');
const debugUtils = require('../../debug-utils');
const { checkAuthToken } = require('../../authentication');
const { demoDir, replayViewerDir, publicDir } = require('../../constants');
const { setSharedArrayBufferHeaders, findEmscriptenFiles } = require('../../shared-functions');

// static serving for the replay viewer
router.use('/replay-viewer', (req, res, next) => {
    if (req.path.endsWith('.dcrec')) {
        return next();
    }
    
    express.static(replayViewerDir, {
        setHeaders: (res, path, stat) => {
            setSharedArrayBufferHeaders(res, path, stat);
            
            // 1 year cache duration, in the event that no new updates are made
            if (path.endsWith('.wasm') || path.endsWith('.wasm.map') || path.endsWith('.js') || path.endsWith('.data')) {
                res.set('Cache-Control', 'public, max-age=31536000, immutable'); // 1 year
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            } 
            // cache html files for 1 day
            else if (path.endsWith('.html')) {
                res.set('Cache-Control', 'public, max-age=86400'); // 1 day
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            }
            // cache service worker for short time to allow updates
            else if (path.endsWith('replay-viewer-service-worker.js')) {
                res.set('Cache-Control', 'public, max-age=3600'); // 1 hour
                res.set('ETag', `"${stat.mtime.getTime()}-${stat.size}"`);
            }
        }
    })(req, res, next);
});

// serve the .dcrec files directly for the replay viewer
router.get(/^\/replay-viewer\/files\/.+\.dcrec$/, checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveDcrecFile', [req.path], 2);
    const filename = path.basename(req.path);
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving .dcrec file:', filename);
    
    const dcrecPath = path.join(demoDir, filename);
    debug.fileOp('serve', dcrecPath, 3);
    
    try {
        await fs.promises.access(dcrecPath);
        debug.level2('File found, serving .dcrec file:', filename);
        res.setHeader('Content-Type', 'application/octet-stream');
        res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
        res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
        res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
        res.sendFile(dcrecPath);
        debug.exit('serveDcrecFile', startTime, 'success', 2);
    } catch (error) {
        debug.level2('Demo file not found on disk:', dcrecPath);
        debug.apiResponse(404, 'Demo file not found', 2);
        debug.exit('serveDcrecFile', startTime, 'not_found', 2);
        res.status(404).send('Demo file not found.');
    }
});

// serve the replay viewer html file
router.get('/replay-viewer/:filename', checkAuthToken, async (req, res) => {
    const startTime = debug.enter('serveReplayViewer', [req.params.filename], 1);
    const filename = req.params.filename;
    
    debug.apiRequest('GET', req.originalUrl, req.user, 1);
    debug.level2('Requested replay viewer for:', filename);
    
    const replayFiles = findEmscriptenFiles(replayViewerDir, 'replay_viewer');
    debug.level2('Found replay viewer files:', replayFiles);
    
    let replayViewerPath;
    let jsFileName = 'defcon.js'; // mostly for development, we would never use this on the live server
    
    if (replayFiles.html) {
        replayViewerPath = path.join(replayViewerDir, replayFiles.html);
        jsFileName = replayFiles.js || 'defcon.js';
    } else {
        debug.level2('No replay viewer HTML found');
        debug.apiResponse(404, 'Replay viewer not found', 2);
        debug.exit('serveReplayViewer', startTime, 'html_not_found', 1);
        return res.status(404).send('Replay viewer not found. Please build the WebAssembly replay viewer first.');
    }
    
    debug.fileOp('check', replayViewerPath, 3);
    
    try {
        await fs.promises.access(replayViewerPath);
        const dcrecPath = path.join(demoDir, filename);
        debug.fileOp('check', dcrecPath, 2);
        
        try {
            await fs.promises.access(dcrecPath);
            debug.level3('Demo file exists: true');
        } catch (error) {
            debug.level2('Demo file not found:', dcrecPath);
            debug.apiResponse(404, 'Demo file not found', 2);
            debug.exit('serveReplayViewer', startTime, 'demo_not_found', 1);
            return res.status(404).send('Demo file not found.');
        }
      
        // read the html file and inject the command line arguments + file loading logic
        debug.fileOp('read', replayViewerPath, 2);
        
        try {
            const data = await fs.promises.readFile(replayViewerPath, 'utf8');
            
            debug.level2('HTML file read successfully, length:', data.length);
            debug.level2('Injecting file loading logic for:', filename);
            debug.level2('Using JS file:', jsFileName);
          
            // inject file loading logic and command line arguments into the html file
            const scriptPattern = new RegExp(`<script[^>]*src="${jsFileName.replace('.', '\\.')}"[^>]*><\\/script>`);
            const beforeMatch = data.match(scriptPattern);
            debug.level3('Script injection pattern:', scriptPattern.toString());
            debug.level3('Found script tag:', beforeMatch ? beforeMatch[0] : 'NOT FOUND');
            
            // this is mostly for development, we would never use this on the live server
            const fallbackPattern = /<script[^>]*src="defcon\.js"[^>]*><\/script>/;
            const actualPattern = beforeMatch ? scriptPattern : fallbackPattern;
          
            const modifiedHtml = data.replace(
                actualPattern,
                `<script>
                    window.Module = window.Module || {};
                    window.Module.arguments = ['-l', '${filename}'];
                    window.Module.print = function(text) {
                    };
                    window.Module.printErr = function(text) {
                        console.error('[WASM Error]:', text);
                    };
                    
                    window.Module.preRun = window.Module.preRun || [];
                    window.Module.preRun.push(async function() {
                        
                        try {
                            const response = await fetch('/replay-viewer/files/${filename}');
                            
                            if (!response.ok) {
                                throw new Error(\`HTTP error! status: \${response.status}\`);
                            }

                            const arrayBuffer = await response.arrayBuffer();
                            const uint8Array = new Uint8Array(arrayBuffer);

                            FS.writeFile('${filename}', uint8Array);
                            
                            if (FS.analyzePath('${filename}').exists) {
                                const stats = FS.stat('${filename}');
                            } else {
                            }
                        } catch (error) {
                        }
                    });
                  
                </script>
                <script async type="text/javascript" src="${jsFileName}"></script>`
            );
          
            // now time to see if the replacement worked
            const afterMatch = modifiedHtml.match(actualPattern);
            debug.level3('Script tag after replacement:', afterMatch ? 'STILL FOUND (ERROR)' : 'REPLACED SUCCESSFULLY');
            debug.level3('Module.preRun injected:', modifiedHtml.includes('Module.preRun'));
            debug.level2('File loading logic injected, sending HTML response');
            debug.apiResponse(200, 'HTML with injected WebAssembly logic', 2);
            debug.exit('serveReplayViewer', startTime, 'success', 1);
            
            // set CORS headers for SharedArrayBuffer support (required for web assembly threading)
            res.set('Cross-Origin-Opener-Policy', 'same-origin');
            res.set('Cross-Origin-Embedder-Policy', 'require-corp');
            res.set('Cross-Origin-Resource-Policy', 'cross-origin');
            res.set('Content-Type', 'text/html');
            
            res.send(modifiedHtml);
            
        } catch (error) {
            debug.error('serveReplayViewer', error, 1);
            debug.apiResponse(500, 'Error reading replay viewer file', 1);
            debug.exit('serveReplayViewer', startTime, 'file_read_error', 1);
            return res.status(500).send('Error reading replay viewer file.');
        }
        
    } catch (error) {
        // darth vader noises
        debug.level2('Replay viewer HTML not found:', replayViewerPath);
        debug.apiResponse(404, 'Replay viewer not found', 2);
        debug.exit('serveReplayViewer', startTime, 'html_not_found', 1);
        res.status(404).send('Replay viewer not found. Please build the WebAssembly replay viewer first.');
    }
});

// serve source files for WebAssembly debugging
router.get(/^\/contrib\/.+$/, (req, res) => {
    const startTime = debug.enter('serveSourceFile', [req.path], 2);
    
    // only serve source files in development or when debugging
    const isDevelopment = process.env.NODE_ENV === 'development';
    const isDebugMode = req.headers['user-agent']?.includes('Chrome') && req.headers.referer?.includes('replay-viewer');
    
    if (!isDevelopment && !isDebugMode) {
        debug.level2('Source file access denied - not in debug mode');
        debug.apiResponse(403, 'Source files not available in production', 2);
        debug.exit('serveSourceFile', startTime, 'access_denied', 2);
        return res.status(403).send('Source files not available in production');
    }
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving source file for debugging:', req.path);
    
    // construct the source file path from project root
    const sourcePath = path.join(__dirname, '../../../..', req.path);
    debug.fileOp('serve_source', sourcePath, 3);
    
    try {
        // check if file exists
        if (fs.existsSync(sourcePath)) {
            debug.level2('Source file found, serving:', path.basename(sourcePath));
            res.setHeader('Content-Type', 'text/plain; charset=utf-8');
            res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
            res.sendFile(sourcePath);
            debug.exit('serveSourceFile', startTime, 'success', 2);
        } else {
            debug.level2('Source file not found:', sourcePath);
            debug.apiResponse(404, 'Source file not found', 2);
            debug.exit('serveSourceFile', startTime, 'not_found', 2);
            res.status(404).send('Source file not found');
        }
    } catch (error) {
        debug.error('serveSourceFile', error, 1);
        debug.apiResponse(500, 'Error serving source file', 1);
        debug.exit('serveSourceFile', startTime, 'error', 1);
        res.status(500).send('Error serving source file');
    }
});

// serve source files from the source directory for WebAssembly debugging
router.get(/^\/source\/.+$/, (req, res) => {
    const startTime = debug.enter('serveSourceDirectory', [req.path], 2);
    
    // only serve source files in development or when explicitly debugging
    const isDevelopment = process.env.NODE_ENV === 'development';
    const isDebugMode = req.headers['user-agent']?.includes('Chrome') && req.headers.referer?.includes('replay-viewer');
    
    if (!isDevelopment && !isDebugMode) {
        debug.level2('Source file access denied - not in debug mode');
        debug.apiResponse(403, 'Source files not available in production', 2);
        debug.exit('serveSourceDirectory', startTime, 'access_denied', 2);
        return res.status(403).send('Source files not available in production');
    }
    
    debug.apiRequest('GET', req.originalUrl, req.user, 2);
    debug.level2('Serving source file for debugging:', req.path);
    
    // strip /source prefix and construct path from project root
    const relativePath = req.path.replace('/source/', '');
    const sourcePath = path.join(__dirname, '../../../..', relativePath);
    debug.fileOp('serve_source', sourcePath, 3);
    
    try {
        // check if file exists
        if (fs.existsSync(sourcePath)) {
            debug.level2('Source file found, serving:', path.basename(sourcePath));
            res.setHeader('Content-Type', 'text/plain; charset=utf-8');
            res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
            res.sendFile(sourcePath);
            debug.exit('serveSourceDirectory', startTime, 'success', 2);
        } else {
            debug.level2('Source file not found:', sourcePath);
            debug.apiResponse(404, 'Source file not found', 2);
            debug.exit('serveSourceDirectory', startTime, 'not_found', 2);
            res.status(404).send('Source file not found');
        }
    } catch (error) {
        debug.error('serveSourceDirectory', error, 1);
        debug.apiResponse(500, 'Error serving source file', 1);
        debug.exit('serveSourceDirectory', startTime, 'error', 1);
        res.status(500).send('Error serving source file');
    }
});

module.exports = router;