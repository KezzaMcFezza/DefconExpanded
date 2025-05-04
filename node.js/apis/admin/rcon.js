const express = require('express');
const router = express.Router();
const dgram = require('dgram');
const { authenticateToken, checkRole } = require('../../authentication');
const activeSessions = new Map();
const logBuffer = new Map();

// Session cleanup interval (5 minutes)
const CLEANUP_INTERVAL = 5 * 60 * 1000;
setInterval(() => {
    const now = Date.now();
    for (const [key, session] of activeSessions.entries()) {
        if (now - session.lastActivity > CLEANUP_INTERVAL) {
            if (session.socket) {
                session.socket.close();
            }
            activeSessions.delete(key);
        }
    }
}, CLEANUP_INTERVAL);

function storeLogMessage(sessionId, message) {
    if (!logBuffer.has(sessionId)) {
        logBuffer.set(sessionId, []);
    }
    logBuffer.get(sessionId).push(message);
    
    // Keep buffer from growing too large
    const buffer = logBuffer.get(sessionId);
    if (buffer.length > 100) {
        buffer.splice(0, buffer.length - 100);
    }
}

function setupLogListener(sessionId, socket, serverAddr) {
    // Set up message handler for log messages
    socket.on('message', (message) => {
        const response = message.toString('utf8');
        
        // Store log messages
        if (response.startsWith('LOG ')) {
            storeLogMessage(sessionId, response);
        }
    });
}

// Connect to RCON server
router.post('/apis/admin/rcon/connect', authenticateToken, checkRole(1), async (req, res) => {
    try {
        const { server, port, password } = req.body;
        
        if (!server || !port || !password) {
            return res.status(400).json({ 
                success: false, 
                message: 'Server address, port, and password are required' 
            });
        }

        const sessionId = `${req.user.id}-${server}-${port}`;
        
        // Close existing session if there is one
        if (activeSessions.has(sessionId)) {
            const existingSession = activeSessions.get(sessionId);
            if (existingSession.socket) {
                existingSession.socket.close();
            }
            activeSessions.delete(sessionId);
        }
        
        // Create a new UDP socket
        const socket = dgram.createSocket('udp4');
        
        // Create a promise to handle the authentication response
        const authPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                socket.removeAllListeners('message');
                reject(new Error('Connection timeout'));
            }, 5000);
            
            socket.on('message', (message) => {
                const response = message.toString('utf8');
                
                // Parse RCON response format (STATUS xxx\nMESSAGE yyy)
                const statusMatch = response.match(/STATUS (\d+)/);
                
                if (statusMatch && statusMatch[1] === '200') {
                    clearTimeout(timeout);
                    resolve(response);
                } else if (statusMatch) {
                    clearTimeout(timeout);
                    const messageMatch = response.match(/MESSAGE (.+)/);
                    reject(new Error(messageMatch ? messageMatch[1] : 'Authentication failed'));
                }
                // Continue listening for other messages (like logs)
            });
            
            socket.on('error', (err) => {
                clearTimeout(timeout);
                reject(err);
            });
        });
        
        // Send authentication packet
        const authCommand = `AUTH ${password}`;
        socket.send(authCommand, 0, authCommand.length, port, server);
        
        // Wait for authentication response
        const authResponse = await authPromise;
        
        // Store the session
        activeSessions.set(sessionId, {
            socket: socket,
            server: server,
            port: port,
            lastActivity: Date.now(),
            authenticated: true
        });
        
        // Set up log listener after successful authentication
        setupLogListener(sessionId, socket, { address: server, port: port });
        
        res.json({ 
            success: true, 
            message: 'Connected to RCON server', 
            sessionId: sessionId
        });
        
    } catch (error) {
        res.status(400).json({ 
            success: false, 
            message: `Failed to connect: ${error.message}` 
        });
    }
});

router.get('/apis/admin/rcon/logs/:sessionId', authenticateToken, checkRole(1), (req, res) => {
    const { sessionId } = req.params;
    
    if (!activeSessions.has(sessionId)) {
        return res.status(404).json({ 
            success: false, 
            message: 'Session not found or expired' 
        });
    }
    
    const logs = logBuffer.get(sessionId) || [];
    
    // Clear the buffer after sending
    logBuffer.set(sessionId, []);
    
    res.json({ 
        success: true, 
        logs: logs 
    });
});

