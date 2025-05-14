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

document.addEventListener('DOMContentLoaded', () => {
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
    const streamGameEvents = document.getElementById('stream-game-events') || { checked: false, disabled: false };
    const streamServerLog = document.getElementById('stream-server-log') || { checked: false, disabled: false };

    let currentSessionId = null;
    let commandHistory = [];
    let historyIndex = -1;
    let isGameEventsActive = false;
    let isServerLogActive = false;
    let currentlyActiveServer = null;
    let isManualMode = false;

    const serverConfigurations = [
        { name: 'Manual Connection', port: null, manual: true },
        { name: 'DefconExpanded Test Server', port: 8800 },
        { name: 'DefconExpanded | 1v1 | Totally Random', port: 8801 },
        { name: 'DefconExpanded | 1v1 | Default', port: 8802 },
        { name: 'DefconExpanded | 1v1 | Best Setups Only!', port: 8803 },
        { name: 'DefconExpanded | 1V1 | Best Setups Only!', port: 8804 },
        { name: 'DefconExpanded | 1v1 | Cursed Setups Only!', port: 8805 },
        { name: 'DefconExpanded | 1v1 | Lots of Units!', port: 8806 },
        { name: 'DefconExpanded | 1v1 | UK and Ireland', port: 8807 },
        { name: 'Muricon | UK Mod', port: 8808 },
        { name: 'DefconExpanded | 2v2 | UK and Ireland', port: 8809 },
        { name: 'DefconExpanded | 2v2 | Totally Random', port: 8810 },
        { name: 'DefconExpanded | Diplomacy | UK and Ireland', port: 8811 },
        { name: 'Raizer\'s Russia vs USA | Totally Random', port: 8812 },
        { name: 'New Player Server', port: 8813 },
        { name: '2v2 Tournament', port: 8814 },
        { name: 'Sony and Hoov\'s Hideout', port: 8815 },
        { name: 'DefconExpanded | 3v3 | Totally Random', port: 8816 },
        { name: 'MURICON | 1v1 Default | 2.8.15', port: 8817 },
        { name: 'MURICON | 1V1 | Totally Random | 2.8.15', port: 8818 },
        { name: '509 CG | 1v1 | Totally Random | 2.8.15', port: 8819 },
        { name: 'DefconExpanded | Free For All | Random Cities', port: 8820 },
        { name: 'DefconExpanded | 8 Player | Diplomacy', port: 8821 },
        { name: 'DefconExpanded | 4V4 | Totally Random', port: 8822 },
        { name: 'DefconExpanded | 10 Player | Diplomacy', port: 8823 }
    ];

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

    async function selectServer(index) {
        if (currentSessionId) {
            await disconnectFromServer();
        }

        const config = serverConfigurations[index];
        
        document.querySelectorAll('#server-list li').forEach(item => {
            item.classList.remove('active');
        });
        document.querySelector(`#server-list li[data-index="${index}"]`).classList.add('active');
        
        if (config.manual) {
            isManualMode = true;
            connectionForm.style.display = 'block';

            const savedServer = localStorage.getItem('rcon_server') || 'localhost';
            const savedPort = localStorage.getItem('rcon_port') || '';
            const savedPassword = localStorage.getItem('rcon_password') || '';
            
            serverInput.value = savedServer;
            portInput.value = savedPort;
            passwordInput.value = savedPassword;
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            
            currentlyActiveServer = null;
            return;
        }
        
        isManualMode = false;
        connectionForm.style.display = 'none';
        serverInput.value = 'localhost';
        portInput.value = config.port;
        passwordInput.value = ''; 
        currentlyActiveServer = index;
    }

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

    const saveServerInfo = (server, port, password) => {
        localStorage.setItem('rcon_server', server);
        localStorage.setItem('rcon_port', port);
        if (password && isManualMode) {
            localStorage.setItem('rcon_password', password);
        }
    };

    loadSavedServerInfo();
    initializeServerList();
    selectServer(0);

    function addConsoleMessage(message, className = '') {
        const messageElement = document.createElement('div');
        messageElement.className = className;
        messageElement.textContent = message;
        consoleOutput.appendChild(messageElement);
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }

    function clearConsole() {
        consoleOutput.innerHTML = '';
    }

    function formatResponse(response) {
        const lines = response.split('\n');
        let formattedResponse = '';

        for (const line of lines) {
            if (line.startsWith('LOG GAMEEVENT ')) {
                addConsoleMessage(line.substring(13), 'log-event');
                continue;
            } else if (line.startsWith('LOG SERVERLOG ')) {
                addConsoleMessage(line.substring(13), 'log-server');
                continue;
            }

            if (line.startsWith('STATUS ')) {
                continue; 
            } else if (line.startsWith('MESSAGE ')) {
                formattedResponse += line.substring(8) + '\n';
            } else {
                formattedResponse += line + '\n';
            }
        }

        return formattedResponse.trim();
    }

    async function connectToServer() {
        if (!isManualMode && currentlyActiveServer === null) {
            addConsoleMessage('Please select a server configuration first', 'error-message');
            return;
        }

        if (isManualMode) {
            if (!serverInput.value || !portInput.value || !passwordInput.value) {
                addConsoleMessage('Error: Server address, port, and password are required', 'error-message');
                return;
            }
        }

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
                connectionStatus.textContent = 'Connected';
                connectionStatus.className = 'status-connected';
                currentSessionId = data.sessionId;
                commandInput.disabled = false;
                sendBtn.disabled = false;

                if (isManualMode) {
                    passwordInput.value = '';
                }

                addConsoleMessage(data.message, 'success-message');
                addConsoleMessage(`Connected to: ${serverConfigurations[currentlyActiveServer]?.name || server}`, 'success-message');
                addConsoleMessage('Type "help" for available commands', 'system-message');

                disconnectBtn.disabled = false;
                commandInput.focus();

                if (isManualMode) {
                    saveServerInfo(server, port, password);
                } else {
                    saveServerInfo(server, port, '');
                }

                isGameEventsActive = enableGameEvents;
                isServerLogActive = enableServerLog;

                if (enableGameEvents || enableServerLog) {
                    startLogPolling();

                    if (enableGameEvents) {
                        addConsoleMessage('Enabling game events stream...', 'system-message');
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
                addConsoleMessage(`Error: ${data.message}`, 'error-message');

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

        const pollInterval = 1000; 

        const poll = async () => {
            if (!currentSessionId) return; 

            try {
                const response = await fetch(`/apis/admin/rcon/logs/${currentSessionId}`);
                const data = await response.json();

                if (data.success && data.logs && data.logs.length > 0) {
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

            setTimeout(poll, pollInterval);
        };

        setTimeout(poll, pollInterval);
    }

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

    function startLogPolling() {
        if (!currentSessionId) return;

        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
        }

        const pollInterval = 100; 

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

    async function disconnectFromServer() {
        if (!currentSessionId) {
            return;
        }

        if (window.logPollingInterval) {
            clearInterval(window.logPollingInterval);
            window.logPollingInterval = null;
        }

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

            connectionStatus.textContent = 'Disconnected';
            connectionStatus.className = 'status-disconnected';
            commandInput.disabled = true;
            sendBtn.disabled = true;
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
            return true; 
        } catch (error) {
            addConsoleMessage(`Disconnect error: ${error.message}`, 'error-message');
            return false; 
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

        commandHistory.push(command);
        historyIndex = commandHistory.length;

        addConsoleMessage(`> ${command}`, 'console-prompt');

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
                const formattedResponse = formatResponse(data.response);
                addConsoleMessage(formattedResponse, 'console-response');
            } else {
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

    connectBtn.addEventListener('click', connectToServer);
    disconnectBtn.addEventListener('click', disconnectFromServer);
    sendBtn.addEventListener('click', sendCommand);
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

    disconnectBtn.disabled = true;
});