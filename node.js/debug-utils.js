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

const fs = require('fs');
const path = require('path');
const readline = require('readline');
const { pool } = require('./constants');

class DebugUtils {
    constructor() {
        this.debugLevel = 0;
        this.debugFile = path.join(__dirname, 'debug.txt');
        this.originalConsoleLog = console.log;
        this.originalConsoleError = console.error;
        this.originalConsoleWarn = console.warn;
        this.functionCallStack = [];
        this.performanceMetrics = new Map();
        this.isDebugging = false;
        this.serverInstance = null;
        
        this.initializeConsoleInterface();
        this.setupDebugLogging();
        this.clearDebugFile();
    }

    initializeConsoleInterface() {
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            prompt: 'DefconExpanded> '
        });

        rl.on('line', async (input) => {
            await this.handleCommand(input.trim());
            rl.prompt();
        });

        rl.on('close', () => {
            console.log('Console interface closed.');
            process.exit(0);
        });

        console.log('DefconExpanded Debug Console initialized. Type /help for commands.');
        rl.prompt();
    }

    async handleCommand(command) {
        const args = command.split(' ');
        const cmd = args[0].toLowerCase();

        switch (cmd) {
            case '/restart':
                this.restartServer();
                break;
            case '/debug':
                this.setDebugLevel(parseInt(args[1]) || 0);
                break;
            case '/quit':
            case '/exit':
                this.quitServer();
                break;
            case '/status':
                await this.showServerStatus();
                break;
            case '/memory':
                this.showMemoryUsage();
                break;
            case '/connections':
                await this.showDatabaseConnections();
                break;
            case '/clear':
                this.clearDebugFile();
                break;
            case '/logs':
                this.showRecentLogs(parseInt(args[1]) || 50);
                break;
            case '/performance':
                this.showPerformanceMetrics();
                break;
            case '/demos':
                await this.showDemoStats();
                break;
            case '/errors':
                this.showRecentErrors();
                break;
            case '/gc':
                this.forceGarbageCollection();
                break;
            case '/clearpending':
                await this.clearPendingLists();
                break;
            case '/pending':
                this.showPendingLists();
                break;
            case '/help':
                this.showHelp();
                break;
            case '/forcerestart':
                this.forceRestart();
                break;
            default:
                console.log(`Unknown command: ${cmd}. Type /help for available commands.`);
        }
    }

    restartServer() {
        console.log('Restarting server...');
        this.writeToDebugFile('SERVER RESTART INITIATED', 'SYSTEM');
        
        setTimeout(() => {
            if (pool) {
                pool.end().then(() => {
                    console.log('Database connections closed.');
                    this.performRestart();
                }).catch((err) => {
                    console.error('Error closing database connections:', err);
                    this.performRestart();
                });
            } else {
                this.performRestart();
            }
        }, 500);
    }

    performRestart() {
        console.log('Performing server restart...');

        if (this.serverInstance) {
            console.log('Closing HTTP server...');
            
            const forceCloseTimeout = setTimeout(() => {
                console.log('Force closing server due to timeout...');
                this.attemptRestart();
            }, 3000);
            
            this.serverInstance.close(() => {
                clearTimeout(forceCloseTimeout);
                console.log('Server closed gracefully. Restarting process...');
                this.attemptRestart();
            });
        } else {
            console.log('No server instance found. Exiting process...');
            this.attemptRestart();
        }
    }

    attemptRestart() {
        console.log('Attempting restart...');
        
        try {
            const { spawn } = require('child_process');
            const path = require('path');
            const scriptPath = path.resolve(process.argv[1]);
            const workingDir = process.cwd();
            
            console.log('Spawning new server process...');
            console.log(`Script path: ${scriptPath}`);
            console.log(`Working directory: ${workingDir}`);
            
            const child = spawn(process.execPath, [scriptPath], {
                cwd: workingDir,
                detached: true,
                stdio: 'ignore',
                windowsHide: false
            });
            
            child.unref();
            console.log('Child process spawned. Exiting...');
            
        } catch (error) {
            console.error('Failed to spawn new process:', error.message);
            console.log('Manual restart required. Use restart-server.bat for automatic restarts.');
        }
        
        console.log('Exiting current process...');
        process.exit(0);
    }

    setDebugLevel(level) {
        if (level < 0 || level > 3) {
            console.log('Debug level must be between 0-3');
            return;
        }

        this.debugLevel = level;
        this.isDebugging = level > 0;
        
        console.log(`Debug level set to ${level}`);
        this.writeToDebugFile(`DEBUG LEVEL CHANGED TO ${level}`, 'SYSTEM');

        switch (level) {
            case 0:
                console.log('Debug disabled');
                break;
            case 1:
                console.log('Debug Level 1: Basic function names');
                break;
            case 2:
                console.log('Debug Level 2: Function names + variables + timings');
                break;
            case 3:
                console.log('Debug Level 3: Full callstack information');
                break;
        }
    }

    quitServer() {
        console.log('Shutting down server...');
        this.writeToDebugFile('SERVER SHUTDOWN INITIATED', 'SYSTEM');
        
        
        if (pool) {
            pool.end().then(() => {
                console.log('Database connections closed.');
                process.exit(0);
            }).catch((err) => {
                console.error('Error closing database connections:', err);
                process.exit(1);
            });
        } else {
            process.exit(0);
        }
    }

    async showServerStatus() {
        const uptime = process.uptime();
        const memUsage = process.memoryUsage();
        
        console.log('\n=== SERVER STATUS ===');
        console.log(`Uptime: ${Math.floor(uptime / 3600)}h ${Math.floor((uptime % 3600) / 60)}m ${Math.floor(uptime % 60)}s`);
        console.log(`Memory Usage: ${Math.round(memUsage.rss / 1024 / 1024)}MB RSS`);
        console.log(`Debug Level: ${this.debugLevel}`);
        console.log(`Node.js Version: ${process.version}`);
        console.log(`Platform: ${process.platform}`);
        
        try {
            const [demos] = await pool.query('SELECT COUNT(*) as count FROM demos');
            const [users] = await pool.query('SELECT COUNT(*) as count FROM users');
            const [mods] = await pool.query('SELECT COUNT(*) as count FROM modlist');
            
            console.log(`Total Demos: ${demos[0].count}`);
            console.log(`Total Users: ${users[0].count}`);
            console.log(`Total Mods: ${mods[0].count}`);
        } catch (error) {
            console.log('Database connection error:', error.message);
        }
    }

    showMemoryUsage() {
        const memUsage = process.memoryUsage();
        
        console.log('\n=== MEMORY USAGE ===');
        console.log(`RSS: ${Math.round(memUsage.rss / 1024 / 1024)}MB`);
        console.log(`Heap Total: ${Math.round(memUsage.heapTotal / 1024 / 1024)}MB`);
        console.log(`Heap Used: ${Math.round(memUsage.heapUsed / 1024 / 1024)}MB`);
        console.log(`External: ${Math.round(memUsage.external / 1024 / 1024)}MB`);
        console.log(`Array Buffers: ${Math.round(memUsage.arrayBuffers / 1024 / 1024)}MB`);
    }

    async showDatabaseConnections() {
        try {
            const [connections] = await pool.query('SHOW PROCESSLIST');
            console.log('\n=== DATABASE CONNECTIONS ===');
            console.log(`Active connections: ${connections.length}`);
            connections.forEach((conn, index) => {
                console.log(`${index + 1}. ID: ${conn.Id}, User: ${conn.User}, State: ${conn.State}, Time: ${conn.Time}s`);
            });
        } catch (error) {
            console.log('Error fetching database connections:', error.message);
        }
    }

    clearDebugFile() {
        try {
            fs.writeFileSync(this.debugFile, '');
            console.log('Debug file cleared.');
        } catch (error) {
            console.error('Error clearing debug file:', error.message);
        }
    }

    showRecentLogs(lines = 50) {
        try {
            const data = fs.readFileSync(this.debugFile, 'utf8');
            const logLines = data.split('\n').filter(line => line.trim());
            const recentLines = logLines.slice(-lines);
            
            console.log(`\n=== RECENT DEBUG LOGS (${recentLines.length} lines) ===`);
            recentLines.forEach(line => console.log(line));
        } catch (error) {
            console.log('No debug logs available or error reading file:', error.message);
        }
    }

    showPerformanceMetrics() {
        console.log('\n=== PERFORMANCE METRICS ===');
        if (this.performanceMetrics.size === 0) {
            console.log('No performance data available. Enable debug level 2+ to collect metrics.');
        } else {
            for (const [func, metrics] of this.performanceMetrics) {
                console.log(`${func}: Avg ${metrics.avgTime}ms, Calls: ${metrics.calls}, Total: ${metrics.totalTime}ms`);
            }
        }
    }

    async showActiveUsers() {
        try {
            const [recentUsers] = await pool.query(`
                SELECT u.username, up.defcon_username, 
                       (SELECT COUNT(*) FROM demos WHERE 
                        player1_name = up.defcon_username OR player2_name = up.defcon_username OR 
                        player3_name = up.defcon_username OR player4_name = up.defcon_username OR
                        player5_name = up.defcon_username OR player6_name = up.defcon_username OR
                        player7_name = up.defcon_username OR player8_name = up.defcon_username OR
                        player9_name = up.defcon_username OR player10_name = up.defcon_username
                        AND date >= DATE_SUB(NOW(), INTERVAL 7 DAY)) as recent_games
                FROM users u 
                JOIN user_profiles up ON u.id = up.user_id 
                WHERE up.defcon_username IS NOT NULL 
                ORDER BY recent_games DESC 
                LIMIT 10
            `);
            
            console.log('\n=== ACTIVE USERS (Last 7 days) ===');
            recentUsers.forEach((user, index) => {
                console.log(`${index + 1}. ${user.username} (${user.defcon_username}) - ${user.recent_games} games`);
            });
        } catch (error) {
            console.log('Error fetching active users:', error.message);
        }
    }

    async showDemoStats() {
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
            
            console.log('\n=== DEMO STATISTICS ===');
            console.log(`Total Demos: ${stats[0].total_demos}`);
            console.log(`Added Today: ${stats[0].today}`);
            console.log(`Added This Week: ${stats[0].this_week}`);
            console.log(`Average Downloads: ${Math.round(stats[0].avg_downloads)}`);
            console.log(`Max Downloads: ${stats[0].max_downloads}`);
        } catch (error) {
            console.log('Error fetching demo stats:', error.message);
        }
    }

    showRecentErrors() {
        try {
            const data = fs.readFileSync(this.debugFile, 'utf8');
            const errorLines = data.split('\n').filter(line => 
                line.includes('[ERROR]') || line.includes('[WARN]')
            );
            
            console.log('\n=== RECENT ERRORS ===');
            if (errorLines.length === 0) {
                console.log('No recent errors found.');
            } else {
                errorLines.slice(-20).forEach(line => console.log(line));
            }
        } catch (error) {
            console.log('Error reading debug file:', error.message);
        }
    }

    forceGarbageCollection() {
        if (global.gc) {
            global.gc();
            console.log('Garbage collection forced.');
            this.showMemoryUsage();
        } else {
            console.log('Garbage collection not available. Start with --expose-gc flag.');
        }
    }

    async clearPendingLists() {
        try {
            const { pendingDemos, pendingJsons, processedJsons } = require('./demos');
            const { pool, demoDir, gameLogsDir } = require('./constants');
            const pendingDemosCount = pendingDemos.size;
            const pendingJsonsCount = pendingJsons.size;
            const processedJsonsCount = processedJsons.size;
            
            pendingDemos.clear();
            pendingJsons.clear();
            processedJsons.clear();
            
            console.log('\n=== PENDING LISTS CLEARED ===');
            console.log(`Cleared ${pendingDemosCount} pending demo files`);
            console.log(`Cleared ${pendingJsonsCount} pending JSON files`);
            console.log(`Cleared ${processedJsonsCount} processed JSON files`);
            console.log('All pending file lists have been reset.');
            console.log('Rescanning directories for files...');
            
            this.writeToDebugFile(`PENDING LISTS CLEARED - Demos: ${pendingDemosCount}, JSONs: ${pendingJsonsCount}, Processed: ${processedJsonsCount}`, 'SYSTEM');
            
            await this.rescanDirectories();
            
            console.log('Directory rescan completed.');
            
        } catch (error) {
            console.error('Error clearing pending lists:', error.message);
            this.writeToDebugFile(`ERROR CLEARING PENDING LISTS: ${error.message}`, 'ERROR');
        }
    }

    async rescanDirectories() {
        try {
            const { pendingDemos, pendingJsons, processedJsons, checkForMatch } = require('./demos');
            const { pool, demoDir, gameLogsDir } = require('./constants');
            const fs = require('fs');
            const path = require('path');
            
            console.log('Scanning demo and JSON directories...');
            
            const demoFiles = await fs.promises.readdir(demoDir);
            console.log(`Found ${demoFiles.length} demo files in the directory.`);
            
            const jsonFiles = await fs.promises.readdir(gameLogsDir);
            console.log(`Found ${jsonFiles.length} JSON files in the directory.`);

            const [rows] = await pool.query('SELECT name, log_file FROM demos');
            const existingDemos = new Map(rows.map(row => [row.name, row.log_file]));
            const existingJsonFiles = new Set(rows.map(row => row.log_file));

            for (const demoFile of demoFiles) {
                if (demoFile.endsWith('.dcrec') || demoFile.endsWith('.d8crec') || demoFile.endsWith('.d10crec')) {
                    if (!existingDemos.has(demoFile)) {
                        const demoStats = await fs.promises.stat(path.join(demoDir, demoFile));
                        pendingDemos.set(demoFile, { 
                            stats: demoStats, 
                            path: path.join(demoDir, demoFile),
                            addedTime: Date.now() 
                        });
                        console.log(`Demo ${demoFile} added to pending list.`);
                    } else {
                        console.log(`Demo ${demoFile} already exists in the database. Skipping.`);
                    }
                }
            }
            
            for (const jsonFile of jsonFiles) {
                if (jsonFile.endsWith('_full.json')) {
                    if (!existingJsonFiles.has(jsonFile)) {
                        pendingJsons.set(jsonFile, { 
                            path: path.join(gameLogsDir, jsonFile), 
                            addedTime: Date.now() 
                        });
                        console.log(`JSON ${jsonFile} added to pending list.`);
                    } else {
                        processedJsons.add(jsonFile);
                        console.log(`JSON ${jsonFile} is already linked to a demo in the database. Skipping.`);
                    }
                }
            }
            
            console.log(`Loaded ${pendingDemos.size} pending demo files.`);
            console.log(`Loaded ${pendingJsons.size} pending JSON files.`);
            console.log(`${existingDemos.size} demos already exist in the database.`);
            
            await checkForMatch();
            console.log('Initial demo matching check completed after rescan.');
            
        } catch (error) {
            console.error('Error during directory rescan:', error);
            this.writeToDebugFile(`ERROR DURING DIRECTORY RESCAN: ${error.message}`, 'ERROR');
            throw error;
        }
    }

    showPendingLists() {
        try {
            const { pendingDemos, pendingJsons, processedJsons } = require('./demos');
            
            console.log('\n=== PENDING LISTS STATUS ===');
            console.log(`Pending Demo Files: ${pendingDemos.size}`);
            console.log(`Pending JSON Files: ${pendingJsons.size}`);
            console.log(`Processed JSON Files: ${processedJsons.size}`);
            
            if (pendingDemos.size > 0) {
                console.log('\nPending Demo Files:');
                let count = 0;
                for (const [fileName, fileInfo] of pendingDemos) {
                    if (count < 10) { 
                        const age = Math.round((Date.now() - fileInfo.addedTime) / 1000);
                        console.log(`  - ${fileName} (${age}s ago)`);
                        count++;
                    } else {
                        console.log(`  ... and ${pendingDemos.size - 10} more`);
                        break;
                    }
                }
            }
            
            if (pendingJsons.size > 0) {
                console.log('\nPending JSON Files:');
                let count = 0;
                for (const [fileName, fileInfo] of pendingJsons) {
                    if (count < 10) {
                        const age = Math.round((Date.now() - fileInfo.addedTime) / 1000);
                        console.log(`  - ${fileName} (${age}s ago)`);
                        count++;
                    } else {
                        console.log(`  ... and ${pendingJsons.size - 10} more`);
                        break;
                    }
                }
            }
            
            if (processedJsons.size > 0) {
                console.log('\nProcessed JSON Files:');
                let count = 0;
                for (const fileName of processedJsons) {
                    if (count < 10) { 
                        console.log(`  - ${fileName}`);
                        count++;
                    } else {
                        console.log(`  ... and ${processedJsons.size - 10} more`);
                        break;
                    }
                }
            }
            
            
        } catch (error) {
            console.error('Error showing pending lists:', error.message);
            this.writeToDebugFile(`ERROR SHOWING PENDING LISTS: ${error.message}`, 'ERROR');
        }
    }

    forceRestart() {
        console.log('Force restarting server immediately...');
        this.writeToDebugFile('FORCE RESTART INITIATED', 'SYSTEM');
        
        if (pool) {
            pool.end().catch(() => {}); 
        }
        
        if (this.serverInstance) {
            this.serverInstance.closeAllConnections?.(); 
            this.serverInstance.close(() => {});
        }

        setTimeout(() => {
            this.attemptRestart();
        }, 100); 
    }

    showHelp() {
        console.log('/restart        - Restart the server (gracefully restart)');
        console.log('/debug <level>  - Set debug level (0-3)');
        console.log('/quit           - Shutdown the server');
        console.log('/status         - Show server status');
        console.log('/memory         - Show memory usage');
        console.log('/connections    - Show database connections');
        console.log('/clear          - Clear debug log file');
        console.log('/logs <lines>   - Show recent debug logs');
        console.log('/performance    - Show performance metrics');
        console.log('/demos          - Show demo statistics');
        console.log('/errors         - Show recent errors');
        console.log('/gc             - Force garbage collection');
        console.log('/pending        - Show pending demo/JSON lists status');
        console.log('/clearpending   - Clear pending demo/JSON lists and rescan directories');
        console.log('/forcerestart   - Force restart (hard restart)');
        console.log('/help           - Show this help');
    }

    setupDebugLogging() {
        
        console.log = (...args) => {
            this.originalConsoleLog(...args);
            if (this.isDebugging) {
                this.writeToDebugFile(args.join(' '), 'LOG');
            }
        };

        console.error = (...args) => {
            this.originalConsoleError(...args);
            this.writeToDebugFile(args.join(' '), 'ERROR');
        };

        console.warn = (...args) => {
            this.originalConsoleWarn(...args);
            this.writeToDebugFile(args.join(' '), 'WARN');
        };

        
        console.debug = (level, ...args) => {
            if (typeof level !== 'number') {
                
                args.unshift(level);
                level = 1;
            }

            if (this.debugLevel >= level) {
                const message = args.join(' ');
                const timestamp = new Date().toISOString();
                const debugMessage = `[DEBUG-L${level}] ${message}`;
                
                
                this.writeToDebugFile(debugMessage, 'DEBUG');
                
                
                if (this.debugLevel >= 1) {
                    this.originalConsoleLog(`[${timestamp}] ${debugMessage}`);
                }
            }
        };
    }

    writeToDebugFile(message, level = 'INFO') {
        const timestamp = new Date().toISOString();
        const logEntry = `[${timestamp}] [${level}] ${message}\n`;
        
        try {
            fs.appendFileSync(this.debugFile, logEntry);
        } catch (error) {
            this.originalConsoleError('Error writing to debug file:', error.message);
        }
    }

    
    getCallStack(skipFrames = 2) {
        const stack = new Error().stack;
        if (!stack) return 'No stack available';
        
        const lines = stack.split('\n').slice(skipFrames);
        return lines.slice(0, 5).map((line, index) => {
            const cleaned = line.trim().replace(/^at\s+/, '');
            return `  ${index + 1}. ${cleaned}`;
        }).join('\n');
    }

    
    debugFunctionEntry(functionName, args = [], level = 1) {
        if (this.debugLevel >= level) {
            const argsStr = level >= 2 ? ` with args: ${JSON.stringify(args).substring(0, 200)}` : '';
            console.debug(level, `→ ENTER ${functionName}()${argsStr}`);
            
            if (level >= 3 && this.debugLevel >= 3) {
                console.debug(3, `Call stack for ${functionName}:\n${this.getCallStack()}`);
            }
        }
        return Date.now();
    }

    
    debugFunctionExit(functionName, startTime, result = null, level = 1) {
        if (this.debugLevel >= level) {
            const duration = Date.now() - startTime;
            const durationStr = level >= 2 ? ` (${duration}ms)` : '';
            const resultStr = level >= 2 && result !== null ? ` → ${JSON.stringify(result).substring(0, 200)}` : '';
            console.debug(level, `← EXIT ${functionName}()${durationStr}${resultStr}`);
        }
    }

    
    traceFunction(functionName, args = [], startTime = Date.now()) {
        if (!this.isDebugging) return;

        const callInfo = {
            name: functionName,
            args: this.debugLevel >= 2 ? args : [],
            startTime,
            stack: this.debugLevel >= 3 ? new Error().stack : null
        };

        this.functionCallStack.push(callInfo);

        if (this.debugLevel >= 1) {
            const indent = '  '.repeat(this.functionCallStack.length - 1);
            let logMessage = `${indent}→ ${functionName}()`;
            
            if (this.debugLevel >= 2 && args.length > 0) {
                logMessage += ` with args: ${JSON.stringify(args).substring(0, 100)}`;
            }
            
            this.writeToDebugFile(logMessage, 'TRACE');
        }

        if (this.debugLevel >= 3 && callInfo.stack) {
            this.writeToDebugFile(`Stack: ${callInfo.stack}`, 'STACK');
        }

        return callInfo;
    }

    traceEnd(callInfo, result = null) {
        if (!this.isDebugging || !callInfo) return;

        const endTime = Date.now();
        const duration = endTime - callInfo.startTime;
        
        
        const index = this.functionCallStack.indexOf(callInfo);
        if (index > -1) {
            this.functionCallStack.splice(index, 1);
        }

        
        if (this.debugLevel >= 2) {
            if (!this.performanceMetrics.has(callInfo.name)) {
                this.performanceMetrics.set(callInfo.name, {
                    calls: 0,
                    totalTime: 0,
                    avgTime: 0
                });
            }

            const metrics = this.performanceMetrics.get(callInfo.name);
            metrics.calls++;
            metrics.totalTime += duration;
            metrics.avgTime = Math.round(metrics.totalTime / metrics.calls);
        }

        if (this.debugLevel >= 1) {
            const indent = '  '.repeat(this.functionCallStack.length);
            let logMessage = `${indent}← ${callInfo.name}()`;
            
            if (this.debugLevel >= 2) {
                logMessage += ` (${duration}ms)`;
                if (result !== null) {
                    logMessage += ` returned: ${JSON.stringify(result).substring(0, 100)}`;
                }
            }
            
            this.writeToDebugFile(logMessage, 'TRACE');
        }
    }

    
    trace(functionName, fn, ...args) {
        const callInfo = this.traceFunction(functionName, args);
        
        try {
            const result = fn(...args);
            
            
            if (result && typeof result.then === 'function') {
                return result.then(res => {
                    this.traceEnd(callInfo, res);
                    return res;
                }).catch(err => {
                    this.traceEnd(callInfo, `ERROR: ${err.message}`);
                    throw err;
                });
            } else {
                this.traceEnd(callInfo, result);
                return result;
            }
        } catch (error) {
            this.traceEnd(callInfo, `ERROR: ${error.message}`);
            throw error;
        }
    }

    setServerInstance(server) {
        this.serverInstance = server;
    }
}


const debugUtils = new DebugUtils();

module.exports = debugUtils; 