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
//Last Edited 05-05-2025

const express = require('express');
const router = express.Router();
const dgram = require('dgram');
const crypto = require('crypto');
const { authenticateToken, checkRole } = require('../../authentication');
const activeSessions = new Map();
const logBuffer = new Map();
const RCON_MAGIC = 0x52434F4E; 
const NONCE_SIZE = 12;
const TAG_SIZE = 16;
const DEFAULT_RCON_PASSWORD = "3498ry3uh9873y4t89734hgtvu9gh3987yt3ghbeuirgy3948th3irufh3r98th3985gh39458g";

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

function generateKey(password) {
    return crypto.createHash('sha256').update(password).digest();
}

function isEncryptedPacket(data) {
    if (data.length < 4) return false;
    
    const magic = data.readUInt32LE(0);
    return magic === RCON_MAGIC;
}

function encryptRconPacket(plaintext, key) {

    const nonce = crypto.randomBytes(NONCE_SIZE);
    const cipher = crypto.createCipheriv('aes-256-gcm', key, nonce);
    const encrypted = Buffer.concat([
        cipher.update(plaintext, 'utf8'),
        cipher.final()
    ]);
    
    const tag = cipher.getAuthTag();
    const magicBuffer = Buffer.alloc(4);
    magicBuffer.writeUInt32LE(RCON_MAGIC, 0);
    
    return Buffer.concat([
        magicBuffer,     
        nonce,           
        tag,             
        encrypted        
    ]);
}

function decryptRconPacket(data, key) {
    if (!isEncryptedPacket(data)) {
        return data.toString('utf8');
    }
    
    try {
        const nonce = data.slice(4, 4 + NONCE_SIZE);
        const tag = data.slice(4 + NONCE_SIZE, 4 + NONCE_SIZE + TAG_SIZE);
        const ciphertext = data.slice(4 + NONCE_SIZE + TAG_SIZE);
        const decipher = crypto.createDecipheriv('aes-256-gcm', key, nonce);
        decipher.setAuthTag(tag);
        
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

function storeLogMessage(sessionId, message) {
    if (!logBuffer.has(sessionId)) {
        logBuffer.set(sessionId, []);
    }
    logBuffer.get(sessionId).push(message);
    
    const buffer = logBuffer.get(sessionId);
    if (buffer.length > 100) {
        buffer.splice(0, buffer.length - 100);
    }
}

function setupLogListener(sessionId, socket, serverAddr, key) {
    socket.on('message', (message) => {
        let response;
        try {
            if (isEncryptedPacket(message)) {
                response = decryptRconPacket(message, key);
            } else {
                response = message.toString('utf8');
            }
            
            if (response === null) {
                console.error('Failed to decrypt message');
                return;
            }
            
            if (response.startsWith('LOG ')) {
                storeLogMessage(sessionId, response);
            }
        } catch (error) {
            console.error('Error processing message:', error);
        }
    });
}

router.post('/apis/admin/rcon/connect', authenticateToken, checkRole(1), async (req, res) => {
    try {
        const { server, port, password } = req.body;
        
        if (!server || !port) {
            return res.status(400).json({ 
                success: false, 
                message: 'Server address and port are required' 
            });
        }

        // Use default password for localhost connections if password is empty
        let connectionPassword = password;
        if (!connectionPassword && server === 'localhost') {
            connectionPassword = DEFAULT_RCON_PASSWORD;
        } else if (!connectionPassword) {
            return res.status(400).json({ 
                success: false, 
                message: 'Password is required' 
            });
        }

        const sessionId = `${req.user.id}-${server}-${port}`;
        
        if (activeSessions.has(sessionId)) {
            const existingSession = activeSessions.get(sessionId);
            if (existingSession.socket) {
                existingSession.socket.close();
            }
            activeSessions.delete(sessionId);
        }
        
        const encryptionKey = generateKey(connectionPassword);
        const socket = dgram.createSocket('udp4');
        const authPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                socket.removeAllListeners('message');
                reject(new Error('Connection timeout'));
            }, 5000);
            
            socket.on('message', (message) => {
                let response;
                
                if (isEncryptedPacket(message)) {
                    response = decryptRconPacket(message, encryptionKey);
                    if (response === null) {
                        return; 
                    }
                } else {

                    response = message.toString('utf8');
                }
                
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
        
        const authCommand = `AUTH ${connectionPassword}`;
        const encryptedAuthCommand = encryptRconPacket(authCommand, encryptionKey);
        socket.send(encryptedAuthCommand, 0, encryptedAuthCommand.length, port, server);
        
        const authResponse = await authPromise;
        
        activeSessions.set(sessionId, {
            socket: socket,
            server: server,
            port: port,
            encryptionKey: encryptionKey,
            lastActivity: Date.now(),
            authenticated: true
        });
        
        setupLogListener(sessionId, socket, { address: server, port: port }, encryptionKey);
        
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
    
    logBuffer.set(sessionId, []);
    
    res.json({ 
        success: true, 
        logs: logs 
    });
});

router.post('/apis/admin/rcon/execute', authenticateToken, checkRole(1), async (req, res) => {
    try {
        const { sessionId, command } = req.body;
        
        if (!sessionId || !command) {
            return res.status(400).json({ 
                success: false, 
                message: 'Session ID and command are required' 
            });
        }
        
        if (!activeSessions.has(sessionId)) {
            return res.status(404).json({ 
                success: false, 
                message: 'Session not found or expired' 
            });
        }
        
        const session = activeSessions.get(sessionId);

        session.lastActivity = Date.now();

        const commandPromise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Command timeout'));
            }, 10000);
            
            const messageHandler = (message) => {
                let response;
                
                if (isEncryptedPacket(message)) {
                    response = decryptRconPacket(message, session.encryptionKey);
                    if (response === null) {
                        return; 
                    }
                } else {
                    response = message.toString('utf8');
                }
                
                if (response.startsWith('LOG ')) {
                    storeLogMessage(sessionId, response);
                    return;
                }
                
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
        
        let rconCommand = command;
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
        
        const encryptedCommand = encryptRconPacket(rconCommand, session.encryptionKey);
        session.socket.send(encryptedCommand, 0, encryptedCommand.length, session.port, session.server);
        
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