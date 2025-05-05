/**
 * DefconExpanded, Created by...
 * KezzaMcFezza - Main Developer
 * Nexustini - Server Managment
 *
 * Notable Mentions...
 * Rad - For helping with python scripts.
 * Bert_the_turtle - Doing everthing with c++
 * 
 * Inspired by Sievert and Wan May
 * 
 * Last Edited 05-05-2025 
 */

/**
 * DedconRCON Framework - Server Implementation
 * A simple Node.js server that provides an API for RCON communication with Dedcon servers
 * 
 * This server implements three endpoints:
 * - /rcon/connect - Connect to an RCON server
 * - /rcon/execute - Execute a command on a connected RCON server
 * - /rcon/disconnect - Disconnect from an RCON server
 */

// Required packages
const express = require('express');
const dgram = require('dgram');
const path = require('path');
const cors = require('cors');
const crypto = require('crypto');

// Create Express app
const app = express();
const PORT = process.env.PORT || 3000;

// RCON Encryption Constants
const RCON_MAGIC = 0x52434F4E; // "RCON" in hex
const NONCE_SIZE = 12;
const TAG_SIZE = 16;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// Store active RCON sessions
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
            logBuffer.delete(key);
            console.log(`Session ${key} cleaned up due to inactivity`);
        }
    }
}, CLEANUP_INTERVAL);

// Generate encryption key from password using SHA-256
function generateKey(password) {
    return crypto.createHash('sha256').update(password).digest();
}

// Check if a packet is encrypted by looking for the RCON magic number
function isEncryptedPacket(data) {
    if (data.length < 4) return false;
    
    const magic = data.readUInt32LE(0);
    return magic === RCON_MAGIC;
}

// Encrypt a message using AES-256-GCM
function encryptRconPacket(plaintext, key) {
    // Generate a random nonce
    const nonce = crypto.randomBytes(NONCE_SIZE);
    
    // Create cipher
    const cipher = crypto.createCipheriv('aes-256-gcm', key, nonce);
    
    // Encrypt the data
    const encrypted = Buffer.concat([
        cipher.update(plaintext, 'utf8'),
        cipher.final()
    ]);
    
    // Get the authentication tag
    const tag = cipher.getAuthTag();
    
    // Create the complete packet: magic + nonce + tag + ciphertext
    const magicBuffer = Buffer.alloc(4);
    magicBuffer.writeUInt32LE(RCON_MAGIC, 0);
    
    return Buffer.concat([
        magicBuffer,     // 4 bytes
        nonce,           // 12 bytes
        tag,             // 16 bytes
        encrypted        // Variable length
    ]);
}

// Decrypt an encrypted message
function decryptRconPacket(data, key) {
    if (!isEncryptedPacket(data)) {
        // Not an encrypted packet, return as plain text
        return data.toString('utf8');
    }
    
    try {
        // Extract packet components
        const nonce = data.slice(4, 4 + NONCE_SIZE);
        const tag = data.slice(4 + NONCE_SIZE, 4 + NONCE_SIZE + TAG_SIZE);
        const ciphertext = data.slice(4 + NONCE_SIZE + TAG_SIZE);
        
        // Create decipher
        const decipher = crypto.createDecipheriv('aes-256-gcm', key, nonce);
        decipher.setAuthTag(tag);
        
        // Decrypt the data
        const decrypted = Buffer.concat([
            decipher.update(ciphertext),
            decipher.final()
        ]);
        
        return decrypted.toString('utf8');
    } catch (error) {
        console.error('Error decrypting packet:', error);
        return null;
    }
}

// Store log messages for a session
function storeLogMessage(sessionId, message) {
    if (!logBuffer.has(sessionId)) {
        logBuffer.set(sessionId, []);
    }
    
    const buffer = logBuffer.get(sessionId);
    buffer.push(message);
    
    // Keep buffer from growing too large
    if (buffer.length > 100) {
        buffer.splice(0, buffer.length - 100);
    }
}

// Set up a log listener for a session
function setupLogListener(sessionId, socket, key) {
    socket.on('message', (message) => {
        let response;
        
        try {
            // Try to decrypt if it's an encrypted packet
            if (isEncryptedPacket(message)) {
                response = decryptRconPacket(message, key);
            } else {
                // Fallback to plain text
                response = message.toString('utf8');
            }
            
            // Handle null response (decryption failed)
            if (response === null) {
                console.error('Failed to decrypt message');
                return;
            }
            
            // Store log messages
            if (response.startsWith('LOG ')) {
                storeLogMessage(sessionId, response);
            }
        } catch (error) {
            console.error('Error processing message:', error);
        }
    });
}

/**
 * Connect to RCON server
 * 
 * Request body:
 * - server: RCON server address (IP or hostname)
 * - port: RCON server port
 * - password: RCON server password
 * 
 * Response:
 * - success: Whether the connection was successful
 * - message: Status message
 * - sessionId: ID to use for subsequent commands (if successful)
 */
