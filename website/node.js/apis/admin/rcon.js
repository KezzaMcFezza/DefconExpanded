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
const dgram = require('dgram');
const crypto = require('crypto');
const permissions = require('../../permission-index');
const activeSessions = new Map();
const logBuffer = new Map();
const RCON_MAGIC = 0x52434F4E; 
const NONCE_SIZE = 12;
const TAG_SIZE = 16;
const DEFAULT_RCON_PASSWORD = "3498ry3uh9873y4t89734hgtvu9gh3987yt3ghbeuirgy3948th3irufh3r98th3985gh39458g";

const { 
    authenticateToken, 
    checkPermission 
} = require('../../authentication');

const serverConfigurations = [
    { name: 'Manual Connection', port: null, manual: true, permission: permissions.RCON_MANUAL_CONNECTION },
    { name: 'DefconExpanded Test Server', port: 8800, permission: permissions.RCON_TEST_SERVER },
    { name: 'DefconExpanded | 1v1 | Totally Random', port: 8801, permission: permissions.RCON_1V1_RANDOM },
    { name: 'DefconExpanded | 1v1 | Default', port: 8802, permission: permissions.RCON_1V1_DEFAULT },
    { name: 'DefconExpanded | 1v1 | Best Setups Only!', port: 8803, permission: permissions.RCON_1V1_BEST_SETUPS_1 },
    { name: 'DefconExpanded | 1V1 | Best Setups Only!', port: 8804, permission: permissions.RCON_1V1_BEST_SETUPS_2 },
    { name: 'DefconExpanded | 1v1 | Cursed Setups Only!', port: 8805, permission: permissions.RCON_1V1_CURSED },
    { name: 'DefconExpanded | 1v1 | Lots of Units!', port: 8806, permission: permissions.RCON_1V1_LOTS_UNITS },
    { name: 'DefconExpanded | 1v1 | UK and Ireland', port: 8807, permission: permissions.RCON_1V1_UK_IRELAND },
    { name: 'Muricon | UK Mod', port: 8808, permission: permissions.RCON_MURICON_UK },
    { name: 'DefconExpanded | 2v2 | UK and Ireland', port: 8809, permission: permissions.RCON_2V2_UK_IRELAND },
    { name: 'DefconExpanded | 2v2 | Totally Random', port: 8810, permission: permissions.RCON_2V2_RANDOM },
    { name: 'DefconExpanded | Diplomacy | UK and Ireland', port: 8811, permission: permissions.RCON_DIPLOMACY_UK },
    { name: 'Raizer\'s Russia vs USA | Totally Random', port: 8812, permission: permissions.RCON_RAIZER_RUSSIA_USA },
    { name: 'New Player Server', port: 8813, permission: permissions.RCON_NEW_PLAYER },
    { name: '2v2 Tournament', port: 8814, permission: permissions.RCON_2V2_TOURNAMENT },
    { name: 'Sony and Hoov\'s Hideout', port: 8815, permission: permissions.RCON_SONY_HOOV },
    { name: 'DefconExpanded | 3v3 | Totally Random', port: 8816, permission: permissions.RCON_3V3_RANDOM },
    { name: 'MURICON | 1v1 Default | 2.8.15', port: 8817, permission: permissions.RCON_MURICON_DEFAULT },
    { name: 'MURICON | 1V1 | Totally Random | 2.8.15', port: 8818, permission: permissions.RCON_MURICON_RANDOM },
    { name: '509 CG | 1v1 | Totally Random | 2.8.15', port: 8819, permission: permissions.RCON_509CG_1V1 },
    { name: 'DefconExpanded | Free For All | Random Cities', port: 8820, permission: permissions.RCON_FFA_RANDOM },
    { name: 'DefconExpanded | 8 Player | Diplomacy', port: 8821, permission: permissions.RCON_8PLAYER_DIPLOMACY },
    { name: 'DefconExpanded | 4V4 | Totally Random', port: 8822, permission: permissions.RCON_4V4_RANDOM },
    { name: 'DefconExpanded | 10 Player | Diplomacy', port: 8823, permission: permissions.RCON_10PLAYER_DIPLOMACY }
];

function hasServerPermission(userPermissions, serverPort) {
    if (!serverPort) {
        return userPermissions.includes(permissions.RCON_MANUAL_CONNECTION);
    }

    const serverConfig = serverConfigurations.find(config => config.port === parseInt(serverPort));
    if (!serverConfig) {
        return false;
    }

    return userPermissions.includes(serverConfig.permission);
}

const checkServerPermission = (req, res, next) => {
    if (!req.user || !req.user.permissions) {
        return res.status(401).json({ error: 'Not authenticated' });
    }
    
    const { port } = req.body;
    
    if (!req.user.permissions.includes(permissions.PAGE_RCON_CONSOLE)) {
        return res.status(403).json({ error: 'Insufficient permissions to access RCON console' });
    }
    
    if (hasServerPermission(req.user.permissions, port)) {
        next();
    } else {
        return res.status(403).json({ error: 'Insufficient permissions for this server' });
    }
};

const CLEANUP_INTERVAL = 5 * 60 * 1000;
setInterval(() => {
    const now = Date.now();
    for (const [key, session] of activeSessions.entries()) {
        if (now - session.lastActivity > CLEANUP_INTERVAL) {
            if (session.socket) {
                session.socket.close();
            }
            activeSessions.delete(key);
            if (logBuffer.has(key)) {
                logBuffer.delete(key);
            }
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

router.post('/apis/admin/rcon/connect', authenticateToken, checkServerPermission, async (req, res) => {
    try {
        const { server, port, password } = req.body;
        
        if (!server || !port) {
            return res.status(400).json({ 
                success: false, 
                message: 'Server address and port are required' 
            });
        }

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
            if (logBuffer.has(sessionId)) {
                logBuffer.delete(sessionId);
            }
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

router.get('/apis/admin/rcon/logs/:sessionId', authenticateToken, checkPermission(permissions.PAGE_RCON_CONSOLE), (req, res) => {
    const { sessionId } = req.params;
    
    const [userId] = sessionId.split('-');
    if (userId !== req.user.id.toString()) {
        return res.status(403).json({ success: false, message: 'Unauthorized access to session logs' });
    }
    
    const logs = logBuffer.get(sessionId) || [];
    logBuffer.set(sessionId, []);
    res.json({ success: true, logs });
});

router.post('/apis/admin/rcon/execute', authenticateToken, checkPermission(permissions.PAGE_RCON_CONSOLE), async (req, res) => {
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

router.post('/apis/admin/rcon/disconnect', authenticateToken, checkPermission(permissions.PAGE_RCON_CONSOLE), (req, res) => {
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
    
    if (logBuffer.has(sessionId)) {
        logBuffer.delete(sessionId);
    }
    
    res.json({ 
        success: true, 
        message: 'Disconnected from RCON server' 
    });
});

module.exports = router;