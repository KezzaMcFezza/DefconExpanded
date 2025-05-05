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

document.addEventListener('DOMContentLoaded', () => {
    // RCON UI Elements
    const serverInput = document.getElementById('server');
    const portInput = document.getElementById('port');
    const passwordInput = document.getElementById('password');
    const connectBtn = document.getElementById('connect-btn');
    const disconnectBtn = document.getElementById('disconnect-btn');
    const connectionStatus = document.getElementById('connection-status');
    const consoleOutput = document.getElementById('console-output');
    const commandInput = document.getElementById('command');
    const sendBtn = document.getElementById('send-btn');
    
    // Get the log stream elements or create dummy elements if they don't exist
    const streamGameEvents = document.getElementById('stream-game-events') || { checked: false, disabled: false };
    const streamServerLog = document.getElementById('stream-server-log') || { checked: false, disabled: false };

    // Session state
    let currentSessionId = null;
    let commandHistory = [];
    let historyIndex = -1;
    let isGameEventsActive = false;
    let isServerLogActive = false;

    // API Endpoints
    const API_CONNECT = '/rcon/connect';
    const API_EXECUTE = '/rcon/execute';
    const API_DISCONNECT = '/rcon/disconnect';
    const API_LOGS = '/rcon/logs';

    const loadSavedServerInfo = () => {
        const savedServer = localStorage.getItem('rcon_server');
        const savedPort = localStorage.getItem('rcon_port');
        
        if (savedServer) {
            serverInput.value = savedServer;
        }
        
        if (savedPort) {
            portInput.value = savedPort;
        } else {
            // Default RCON port
            portInput.value = '8800';
        }
    };

    const saveServerInfo = (server, port) => {
        localStorage.setItem('rcon_server', server);
        localStorage.setItem('rcon_port', port);
    };

    function addConsoleMessage(message, className = '') {
        const messageElement = document.createElement('div');
        messageElement.className = className;
        messageElement.textContent = message;
        consoleOutput.appendChild(messageElement);
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }

    function formatResponse(response) {
        const lines = response.split('\n');
        let formattedResponse = '';
        
        for (const line of lines) {
            // Handle log stream responses
            if (line.startsWith('LOG GAMEEVENT ')) {
                addConsoleMessage(line.substring(13), 'log-event');
                continue;
            } else if (line.startsWith('LOG SERVERLOG ')) {
                addConsoleMessage(line.substring(13), 'log-server');
                continue;
            }
            
            if (line.startsWith('STATUS ')) {
                continue; // Skip status lines
            } else if (line.startsWith('MESSAGE ')) {
                formattedResponse += line.substring(8) + '\n';
            } else {
                formattedResponse += line + '\n';
            }
        }
        
        return formattedResponse.trim();
    }

    function startLogPolling() {
        if (!currentSessionId) return;
        
        // Clear any existing polling
        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
        }
        
        const pollInterval = 1000; // Poll every second
        
        window.logPollingInterval = setInterval(async () => {
            if (!currentSessionId) {
                clearInterval(window.logPollingInterval);
                window.logPollingInterval = null;
                return;
            }
            
            try {
                const response = await fetch(`${API_LOGS}/${currentSessionId}`);
                const data = await response.json();
                
                if (data.success && data.logs && data.logs.length > 0) {
                    // Process each log message
                    data.logs.forEach(logMsg => {
                        if (logMsg.startsWith('LOG GAMEEVENT ')) {
                            addConsoleMessage(logMsg.substring(13), 'log-event');
                        } else if (logMsg.startsWith('LOG SERVERLOG ')) {
                            addConsoleMessage(logMsg.substring(13), 'log-server');
                        }
                    });
                }
            } catch (error) {
                console.error('Log polling error:', error);
            }
        }, pollInterval);
    }

    async function connectToServer() {
        // Disable inputs during connection attempt
        serverInput.disabled = true;
        portInput.disabled = true;
        passwordInput.disabled = true;
        connectBtn.disabled = true;
        streamGameEvents.disabled = true;
        streamServerLog.disabled = true;
        
        const server = serverInput.value.trim();
        const port = parseInt(portInput.value);
        const password = passwordInput.value;
        const enableGameEvents = streamGameEvents.checked;
        const enableServerLog = streamServerLog.checked;

        if (!server || !port) {
            addConsoleMessage('Error: Server address and port are required', 'error-message');
            
            // Re-enable inputs
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            connectBtn.disabled = false;
            streamGameEvents.disabled = false;
            streamServerLog.disabled = false;
            return;
        }

        // Clear console output
        consoleOutput.innerHTML = '';
        addConsoleMessage(`Connecting to ${server}:${port}...`, 'system-message');
        
        try {
            const response = await fetch(API_CONNECT, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    server,
                    port,
                    password
                })
            });

            const data = await response.json();

            if (data.success) {
                // Update UI for connected state
                connectionStatus.textContent = 'Connected';
                connectionStatus.className = 'status-connected';
                currentSessionId = data.sessionId;
                
                // Enable command input
                commandInput.disabled = false;
                sendBtn.disabled = false;
                passwordInput.value = '';
                
                // Keep server and port disabled, but enable disconnect
                disconnectBtn.disabled = false;
                
                addConsoleMessage(data.message, 'success-message');
                addConsoleMessage('Type "help" for available commands', 'system-message');
                
                // Set focus to command input
                commandInput.focus();

                // Save server and port to localStorage
                saveServerInfo(server, port);
                
                // Set state for log streams
                isGameEventsActive = enableGameEvents;
                isServerLogActive = enableServerLog;
                
                // Start log streams if requested
                if (enableGameEvents || enableServerLog) {
                    // Start polling immediately after connection
                    startLogPolling();
                    
                    if (enableGameEvents) {
                        addConsoleMessage('Enabling game events stream...', 'system-message');
                        // Send the command to start game events
                        const streamResponse = await fetch(API_EXECUTE, {
                            method: 'POST',
                            headers: {
                                'Content-Type': 'application/json'
                            },
                            body: JSON.stringify({
                                sessionId: currentSessionId,
                                command: 'STARTGAMELOG'
                            })
                        });
                        
                        const streamData = await streamResponse.json();
                        if (streamData.success) {
                            addConsoleMessage('Game event stream enabled', 'success-message');
                        } else {
                            addConsoleMessage(`Failed to enable game events: ${streamData.message}`, 'error-message');
                        }
                    }
                    
                    if (enableServerLog) {
                        addConsoleMessage('Enabling server log stream...', 'system-message');
                        // Send the command to start server log
                        const streamResponse = await fetch(API_EXECUTE, {
                            method: 'POST',
                            headers: {
                                'Content-Type': 'application/json'
                            },
                            body: JSON.stringify({
                                sessionId: currentSessionId,
                                command: 'STARTSERVERLOG'
                            })
                        });
                        
                        const streamData = await streamResponse.json();
                        if (streamData.success) {
                            addConsoleMessage('Server log stream enabled', 'success-message');
                        } else {
                            addConsoleMessage(`Failed to enable server log: ${streamData.message}`, 'error-message');
                        }
                    }
                }
            } else {
                // Show error message and re-enable inputs
                addConsoleMessage(`Error: ${data.message}`, 'error-message');
                
                serverInput.disabled = false;
                portInput.disabled = false;
                passwordInput.disabled = false;
                connectBtn.disabled = false;
                streamGameEvents.disabled = false;
                streamServerLog.disabled = false;
            }
        } catch (error) {
            addConsoleMessage(`Connection error: ${error.message}`, 'error-message');
            
            // Re-enable inputs
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            connectBtn.disabled = false;
            streamGameEvents.disabled = false;
            streamServerLog.disabled = false;
        }
    }

    async function startGameEventStream() {
        if (!currentSessionId || isGameEventsActive) {
            return;
        }
        
        try {
            const response = await fetch(API_EXECUTE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId,
                    command: 'STARTGAMELOG'
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                isGameEventsActive = true;
                addConsoleMessage('Game event log stream started', 'success-message');
                
                // Start polling if not already running
                if (!window.logPollingInterval) {
                    startLogPolling();
                }
            } else {
                addConsoleMessage(`Failed to start game event stream: ${data.message}`, 'error-message');
            }
        } catch (error) {
            addConsoleMessage(`Game event stream error: ${error.message}`, 'error-message');
        }
    }
    
    async function startServerLogStream() {
        if (!currentSessionId || isServerLogActive) {
            return;
        }
        
        try {
            const response = await fetch(API_EXECUTE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId,
                    command: 'STARTSERVERLOG'
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                isServerLogActive = true;
                addConsoleMessage('Server log stream started', 'success-message');
                
                // Start polling if not already running
                if (!window.logPollingInterval) {
                    startLogPolling();
                }
            } else {
                addConsoleMessage(`Failed to start server log stream: ${data.message}`, 'error-message');
            }
        } catch (error) {
            addConsoleMessage(`Server log stream error: ${error.message}`, 'error-message');
        }
    }
    
    async function stopGameEventStream() {
        if (!currentSessionId || !isGameEventsActive) {
            return;
        }
        
        try {
            const response = await fetch(API_EXECUTE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId,
                    command: 'STOPGAMELOG'
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                isGameEventsActive = false;
                addConsoleMessage('Game event log stream stopped', 'success-message');
            } else {
                addConsoleMessage(`Failed to stop game event stream: ${data.message}`, 'error-message');
            }
        } catch (error) {
            addConsoleMessage(`Game event stream error: ${error.message}`, 'error-message');
        }
    }
    
    async function stopServerLogStream() {
        if (!currentSessionId || !isServerLogActive) {
            return;
        }
        
        try {
            const response = await fetch(API_EXECUTE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId,
                    command: 'STOPSERVERLOG'
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                isServerLogActive = false;
                addConsoleMessage('Server log stream stopped', 'success-message');
            } else {
                addConsoleMessage(`Failed to stop server log stream: ${data.message}`, 'error-message');
            }
        } catch (error) {
            addConsoleMessage(`Server log stream error: ${error.message}`, 'error-message');
        }
    }

    async function disconnectFromServer() {
        if (!currentSessionId) {
            return;
        }
        
        // Stop polling
        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
            window.logPollingInterval = null;
        }
        
        // Stop any active log streams
        try {
            if (isGameEventsActive) {
                await stopGameEventStream();
            }
            
            if (isServerLogActive) {
                await stopServerLogStream();
            }
        } catch (streamError) {
            console.error('Error stopping streams:', streamError);
            addConsoleMessage(`Warning: Error stopping streams: ${streamError.message}`, 'error-message');
        }

        try {
            const response = await fetch(API_DISCONNECT, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId
                })
            });

            const data = await response.json();
            
            // Update UI for disconnected state
            connectionStatus.textContent = 'Disconnected';
            connectionStatus.className = 'status-disconnected';
            
            // Disable command input
            commandInput.disabled = true;
            sendBtn.disabled = true;
            
            // Enable connection inputs
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            connectBtn.disabled = false;
            disconnectBtn.disabled = true;
            streamGameEvents.disabled = false;
            streamServerLog.disabled = false;
            
            currentSessionId = null;
            isGameEventsActive = false;
            isServerLogActive = false;
            
            addConsoleMessage(data.message, 'system-message');
        } catch (error) {
            addConsoleMessage(`Disconnect error: ${error.message}`, 'error-message');
        }
    }

    async function sendCommand() {
        if (!currentSessionId) {
            addConsoleMessage('Error: Not connected to any server', 'error-message');
            return;
        }

        const command = commandInput.value.trim();
        if (!command) {
            return;
        }

        // Handle log stream commands
        if (command === 'logstream-gameevents') {
            commandInput.value = '';
            addConsoleMessage('> logstream-gameevents', 'console-prompt');
            await startGameEventStream();
            return;
        } else if (command === 'logstream-serverlog') {
            commandInput.value = '';
            addConsoleMessage('> logstream-serverlog', 'console-prompt');
            await startServerLogStream();
            return;
        } else if (command === '!logstream-gameevents') {
            commandInput.value = '';
            addConsoleMessage('> !logstream-gameevents', 'console-prompt');
            await stopGameEventStream();
            return;
        } else if (command === '!logstream-serverlog') {
            commandInput.value = '';
            addConsoleMessage('> !logstream-serverlog', 'console-prompt');
            await stopServerLogStream();
            return;
        } else if (command === 'logstream-all') {
            commandInput.value = '';
            addConsoleMessage('> logstream-all', 'console-prompt');
            await startGameEventStream();
            await startServerLogStream();
            return;
        } else if (command === '!logstream-all') {
            commandInput.value = '';
            addConsoleMessage('> !logstream-all', 'console-prompt');
            await stopGameEventStream();
            await stopServerLogStream();
            return;
        } else if (command === 'help') {
            commandInput.value = '';
            addConsoleMessage('> help', 'console-prompt');
            showHelpText();
            return;
        }

        // Add command to history
        commandHistory.push(command);
        historyIndex = commandHistory.length;
        
        // Display command in console
        addConsoleMessage(`> ${command}`, 'console-prompt');
        
        // Clear input
        commandInput.value = '';
        
        try {
            const response = await fetch(API_EXECUTE, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    sessionId: currentSessionId,
                    command
                })
            });

            const data = await response.json();

            if (data.success) {
                // Display formatted response
                const formattedResponse = formatResponse(data.response);
                addConsoleMessage(formattedResponse, 'console-response');
            } else {
                // Check if we need to reconnect
                if (data.needsReauth) {
                    addConsoleMessage('Session expired. Please reconnect.', 'error-message');
                    await disconnectFromServer();
                } else {
                    addConsoleMessage(`Error: ${data.message}`, 'error-message');
                }
            }
        } catch (error) {
            addConsoleMessage(`Command error: ${error.message}`, 'error-message');
        }
    }

    function handleCommandHistory(direction) {
        if (commandHistory.length === 0) {
            return;
        }

        if (direction === 'up') {
            historyIndex = Math.max(0, historyIndex - 1);
        } else {
            historyIndex = Math.min(commandHistory.length, historyIndex + 1);
        }

        if (historyIndex === commandHistory.length) {
            commandInput.value = '';
        } else {
            commandInput.value = commandHistory[historyIndex];
        }
    }
    
    function showHelpText() {
        const commonCommands = [
            { cmd: 'ServerName <name>', desc: 'Change the server name' },
            { cmd: 'MaxTeams <number>', desc: 'Set max player slots' },
            { cmd: 'MinTeams <number>', desc: 'Set min players to start' },
            { cmd: 'ServerPassword <password>', desc: 'Set server password' },
            { cmd: 'quit', desc: 'Restart the server, will require a reconnect' },
            { cmd: 'logstream-gameevents', desc: 'Start game event logging' },
            { cmd: 'logstream-serverlog', desc: 'Start server logging' },
            { cmd: 'logstream-all', desc: 'Start all log streams' },
            { cmd: '!logstream-gameevents', desc: 'Stop game event logging' },
            { cmd: '!logstream-serverlog', desc: 'Stop server logging' },
            { cmd: '!logstream-all', desc: 'Stop all log streams' },
            { cmd: 'help', desc: 'Show this help message' }
        ];
        
        addConsoleMessage('Common Dedcon Commands:', 'system-message');
        
        let helpText = '';
        commonCommands.forEach(cmd => {
            helpText += `${cmd.cmd.padEnd(25)} - ${cmd.desc}\n`;
        });
        
        addConsoleMessage(helpText, 'console-response');
    }

    // Event Listeners
    connectBtn.addEventListener('click', connectToServer);
    disconnectBtn.addEventListener('click', disconnectFromServer);
    
    // Send command when button clicked
    sendBtn.addEventListener('click', sendCommand);
    
    // Send command when Enter key is pressed
    commandInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            sendCommand();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            handleCommandHistory('up');
        } else if (e.key === 'ArrowDown') {
            e.preventDefault();
            handleCommandHistory('down');
        }
    });
    
    // Initialize
    loadSavedServerInfo();
});