app.post('/rcon/connect', async (req, res) => {
    try {
        const { server, port, password } = req.body;

        if (!server || !port || !password) {
            return res.status(400).json({
                success: false,
                message: 'Server address, port, and password are required'
            });
        }

        // Generate a unique session ID
        const sessionId = `${Date.now()}-${Math.random().toString(36).substring(2, 15)}`;

        // Generate encryption key from password
        const encryptionKey = generateKey(password);

        // Create a new UDP socket
        const socket = dgram.createSocket('udp4');

        // Create a promise to handle the authentication response
        const authPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                socket.removeAllListeners('message');
                reject(new Error('Connection timeout'));
            }, 5000);

            socket.on('message', (message) => {
                let response;
                
                // Try to decrypt the message if it's encrypted
                if (isEncryptedPacket(message)) {
                    response = decryptRconPacket(message, encryptionKey);
                    if (response === null) {
                        return; // Decryption failed, ignore this message
                    }
                } else {
                    // Fallback to plain text
                    response = message.toString('utf8');
                }
                
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
            });

            socket.on('error', (err) => {
                clearTimeout(timeout);
                reject(err);
            });
        });

        // Send authentication packet (encrypted)
        const authCommand = `AUTH ${password}`;
        const encryptedAuthCommand = encryptRconPacket(authCommand, encryptionKey);
        socket.send(encryptedAuthCommand, 0, encryptedAuthCommand.length, port, server);

        // Wait for authentication response
        const authResponse = await authPromise;

        // Extract message from the response
        const responseMessage = authResponse.includes('MESSAGE')
            ? authResponse.split('MESSAGE ')[1].split('\n')[0]
            : 'Connected to RCON server';

        // Store the session
        activeSessions.set(sessionId, {
            socket: socket,
            server: server,
            port: port,
            encryptionKey: encryptionKey,
            lastActivity: Date.now(),
            authenticated: true
        });

        // Set up log listener
        setupLogListener(sessionId, socket, encryptionKey);

        console.log(`Connected to RCON server ${server}:${port}`);

        res.json({
            success: true,
            message: responseMessage,
            sessionId: sessionId
        });

    } catch (error) {
        console.error('RCON connection error:', error.message);
        res.status(400).json({
            success: false,
            message: `Failed to connect: ${error.message}`
        });
    }
});

/**
 * Execute RCON command
 * 
 * Request body:
 * - sessionId: Session ID from the connect endpoint
 * - command: Command to execute
 * 
 * Response:
 * - success: Whether the command was executed successfully
 * - response: Server response (if successful)
 * - message: Error message (if failed)
 * - needsReauth: Whether the client needs to reconnect (if session expired)
 */
app.post('/rcon/execute', async (req, res) => {
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
                message: 'Session not found or expired',
                needsReauth: true
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
                let response;
                
                // Try to decrypt the message if it's encrypted
                if (isEncryptedPacket(message)) {
                    response = decryptRconPacket(message, session.encryptionKey);
                    if (response === null) {
                        return; // Decryption failed, ignore this message
                    }
                } else {
                    // Fallback to plain text
                    response = message.toString('utf8');
                }
                
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

        console.log(`Executing command on ${session.server}:${session.port}: ${command}`);

        // Encrypt and send the command
        const encryptedCommand = encryptRconPacket(rconCommand, session.encryptionKey);
        session.socket.send(encryptedCommand, 0, encryptedCommand.length, session.port, session.server);

        // Wait for command response
        const commandResponse = await commandPromise;

        // Check if we need to re-authenticate
        if (commandResponse.includes('STATUS 401') && commandResponse.includes('Authentication required')) {
            session.authenticated = false;
            return res.status(401).json({
                success: false,
                message: 'Authentication required',
                needsReauth: true
            });
        }

        res.json({
            success: true,
            response: commandResponse
        });

    } catch (error) {
        console.error('RCON command error:', error.message);
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

/**
 * Get log messages for a session
 * 
 * Response:
 * - success: Always true if session exists
 * - logs: Array of log messages
 */
app.get('/rcon/logs/:sessionId', (req, res) => {
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

/**
 * Disconnect from RCON server
 * 
 * Request body:
 * - sessionId: Session ID from the connect endpoint
 * 
 * Response:
 * - success: Whether the disconnection was successful
 * - message: Status message
 */
app.post('/rcon/disconnect', (req, res) => {
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
        logBuffer.delete(sessionId);
        console.log(`Disconnected from RCON server ${session.server}:${session.port}`);
    }

    res.json({
        success: true,
        message: 'Disconnected from RCON server'
    });
});

// Start the server
app.listen(PORT, () => {
    console.log(`DedconRCON webserver running on port ${PORT}`);
    console.log(`Open http://localhost:${PORT} in your browser to use the RCON console`);
});