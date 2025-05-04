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

    // Session state
    let currentSessionId = null;
    let commandHistory = [];
    let historyIndex = -1;

    // API Endpoints
    const API_CONNECT = '/rcon/connect';
    const API_EXECUTE = '/rcon/execute';
    const API_DISCONNECT = '/rcon/disconnect';

    /**
     * Load saved server info from localStorage
     */
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

    /**
     * Save server info to localStorage
     */
    const saveServerInfo = (server, port) => {
        localStorage.setItem('rcon_server', server);
        localStorage.setItem('rcon_port', port);
    };

    /**
     * Add a message to the console output
     * @param {string} message - The message to display
     * @param {string} className - CSS class for styling the message
     */
    function addConsoleMessage(message, className = '') {
        const messageElement = document.createElement('div');
        messageElement.className = className;
        messageElement.textContent = message;
        consoleOutput.appendChild(messageElement);
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }

    /**
     * Format RCON response for display
     * @param {string} response - Raw response from the RCON server
     * @returns {string} Formatted response
     */
    function formatResponse(response) {
        const lines = response.split('\n');
        let formattedResponse = '';
        
        for (const line of lines) {
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

    /**
     * Connect to RCON server
     */
    async function connectToServer() {
        // Disable inputs during connection attempt
        serverInput.disabled = true;
        portInput.disabled = true;
        passwordInput.disabled = true;
        connectBtn.disabled = true;
        
        const server = serverInput.value.trim();
        const port = parseInt(portInput.value);
        const password = passwordInput.value;

        if (!server || !port) {
            addConsoleMessage('Error: Server address and port are required', 'error-message');
            
            // Re-enable inputs
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            connectBtn.disabled = false;
            return;
        }

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
            } else {
                // Show error message and re-enable inputs
                addConsoleMessage(`Error: ${data.message}`, 'error-message');
                
                serverInput.disabled = false;
                portInput.disabled = false;
                passwordInput.disabled = false;
                connectBtn.disabled = false;
            }
        } catch (error) {
            addConsoleMessage(`Connection error: ${error.message}`, 'error-message');
            
            // Re-enable inputs
            serverInput.disabled = false;
            portInput.disabled = false;
            passwordInput.disabled = false;
            connectBtn.disabled = false;
        }
    }

    /**
     * Disconnect from RCON server
     */
    async function disconnectFromServer() {
        if (!currentSessionId) {
            return;
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
            
            currentSessionId = null;
            
            addConsoleMessage(data.message, 'system-message');
        } catch (error) {
            addConsoleMessage(`Disconnect error: ${error.message}`, 'error-message');
        }
    }

    /**
     * Send RCON command
     */
    async function sendCommand() {
        if (!currentSessionId) {
            addConsoleMessage('Error: Not connected to any server', 'error-message');
            return;
        }

        const command = commandInput.value.trim();
        if (!command) {
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
                    disconnectFromServer();
                } else {
                    addConsoleMessage(`Error: ${data.message}`, 'error-message');
                }
            }
        } catch (error) {
            addConsoleMessage(`Command error: ${error.message}`, 'error-message');
        }
    }

    /**
     * Handle command history navigation with up/down arrow keys
     */
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

    /**
     * Generate help text for common RCON commands
     */
    function showHelpText() {
        const commonCommands = [
            { cmd: 'ServerName <name>', desc: 'Change the server name' },
            { cmd: 'MaxTeams <number>', desc: 'Set max player slots' },
            { cmd: 'MinTeams <number>', desc: 'Set min players to start' },
            { cmd: 'ServerPassword <password>', desc: 'Set server password' },
            { cmd: 'quit', desc: 'Restart the server' }
        ];
        
        addConsoleMessage('> help', 'console-prompt');
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
            // Check for help command
            if (commandInput.value.trim().toLowerCase() === 'help') {
                e.preventDefault();
                showHelpText();
                commandInput.value = '';
                return;
            }
            sendCommand();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            handleCommandHistory('up');
        } else if (e.key === 'ArrowDown') {
            e.preventDefault();
            handleCommandHistory('down');
        }
    });
    
    // Load saved server info on page load
    loadSavedServerInfo();
});