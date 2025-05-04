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
 * Last Edited 03-05-2025 
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

// Create Express app
const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// Store active RCON sessions
const activeSessions = new Map();

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
            console.log(`Session ${key} cleaned up due to inactivity`);
        }
    }
}, CLEANUP_INTERVAL);

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

        // Create a new UDP socket
        const socket = dgram.createSocket('udp4');

        // Create a promise to handle the authentication response
        const authPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                socket.removeAllListeners('message');
                reject(new Error('Connection timeout'));
            }, 5000);

            socket.on('message', (message) => {
                clearTimeout(timeout);
                const response = message.toString('utf8');
                if (response.includes('STATUS 200')) {
                    resolve(response);
                } else {
                    reject(new Error(response));
                }
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

        // Extract message from the response
        const responseMessage = authResponse.includes('MESSAGE')
            ? authResponse.split('MESSAGE ')[1].split('\n')[0]
            : 'Connected to RCON server';

        // Store the session
        activeSessions.set(sessionId, {
            socket: socket,
            server: server,
            port: port,
            lastActivity: Date.now(),
            authenticated: true
        });

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
                session.socket.removeAllListeners('message');
                reject(new Error('Command timeout'));
            }, 10000);

            session.socket.once('message', (message) => {
                clearTimeout(timeout);
                resolve(message.toString('utf8'));
            });

            session.socket.once('error', (err) => {
                clearTimeout(timeout);
                reject(err);
            });
        });

        // Send command packet
        const execCommand = `EXEC ${command}`;
        session.socket.send(execCommand, 0, execCommand.length, session.port, session.server);

        console.log(`Executing command on ${session.server}:${session.port}: ${command}`);

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
        res.status(400).json({
            success: false,
            message: `Command failed: ${error.message}`
        });
    }
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
        console.log(`Disconnected from RCON server ${session.server}:${session.port}`);
    }

    res.json({
        success: true,
        message: 'Disconnected from RCON server'
    });
});

// Serve index.html for the root route
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Start the server
app.listen(PORT, () => {
    console.log(`DedconRCON Server running on http://localhost:${PORT}`);
    console.log('Press Ctrl+C to stop the server');
});