// Execute RCON command
router.post('/apis/admin/rcon/execute', authenticateToken, checkRole(1), async (req, res) => {
    try {
        const { sessionId, command } = req.body;
        
        if (!sessionId || !command) {
            return res.status(400).json({ 
                success: false, 
                message: 'Session ID and command are required' 
            });
        }
        
        // Check if session exists
        if (!activeSessions.has(sessionId)) {
            return res.status(404).json({ 
                success: false, 
                message: 'Session not found or expired' 
            });
        }
        
        const session = activeSessions.get(sessionId);
        
        // Update last activity time
        session.lastActivity = Date.now();
        
        // Create a promise to handle the command response
        const commandPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Command timeout'));
            }, 10000);
            
            const messageHandler = (message) => {
                const response = message.toString('utf8');
                
                // Check if this is a log message
                if (response.startsWith('LOG ')) {
                    storeLogMessage(sessionId, response);
                    return; // Continue listening for the actual command response
                }
                
                // Parse RCON response format (STATUS xxx\nMESSAGE yyy)
                const statusMatch = response.match(/STATUS (\d+)/);
                
                if (statusMatch) {
                    clearTimeout(timeout);
                    session.socket.removeListener('message', messageHandler);
                    
                    if (statusMatch[1] === '200') {
                        resolve(response);
                    } else if (statusMatch[1] === '401') {
                        session.authenticated = false;
                        reject(new Error('Authentication required'));
                    } else {
                        const messageMatch = response.match(/MESSAGE (.+)/);
                        reject(new Error(messageMatch ? messageMatch[1] : 'Command failed'));
                    }
                }
            };
            
            session.socket.on('message', messageHandler);
            
            session.socket.once('error', (err) => {
                clearTimeout(timeout);
                session.socket.removeListener('message', messageHandler);
                reject(err);
            });
        });
        
        // Send command packet
        let rconCommand = command;
        // Convert commands to proper RCON format
        if (command.toLowerCase() === 'logstream-gameevents' || command === 'STARTGAMELOG') {
            rconCommand = 'STARTGAMELOG';
        } else if (command.toLowerCase() === 'logstream-serverlog' || command === 'STARTSERVERLOG') {
            rconCommand = 'STARTSERVERLOG';
        } else if (command.toLowerCase() === '!logstream-gameevents' || command === 'STOPGAMELOG') {
            rconCommand = 'STOPGAMELOG';
        } else if (command.toLowerCase() === '!logstream-serverlog' || command === 'STOPSERVERLOG') {
            rconCommand = 'STOPSERVERLOG';
        } else if (!command.startsWith('EXEC ') && !command.startsWith('AUTH ')) {
            rconCommand = `EXEC ${command}`;
        }
        
        session.socket.send(rconCommand, 0, rconCommand.length, session.port, session.server);
        
        // Wait for command response
        const commandResponse = await commandPromise;
        
        res.json({ 
            success: true, 
            response: commandResponse 
        });
        
    } catch (error) {
        if (error.message === 'Authentication required') {
            res.status(401).json({ 
                success: false, 
                message: 'Authentication required', 
                needsReauth: true 
            });
        } else {
            res.status(400).json({ 
                success: false, 
                message: `Command failed: ${error.message}` 
            });
        }
    }
});

// Disconnect from RCON server
router.post('/apis/admin/rcon/disconnect', authenticateToken, checkRole(1), (req, res) => {
    const { sessionId } = req.body;
    
    if (!sessionId) {
        return res.status(400).json({ 
            success: false, 
            message: 'Session ID is required' 
        });
    }
    
    if (activeSessions.has(sessionId)) {
        const session = activeSessions.get(sessionId);
        if (session.socket) {
            session.socket.close();
        }
        activeSessions.delete(sessionId);
    }
    
    res.json({ 
        success: true, 
        message: 'Disconnected from RCON server' 
    });
});

module.exports = router;