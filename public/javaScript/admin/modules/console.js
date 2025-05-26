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

const ConsoleModule = (() => {
    let socket = null;
    let serverLogStream = false;
    let debugLogStream = false;

    function initializeConsole() {
        if (typeof io === 'undefined') {
            console.error('Socket.IO not loaded');
            return;
        }

        socket = io();
        const outputDiv = document.getElementById('console-output');

        if (!outputDiv) {
            console.error('Console output div not found');
            return;
        }

        socket.on('console_output', (message) => {
            if (serverLogStream) {
                addLogMessage(message, 'server-log');
            }
        });

        socket.on('debug_output', (message) => {
            if (debugLogStream) {
                addLogMessage(message, 'debug-log');
            }
        });

        setupLogStreamControls();
    }

    function addLogMessage(message, type) {
        const outputDiv = document.getElementById('console-output');
        if (!outputDiv) return;

        const logEntry = document.createElement('div');
        logEntry.className = `log-entry ${type}`;

        const timestampRegex = /^(\d{4}-\d{2}-\d{2}-\d{2}:\d{2}:\d{2}\.\d{2})/;
        const match = message.match(timestampRegex);

        if (match) {
            const timestamp = match[1];
            const timestampSpan = document.createElement('span');
            timestampSpan.className = 'timestamp';
            timestampSpan.textContent = timestamp + ' ';
            logEntry.appendChild(timestampSpan);

            const messageContent = message.substring(timestamp.length + 1);
            const messageSpan = document.createElement('span');
            messageSpan.className = type === 'debug-log' ? 'debug-message' : 'message';
            messageSpan.textContent = messageContent;
            logEntry.appendChild(messageSpan);
        } else {
            const messageSpan = document.createElement('span');
            messageSpan.className = type === 'debug-log' ? 'debug-message' : 'message';
            messageSpan.textContent = message;
            logEntry.appendChild(messageSpan);
        }

        outputDiv.appendChild(logEntry);
        outputDiv.scrollTop = outputDiv.scrollHeight;
    }

    function setupLogStreamControls() {
        const serverLogCheckbox = document.getElementById('stream-server-log');
        const debugLogCheckbox = document.getElementById('stream-debug-log');

        if (serverLogCheckbox) {
            serverLogCheckbox.addEventListener('change', (e) => {
                serverLogStream = e.target.checked;
                if (serverLogStream) {
                    addLogMessage('Server log stream enabled', 'system-message');
                    socket.emit('start_server_log_stream');
                } else {
                    addLogMessage('Server log stream disabled', 'system-message');
                    socket.emit('stop_server_log_stream');
                }
            });
        }

        if (debugLogCheckbox) {
            debugLogCheckbox.addEventListener('change', (e) => {
                debugLogStream = e.target.checked;
                if (debugLogStream) {
                    addLogMessage('Debug log stream enabled', 'system-message');
                    socket.emit('start_debug_log_stream');
                } else {
                    addLogMessage('Debug log stream disabled', 'system-message');
                    socket.emit('stop_debug_log_stream');
                }
            });
        }
    }

    function downloadLogs() {
        window.location.href = '/download-logs';
    }

    function setupEventListeners() {
        const downloadButton = document.getElementById('downloadButton');
        if (downloadButton) {
            downloadButton.addEventListener('click', downloadLogs);
        }


        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', initializeConsole);
        } else {

            initializeConsole();
        }
    }

    return {
        initializeConsole,
        downloadLogs,
        setupEventListeners
    };
})();

export default ConsoleModule;