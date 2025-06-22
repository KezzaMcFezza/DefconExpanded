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

const {
    authenticateToken,
    checkPermission
} = require('../../authentication');

const permissions = require('../../permission-index');
const debug = require('../../debug-helpers');

router.post('/api/console/execute', authenticateToken, checkPermission(permissions.PAGE_SERVER_CONSOLE), async (req, res) => {
    const startTime = debug.enter('executeConsoleCommand', [req.body.command, req.user.username], 1);
    const { command } = req.body;
    
    debug.level2('Console command execution request:', { command, by: req.user.username });
    
    if (!command || typeof command !== 'string') {
        debug.level1('Invalid command provided');
        debug.exit('executeConsoleCommand', startTime, 'invalid_command', 1);
        return res.status(400).json({ 
            success: false, 
            error: 'Command is required and must be a string' 
        });
    }

    const allowedCommands = [
        '/restart',
        '/forcerestart',
        '/debug',
        '/quit',
        '/exit',
        '/status',
        '/memory',
        '/connections',
        '/clear',
        '/logs',
        '/performance',
        '/demos',
        '/errors',
        '/gc',
        '/pending',
        '/clearpending',
        '/help',
    ];

    const isAllowed = allowedCommands.some(allowed => 
        command.startsWith(allowed) || 
        command.includes(allowed) ||
        command.match(/^\/\w+(\s+\d+)?$/)
    );

    if (!isAllowed) {
        debug.level1('Unauthorized command attempted:', command);
        debug.exit('executeConsoleCommand', startTime, 'unauthorized', 1);
        return res.status(403).json({ 
            success: false, 
            error: 'Command not allowed. Only debug and system commands are permitted.' 
        });
    }

    debug.level2('Executing allowed command:', command);

    try {
        if (command.startsWith('/')) {
            const result = await handleDebugCommand(command);
            debug.level2('Debug command executed successfully');
            debug.exit('executeConsoleCommand', startTime, 'success', 1);
            return res.json({
                success: true,
                output: result
            });
        }

        if (command.startsWith('process.') || command.startsWith('debug.')) {
            const result = await handleNodeCommand(command);
            debug.level2('Node command executed successfully');
            debug.exit('executeConsoleCommand', startTime, 'success', 1);
            return res.json({
                success: true,
                output: result
            });
        }

        debug.level1('Unsupported command type:', command);
        debug.exit('executeConsoleCommand', startTime, 'unsupported', 1);
        res.status(400).json({
            success: false,
            error: 'Unsupported command type'
        });

    } catch (error) {
        debug.error('executeConsoleCommand', error, 1);
        debug.exit('executeConsoleCommand', startTime, 'error', 1);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

async function handleDebugCommand(command) {
    const debugUtils = require('../../debug-utils');
    const { pool } = require('../../constants');
    const parts = command.slice(1).split(' '); 
    const cmd = parts[0].toLowerCase();
    const args = parts.slice(1);

    switch (cmd) {
        case 'restart':
            console.log('Restart command received via web interface. Initiating restart...');
            debugUtils.restartServer();
            return 'Server restart initiated. The server will close database connections and restart the process.';

        case 'forcerestart':
            console.log('Force restart command received via web interface. Initiating immediate restart...');
            debugUtils.forceRestart();
            return 'Force restart initiated. The server will restart immediately without waiting for graceful shutdown.';
            
        case 'debug':
            const level = parseInt(args[0]) || 0;
            if (level < 0 || level > 3) {
                return 'Debug level must be between 0-3';
            }
            debugUtils.setDebugLevel(level);
            let debugMsg = `Debug level set to ${level}\n`;
            switch (level) {
                case 0:
                    debugMsg += 'Debug disabled';
                    break;
                case 1:
                    debugMsg += 'Debug Level 1: Basic function names';
                    break;
                case 2:
                    debugMsg += 'Debug Level 2: Function names + variables + timings';
                    break;
                case 3:
                    debugMsg += 'Debug Level 3: Full callstack information';
                    break;
            }
            return debugMsg;

        case 'quit':
        case 'exit':
            return 'Server shutdown initiated. Note: This will terminate the current session.';
            
        case 'status':
            try {
                const uptime = process.uptime();
                const memUsage = process.memoryUsage();
                
                let statusMsg = '\n=== SERVER STATUS ===\n';
                statusMsg += `Uptime: ${Math.floor(uptime / 3600)}h ${Math.floor((uptime % 3600) / 60)}m ${Math.floor(uptime % 60)}s\n`;
                statusMsg += `Memory Usage: ${Math.round(memUsage.rss / 1024 / 1024)}MB RSS\n`;
                statusMsg += `Debug Level: ${debugUtils.debugLevel}\n`;
                statusMsg += `Node.js Version: ${process.version}\n`;
                statusMsg += `Platform: ${process.platform}\n`;
                
                try {
                    const [demos] = await pool.query('SELECT COUNT(*) as count FROM demos');
                    const [users] = await pool.query('SELECT COUNT(*) as count FROM users');
                    const [mods] = await pool.query('SELECT COUNT(*) as count FROM modlist');
                    
                    statusMsg += `Total Demos: ${demos[0].count}\n`;
                    statusMsg += `Total Users: ${users[0].count}\n`;
                    statusMsg += `Total Mods: ${mods[0].count}\n`;
                } catch (error) {
                    statusMsg += `Database connection error: ${error.message}\n`;
                }
                return statusMsg;
            } catch (error) {
                return `Error getting server status: ${error.message}`;
            }

        case 'memory':
            const mem = process.memoryUsage();
            return `\n=== MEMORY USAGE ===
RSS: ${Math.round(mem.rss / 1024 / 1024)}MB
Heap Total: ${Math.round(mem.heapTotal / 1024 / 1024)}MB
Heap Used: ${Math.round(mem.heapUsed / 1024 / 1024)}MB
External: ${Math.round(mem.external / 1024 / 1024)}MB
Array Buffers: ${Math.round(mem.arrayBuffers / 1024 / 1024)}MB`;

        case 'connections':
            try {
                const [connections] = await pool.query('SHOW PROCESSLIST');
                let connMsg = '\n=== DATABASE CONNECTIONS ===\n';
                connMsg += `Active connections: ${connections.length}\n`;
                connections.forEach((conn, index) => {
                    connMsg += `${index + 1}. ID: ${conn.Id}, User: ${conn.User}, State: ${conn.State}, Time: ${conn.Time}s\n`;
                });
                return connMsg;
            } catch (error) {
                return `Error fetching database connections: ${error.message}`;
            }

        case 'clear':
            try {
                const fs = require('fs');
                fs.writeFileSync(debugUtils.debugFile, '');
                return 'Debug file cleared.';
            } catch (error) {
                return `Error clearing debug file: ${error.message}`;
            }

        case 'logs':
            const lines = parseInt(args[0]) || 50;
            try {
                const fs = require('fs');
                const data = fs.readFileSync(debugUtils.debugFile, 'utf8');
                const logLines = data.split('\n').filter(line => line.trim());
                const recentLines = logLines.slice(-lines);
                return `\n=== RECENT DEBUG LOGS (${recentLines.length} lines) ===\n${recentLines.join('\n')}`;
            } catch (error) {
                return `No debug logs available or error reading file: ${error.message}`;
            }

        case 'performance':
            let perfMsg = '\n=== PERFORMANCE METRICS ===\n';
            if (debugUtils.performanceMetrics.size === 0) {
                perfMsg += 'No performance data available. Enable debug level 2+ to collect metrics.';
            } else {
                for (const [func, metrics] of debugUtils.performanceMetrics) {
                    perfMsg += `${func}: Avg ${metrics.avgTime}ms, Calls: ${metrics.calls}, Total: ${metrics.totalTime}ms\n`;
                }
            }
            return perfMsg;

        case 'demos':
            try {
                const [stats] = await pool.query(`
                    SELECT 
                        COUNT(*) as total_demos,
                        COUNT(CASE WHEN date >= DATE_SUB(NOW(), INTERVAL 1 DAY) THEN 1 END) as today,
                        COUNT(CASE WHEN date >= DATE_SUB(NOW(), INTERVAL 7 DAY) THEN 1 END) as this_week,
                        AVG(download_count) as avg_downloads,
                        MAX(download_count) as max_downloads
                    FROM demos
                `);
                
                return `\n=== DEMO STATISTICS ===
Total Demos: ${stats[0].total_demos}
Added Today: ${stats[0].today}
Added This Week: ${stats[0].this_week}
Average Downloads: ${Math.round(stats[0].avg_downloads)}
Max Downloads: ${stats[0].max_downloads}`;
            } catch (error) {
                return `Error fetching demo stats: ${error.message}`;
            }

        case 'errors':
            try {
                const fs = require('fs');
                const data = fs.readFileSync(debugUtils.debugFile, 'utf8');
                const errorLines = data.split('\n').filter(line => 
                    line.includes('[ERROR]') || line.includes('[WARN]')
                );
                
                let errorsMsg = '\n=== RECENT ERRORS ===\n';
                if (errorLines.length === 0) {
                    errorsMsg += 'No recent errors found.';
                } else {
                    errorsMsg += errorLines.slice(-20).join('\n');
                }
                return errorsMsg;
            } catch (error) {
                return `Error reading debug file: ${error.message}`;
            }

        case 'gc':
            if (global.gc) {
                global.gc();
                const mem = process.memoryUsage();
                return `Garbage collection forced.
                
=== MEMORY USAGE AFTER GC ===
RSS: ${Math.round(mem.rss / 1024 / 1024)}MB
Heap Total: ${Math.round(mem.heapTotal / 1024 / 1024)}MB
Heap Used: ${Math.round(mem.heapUsed / 1024 / 1024)}MB
External: ${Math.round(mem.external / 1024 / 1024)}MB`;
            } else {
                return 'Garbage collection not available. Start with --expose-gc flag.';
            }

        case 'pending':
            try {
                const { pendingDemos, pendingJsons, processedJsons } = require('../../demos');
                
                let result = '\n=== PENDING LISTS STATUS ===\n';
                result += `Pending Demo Files: ${pendingDemos.size}\n`;
                result += `Pending JSON Files: ${pendingJsons.size}\n`;
                result += `Processed JSON Files: ${processedJsons.size}\n`;
                
                if (pendingDemos.size > 0) {
                    result += '\nPending Demo Files:\n';
                    let count = 0;
                    for (const [fileName, fileInfo] of pendingDemos) {
                        if (count < 10) {
                            const age = Math.round((Date.now() - fileInfo.addedTime) / 1000);
                            result += `  - ${fileName} (${age}s ago)\n`;
                            count++;
                        } else {
                            result += `  ... and ${pendingDemos.size - 10} more\n`;
                            break;
                        }
                    }
                }
                
                if (pendingJsons.size > 0) {
                    result += '\nPending JSON Files:\n';
                    let count = 0;
                    for (const [fileName, fileInfo] of pendingJsons) {
                        if (count < 10) {
                            const age = Math.round((Date.now() - fileInfo.addedTime) / 1000);
                            result += `  - ${fileName} (${age}s ago)\n`;
                            count++;
                        } else {
                            result += `  ... and ${pendingJsons.size - 10} more\n`;
                            break;
                        }
                    }
                }
                
                return result;
            } catch (error) {
                return `Error showing pending lists: ${error.message}`;
            }

        case 'clearpending':
            try {
                const { pendingDemos, pendingJsons, processedJsons } = require('../../demos');
                
                const pendingDemosCount = pendingDemos.size;
                const pendingJsonsCount = pendingJsons.size;
                const processedJsonsCount = processedJsons.size;
                
                pendingDemos.clear();
                pendingJsons.clear();
                processedJsons.clear();
                
                let result = '\n=== PENDING LISTS CLEARED ===\n';
                result += `Cleared ${pendingDemosCount} pending demo files\n`;
                result += `Cleared ${pendingJsonsCount} pending JSON files\n`;
                result += `Cleared ${processedJsonsCount} processed JSON files\n`;
                result += 'All pending file lists have been reset.\n';
                result += 'The server will now treat all files as new when they are detected again.\n';
                
                debugUtils.writeToDebugFile(`PENDING LISTS CLEARED - Demos: ${pendingDemosCount}, JSONs: ${pendingJsonsCount}, Processed: ${processedJsonsCount}`, 'SYSTEM');
                
                return result;
            } catch (error) {
                return `Error clearing pending lists: ${error.message}`;
            }

        case 'download':
            return `To download logs, use the "Download Logs (ZIP)" button above.
This will download a ZIP file containing:
- server.log (Node.js server logs)
- debug.txt (Debug system logs)
- download_info.txt (Download timestamp and user info)`;

        case 'help':
            return `\n
/restart        - Restart the server (graceful restart)
/forcerestart   - Force restart (hard restart)
/debug <level>  - Set debug level (0-3)
/quit           - Shutdown the server
/status         - Show server status
/memory         - Show memory usage
/connections    - Show database connections
/clear          - Clear debug log file
/logs <lines>   - Show recent debug logs
/performance    - Show performance metrics
/demos          - Show demo statistics
/errors         - Show recent errors
/gc             - Force garbage collection
/pending        - Show pending demo/JSON lists status
/clearpending   - Clear pending demo/JSON lists
/help           - Show this help

Note: Use the checkboxes above to enable/disable log streams.`;

        default:
            return `Unknown command: ${cmd}. Type /help for available commands.`;
    }
}

async function handleNodeCommand(command) {
    try {
        if (command === 'process.memoryUsage()') {
            const mem = process.memoryUsage();
            return JSON.stringify(mem, null, 2);
        }
        
        if (command === 'process.uptime()') {
            return `${process.uptime()} seconds`;
        }
        
        if (command === 'process.version') {
            return process.version;
        }
        
        if (command === 'process.platform') {
            return process.platform;
        }

        if (command.startsWith('debug.level')) {
            const debug = require('../../debug-helpers');
            const level = command.match(/\d+/)?.[0];
            if (level) {
                debug[`level${level}`](`Test message from console at level ${level}`);
                return `Debug level ${level} message sent`;
            }
        }

        return 'Command not supported or invalid syntax';
    } catch (error) {
        throw new Error(`Evaluation error: ${error.message}`);
    }
}

module.exports = router; 