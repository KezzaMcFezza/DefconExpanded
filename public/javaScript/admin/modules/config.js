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


import API from '../api.js';
import Auth from '../auth.js';
import UI from '../ui.js';

const ConfigModule = (() => {
    let selectedFile = null;
    
    const serverConfigs = [
        { name: "DefconExpanded | 1v1 | Totally Random", filename: "1v1config.txt" },
        { name: "DefconExpanded | 1v1 | Best Setups 1", filename: "1v1configbest.txt" },
        { name: "DefconExpanded | 1v1 | Best Setups 2", filename: "1v1configbest2.txt" },
        { name: "DefconExpanded | 1v1 | Default", filename: "1v1configdefault.txt" },
        { name: "DefconExpanded Test Server", filename: "1v1configtest.txt" },
        { name: "New Player Server", filename: "noobfriendly.txt" },
        { name: "DefconExpanded | 2v2 | Totally Random", filename: "2v2config.txt" },
        { name: "2v2 Tournament", filename: "2v2tournament.txt" },
        { name: "DefconExpanded | 3v3 | Totally Random", filename: "3v3ffaconfig.txt" },
        { name: "DefconExpanded | Free For All | Random Cities", filename: "6playerffaconfig.txt" },
        { name: "509 CG | 1v1 | Totally Random", filename: "hawhaw1v1config.txt" },
        { name: "509 CG | 2v2 | Totally Random", filename: "hawhaw2v2config.txt" },
        { name: "MURICON | 1v1 Default", filename: "1v1muricon.txt" },
        { name: "DefconExpanded | 4v4 | Totally Random", filename: "4v4config.txt" },
        { name: "DefconExpanded | 5v5 | FFA | Totally Random", filename: "5v5config.txt" },
        { name: "DefconExpanded | 8 Player | Diplomacy", filename: "8playerdiplo.txt" },
        { name: "DefconExpanded | 10 Player | Diplomacy", filename: "10playerdiplo.txt" },
        { name: "DefconExpanded | 16 Player | Test Server", filename: "16playerconfig.txt" }
    ];

    async function loadConfigFiles() {
        try {
            const files = await API.get('/api/config-files');
            const fileList = document.getElementById('file-list');
            if (!fileList) return;
            
            fileList.innerHTML = '';

            files.forEach(file => {
                const div = document.createElement('div');
                div.className = 'file-item';
                div.textContent = file;
                div.onclick = () => loadFileContent(file);
                fileList.appendChild(div);
            });
        } catch (error) {
            console.error('Error loading config files:', error);
            await UI.showAlert('Failed to load config files: ' + error.message);
        }
    }

    async function loadFileContent(filename) {
        try {
            const data = await API.get(`/api/config-files/${filename}`);
            selectedFile = filename;

            const editorContent = document.getElementById('editor-content');
            if (editorContent) editorContent.value = data.content;
            
            UI.showElement('editor');
        } catch (error) {
            console.error('Error loading file content:', error);
            await UI.showAlert(`Error loading file: ${error.message}`);
        }
    }

    async function saveFileContent() {
        if (!selectedFile) return;

        const content = document.getElementById('editor-content')?.value;
        if (!content) {
            await UI.showAlert('No content to save');
            return;
        }

        try {
            await API.put(`/api/config-files/${selectedFile}`, { content });
            await UI.showAlert('File saved successfully!');
        } catch (error) {
            console.error('Error saving file:', error);
            await UI.showAlert('Error saving file: ' + error.message);
        }
    }

    function populateServerList() {
        const serverList = document.getElementById('server-list');
        if (!serverList) return;
        
        serverList.innerHTML = '';

        serverConfigs.forEach(server => {
            const div = document.createElement('div');
            div.className = 'server-item';
            div.innerHTML = `
                <div class="server-name">${server.name}</div>
                <div class="server-details">
                    <div class="config-filename">Config file: ${server.filename}</div>
                </div>
            `;
            div.onclick = () => {
                document.querySelectorAll('.server-item').forEach(item => {
                    item.classList.remove('active');
                });
                div.classList.add('active');
                loadFileContent(server.filename);
            };
            serverList.appendChild(div);
        });
    }

    function setupEventListeners() {
        document.addEventListener('DOMContentLoaded', loadConfigFiles);
        document.addEventListener('DOMContentLoaded', populateServerList);
        
        const saveButton = document.getElementById('save-file-button');
        if (saveButton) {
            saveButton.addEventListener('click', saveFileContent);
        }
    }

    return {
        loadConfigFiles,
        populateServerList,
        loadFileContent,
        saveFileContent,
        setupEventListeners
    };
})();

export default ConfigModule;