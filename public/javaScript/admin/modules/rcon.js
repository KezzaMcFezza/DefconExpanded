// RCON Console Module for DefconExpanded
// This module handles RCON communication with Dedcon servers

document.addEventListener('DOMContentLoaded', () => {
    // RCON Elements
    const serverInput = document.getElementById('server');
    const portInput = document.getElementById('port');
    const passwordInput = document.getElementById('password');
    const connectBtn = document.getElementById('connect-btn');
    const disconnectBtn = document.getElementById('disconnect-btn');
    const connectionStatus = document.getElementById('connection-status');
    const consoleOutput = document.getElementById('console-output');
    const commandInput = document.getElementById('command');
    const sendBtn = document.getElementById('send-btn');
    const serverList = document.getElementById('server-list');
    const connectionForm = document.getElementById('connection-form');

    // Get the log stream elements or create dummy elements if they don't exist
    const streamGameEvents = document.getElementById('stream-game-events') || { checked: false, disabled: false };
    const streamServerLog = document.getElementById('stream-server-log') || { checked: false, disabled: false };

    let currentSessionId = null;
    let commandHistory = [];
    let historyIndex = -1;
    let isGameEventsActive = false;
    let isServerLogActive = false;
    let currentlyActiveServer = null;
    let isManualMode = false;

    // Default RCON password for all servers
    const DEFAULT_RCON_PASSWORD = "D3vilsAdvocat3";

    // Server configurations
    const serverConfigurations = [
        { name: 'Manual Connection', port: null, manual: true },
        { name: 'New Player Server', port: 8800 },
        { name: 'DefconExpanded Test Server', port: 8801 },
        { name: 'DefconExpanded | 1v1 | Totally Random', port: 8802 },
        { name: 'DefconExpanded | 1V1 | Best Setups Only!', port: 8803 },
        { name: 'DefconExpanded | 1v1 | Cursed Setups Only!', port: 8804 },
        { name: 'Raizer\'s Russia vs USA | Totally Random', port: 8805 },
        { name: 'DefconExpanded | 1v1 | Default', port: 8806 },
        { name: 'DefconExpanded | 2v2 | Totally Random', port: 8807 },
        { name: '2v2 Tournament', port: 8808 },
        { name: 'Sony and Hoov\'s Hideout', port: 8809 },
        { name: 'DefconExpanded | 3v3 | Totally Random', port: 8810 },
        { name: 'MURICON | 1v1 Default | 2.8.15', port: 8811 },
        { name: 'MURICON | 1V1 | Totally Random | 2.8.15', port: 8812 },
        { name: '509 CG | 1v1 | Totally Random | 2.8.15', port: 8813 },
        { name: 'DefconExpanded | Free For All | Random Cities', port: 8814 },
        { name: 'DefconExpanded | 8 Player | Diplomacy', port: 8815 },
        { name: 'DefconExpanded | 4V4 | Totally Random', port: 8816 },
        { name: 'DefconExpanded | 10 Player | Diplomacy', port: 8817 }
    ];

    // Initialize server list
    function initializeServerList() {
        serverList.innerHTML = '';
        
        serverConfigurations.forEach((config, index) => {
            const listItem = document.createElement('li');
            listItem.textContent = config.name;
            listItem.dataset.index = index;
            
            if (config.manual) {
                listItem.classList.add('manual-connection');
            }
            
            listItem.addEventListener('click', () => selectServer(index));
            serverList.appendChild(listItem);
        });
    }

    // Select a server from the list
    async function selectServer(index) {
        // If already connected to a server, disconnect first
        if (currentSessionId) {
            await disconnectFromServer();
        }

        const config = serverConfigurations[index];
        
        // Update UI to show which server is selected
        document.querySelectorAll('#server-list li').forEach(item => {
            item.classList.remove('active');
        });
        document.querySelector(`#server-list li[data-index="${index}"]`).classList.add('active');
        
        // Handle manual connection mode
        if (config.manual) {
            isManualMode = true;
            connectionForm.style.display = 'block';
            
            // Pre-fill with last used values or defaults
            const savedServer = localStorage.getItem('rcon_server') || 'localhost';
            const savedPort = localStorage.getItem('rcon_port') || '';
            const savedPassword = localStorage.getItem('rcon_password') || '';
            
            serverInput.value = savedServer;
            portInput.value = savedPort;
            passwordInput.value = savedPassword;
            
            // Enable the form fields
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            
            currentlyActiveServer = null;
            return;
        }
        
        // Handle predefined server
        isManualMode = false;
        connectionForm.style.display = 'none';
        
        // Fill in the form with the server details (these will be hidden but still used for connection)
        serverInput.value = 'localhost';
        portInput.value = config.port;
        passwordInput.value = DEFAULT_RCON_PASSWORD;
        currentlyActiveServer = index;
    }

    // Load saved server and port from localStorage if available
    const loadSavedServerInfo = () => {
        const savedServer = localStorage.getItem('rcon_server');
        const savedPort = localStorage.getItem('rcon_port');
        const savedPassword = localStorage.getItem('rcon_password');

        if (savedServer) {
            serverInput.value = savedServer;
        }

        if (savedPort) {
            portInput.value = savedPort;
        }

        if (savedPassword) {
            passwordInput.value = savedPassword;
        }
    };

    // Save server and port to localStorage
    const saveServerInfo = (server, port, password) => {
        localStorage.setItem('rcon_server', server);
        localStorage.setItem('rcon_port', port);
        if (password && isManualMode) {
            localStorage.setItem('rcon_password', password);
        }
    };

    // Load saved values on page load
    loadSavedServerInfo();
    initializeServerList();
    
    // By default select the Manual Connection option
    selectServer(0);

    // Function to add a message to the console output
    function addConsoleMessage(message, className = '') {
        const messageElement = document.createElement('div');
        messageElement.className = className;
        messageElement.textContent = message;
        consoleOutput.appendChild(messageElement);
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }

    // Clear console output
    function clearConsole() {
        consoleOutput.innerHTML = '';
    }

    // Function to parse and format RCON response
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

            // Handle standard responses
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

    // Connect to RCON server
    async function connectToServer() {
        // Don't try to connect if no server is selected
        if (!isManualMode && currentlyActiveServer === null) {
            addConsoleMessage('Please select a server configuration first', 'error-message');
            return;
        }
        
        // Validate form in manual mode
        if (isManualMode) {
            if (!serverInput.value || !portInput.value || !passwordInput.value) {
                addConsoleMessage('Error: Server address, port, and password are required', 'error-message');
                return;
            }
        }
        
        // Disable UI during connection
        connectBtn.disabled = true;
        streamGameEvents.disabled = true;
        streamServerLog.disabled = true;
        
        if (isManualMode) {
            serverInput.disabled = true;
            portInput.disabled = true;
            passwordInput.disabled = true;
        }

        const server = serverInput.value.trim();
        const port = parseInt(portInput.value);
        const password = passwordInput.value;
        const enableGameEvents = streamGameEvents.checked;
        const enableServerLog = streamServerLog.checked;

        try {
            clearConsole();
            addConsoleMessage(`Connecting to ${server}:${port}...`, 'system-message');

            const response = await fetch('/apis/admin/rcon/connect', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    server,
                    port,
                    password,
                    enableGameEvents,
                    enableServerLog
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
                
                // Don't clear password for predefined servers
                if (isManualMode) {
                    passwordInput.value = '';
                }

                addConsoleMessage(data.message, 'success-message');
                addConsoleMessage(`Connected to: ${serverConfigurations[currentlyActiveServer]?.name || server}`, 'success-message');
                addConsoleMessage('Type "help" for available commands', 'system-message');

                // Enable disconnect
                disconnectBtn.disabled = false;

                // Set focus to command input
                commandInput.focus();

                // Save server and port to localStorage
                saveServerInfo(server, port, password);

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
                        const streamResponse = await fetch('/apis/admin/rcon/execute', {
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
                        const streamResponse = await fetch('/apis/admin/rcon/execute', {
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
                // Show error message
                addConsoleMessage(`Error: ${data.message}`, 'error-message');

                // Re-enable inputs
                connectBtn.disabled = false;
                streamGameEvents.disabled = false;
                streamServerLog.disabled = false;
                
                if (isManualMode) {
                    serverInput.disabled = false;
                    portInput.disabled = false;
                    passwordInput.disabled = false;
                }
            }
        } catch (error) {
            addConsoleMessage(`Connection error: ${error.message}`, 'error-message');

            // Re-enable inputs
            connectBtn.disabled = false;
            streamGameEvents.disabled = false;
            streamServerLog.disabled = false;
            
            if (isManualMode) {
                serverInput.disabled = false;
                portInput.disabled = false;
                passwordInput.disabled = false;
            }
        }
    }

    function startLogPolling() {
        if (!currentSessionId) return;

        const pollInterval = 1000; // Poll every second

        const poll = async () => {
            if (!currentSessionId) return; // Stop if disconnected

            try {
                const response = await fetch(`/apis/admin/rcon/logs/${currentSessionId}`);
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

            // Continue polling
            setTimeout(poll, pollInterval);
        };

        // Start polling
        setTimeout(poll, pollInterval);
    }

    // Start game event log stream
    async function startGameEventStream() {
        if (!currentSessionId || isGameEventsActive) {
            return;
        }

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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

    // Start server log stream
    async function startServerLogStream() {
        if (!currentSessionId || isServerLogActive) {
            return;
        }

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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

    // Stop game event log stream
    function startLogPolling() {
        if (!currentSessionId) return;

        // Clear any existing interval
        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
        }

        const pollInterval = 100; // Poll every 100ms for more responsive logs

        window.logPollingInterval = setInterval(async () => {
            if (!currentSessionId) {
                clearInterval(window.logPollingInterval);
                window.logPollingInterval = null;
                return;
            }

            try {
                const response = await fetch(`/apis/admin/rcon/logs/${currentSessionId}`);
                const data = await response.json();

                if (data.success && data.logs && data.logs.length > 0) {
                    // Process each log message
                    data.logs.forEach(logMsg => {
                        if (logMsg.startsWith('LOG GAMEEVENT ')) {
                            addConsoleMessage(logMsg.substring(14), 'log-event');
                        } else if (logMsg.startsWith('LOG SERVERLOG ')) {
                            addConsoleMessage(logMsg.substring(14), 'log-server');
                        }
                    });
                }
            } catch (error) {
                console.error('Log polling error:', error);
            }
        }, pollInterval);
    }

    // Stop server log stream
    async function stopServerLogStream() {
        if (!currentSessionId || !isServerLogActive) {
            return;
        }

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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

    // Stop game event log stream
    async function stopGameEventStream() {
        if (!currentSessionId || !isGameEventsActive) {
            return;
        }

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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

    // Stop server log stream
    async function stopServerLogStream() {
        if (!currentSessionId || !isServerLogActive) {
            return;
        }

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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

    // Disconnect from RCON server
    async function disconnectFromServer() {
        if (!currentSessionId) {
            return;
        }
    
        // Stop polling
        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
            window.logPollingInterval = null;
        }
    
        // Stop any active log streams first (with error handling)
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
            const response = await fetch('/apis/admin/rcon/disconnect', {
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
            connectBtn.disabled = false;
            disconnectBtn.disabled = true;
            streamGameEvents.disabled = false;
            streamServerLog.disabled = false;
            
            if (isManualMode) {
                serverInput.disabled = false;
                portInput.disabled = false;
                passwordInput.disabled = false;
            }
            
            currentSessionId = null;
            isGameEventsActive = false;
            isServerLogActive = false;
            
            addConsoleMessage(data.message, 'system-message');
            return true; // Successfully disconnected
        } catch (error) {
            addConsoleMessage(`Disconnect error: ${error.message}`, 'error-message');
            return false; // Failed to disconnect
        }
    }

    // Send RCON command
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
        }

        // Add command to history
        commandHistory.push(command);
        historyIndex = commandHistory.length;

        // Display command in console
        addConsoleMessage(`> ${command}`, 'console-prompt');

        // Clear input
        commandInput.value = '';

        try {
            const response = await fetch('/apis/admin/rcon/execute', {
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
                    disconnectFromServer();
                } else {
                    addConsoleMessage(`Error: ${data.message}`, 'error-message');
                }
            }
        } catch (error) {
            addConsoleMessage(`Command error: ${error.message}`, 'error-message');
        }
    }

    // Handle command history navigation
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

    // Add RCON command helper
    const commonCommands = [
        { cmd: 'ServerName <name>', desc: 'Change the server name' },
        { cmd: 'MaxTeams <number>', desc: 'Set max player slots' },
        { cmd: 'MinTeams <number>', desc: 'Set min players to start' },
        { cmd: 'ServerPassword <password>', desc: 'Set server password' },
        { cmd: 'quit', desc: 'Restart the server' },
        { cmd: 'logstream-gameevents', desc: 'Start game event logging' },
        { cmd: 'logstream-serverlog', desc: 'Start server logging' },
        { cmd: 'logstream-all', desc: 'Start all log streams' },
        { cmd: '!logstream-gameevents', desc: 'Stop game event logging' },
        { cmd: '!logstream-serverlog', desc: 'Stop server logging' },
        { cmd: '!logstream-all', desc: 'Stop all log streams' },
        { cmd: 'help', desc: 'Show this help message' }
    ];

    // Add help command handler
    commandInput.addEventListener('input', (e) => {
        if (e.target.value.trim().toLowerCase() === 'help') {
            e.target.dataset.willShowHelp = 'true';
        } else {
            delete e.target.dataset.willShowHelp;
        }
    });

    commandInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && e.target.dataset.willShowHelp === 'true') {
            e.preventDefault();
            delete e.target.dataset.willShowHelp;

            // Display help in console
            addConsoleMessage('> help', 'console-prompt');
            addConsoleMessage('Common Dedcon Commands:', 'system-message');

            let helpText = '';
            commonCommands.forEach(cmd => {
                helpText += `${cmd.cmd.padEnd(25)} - ${cmd.desc}\n`;
            });

            addConsoleMessage(helpText, 'console-response');
            commandInput.value = '';
        }
    });

    // Initial setup
    disconnectBtn.disabled = true;
});