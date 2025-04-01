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
//Last Edited 01-04-2025

const ConsoleModule = (() => {
    function initializeConsole() {
        if (typeof io === 'undefined') {
            console.error('Socket.IO not loaded');
            return;
        }

        const socket = io();
        const outputDiv = document.getElementById('output');

        if (!outputDiv) {
            console.error('Output div not found');
            return;
        }

        socket.on('console_output', (message) => {
            const logEntry = document.createElement('div');
            logEntry.className = 'log-entry';

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
                messageSpan.className = 'message';
                messageSpan.textContent = messageContent;
                logEntry.appendChild(messageSpan);
            } else {
                logEntry.textContent = message;
            }

            outputDiv.appendChild(logEntry);
            outputDiv.scrollTop = outputDiv.scrollHeight;
        });
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