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

import API from '../api.js';
import Auth from '../auth.js';
import UI from '../ui.js';
import PERMISSIONS from '../permission-index.js';

const ConfigModule = (() => {
    let selectedFile = null;
    
    const serverConfigs = [
        { name: "DefconExpanded Test Server", filename: "DefconExpanded Test Server.txt", permission: PERMISSIONS.CONFIG_1V1_TEST },
        { name: "DefconExpanded | 1v1 | Totally Random", filename: "DefconExpanded 1v1 Totally Random.txt", permission: PERMISSIONS.CONFIG_1V1_RANDOM },
        { name: "DefconExpanded | 1v1 | Default", filename: "DefconExpanded 1v1 Default.txt", permission: PERMISSIONS.CONFIG_1V1_DEFAULT },
        { name: "DefconExpanded | 1v1 | Best Setups Only!", filename: "DefconExpanded 1v1 Best Setups Only.txt", permission: PERMISSIONS.CONFIG_1V1_BEST_SETUPS_1 },
        { name: "DefconExpanded | 1v1 | Best Setups Only Second Server", filename: "DefconExpanded 1v1 Best Setups Only Second Server.txt", permission: PERMISSIONS.CONFIG_1V1_BEST_SETUPS_2 },
        { name: "DefconExpanded | 1v1 | Cursed Setups Only!", filename: "DefconExpanded 1v1 Cursed Setups Only.txt", permission: PERMISSIONS.CONFIG_1V1_CURSED },
        { name: "DefconExpanded | 1v1 | Lots of Units!", filename: "DefconExpanded 1v1 Lots of Units.txt", permission: PERMISSIONS.CONFIG_1V1_VARIABLE },
        { name: "DefconExpanded | 1v1 | UK and Ireland", filename: "DefconExpanded 1v1 UK and Ireland.txt", permission: PERMISSIONS.CONFIG_1V1_UK_BALANCED },
        { name: "DefconExpanded | 1v1 | Totally Random - Second Server", filename: "DefconExpanded 1v1 Totally Random - Second Server.txt", permission: PERMISSIONS.CONFIG_1V1_RANDOM },
        { name: "MURICON | 1v1 Default | 2.8.15", filename: "MURICON 1v1 Default 2.8.15.txt", permission: PERMISSIONS.CONFIG_MURICON_DEFAULT },
        { name: "DefconExpanded | 2v2 | Totally Random", filename: "DefconExpanded 2v2 Totally Random.txt", permission: PERMISSIONS.CONFIG_2V2_DEFAULT },
        { name: "DefconExpanded | 2v2 | UK and Ireland", filename: "DefconExpanded 2v2 UK and Ireland.txt", permission: PERMISSIONS.CONFIG_2V2_UK_BALANCED },
        { name: "DefconExpanded | 2v2 | Tournament", filename: "DefconExpanded 2v2 Tournament.txt", permission: PERMISSIONS.CONFIG_2V2_TOURNAMENT },
        { name: "DefconExpanded | 2v2 | Max Cities - Pop", filename: "DefconExpanded 2v2 Max Cities - Pop.txt", permission: PERMISSIONS.CONFIG_2V2_MAX_CITIES },
        { name: "MURICON | 1v1 | Totally Random | 2.8.15", filename: "MURICON 1v1 Totally Random 2.8.15.txt", permission: PERMISSIONS.CONFIG_MURICON_RANDOM },
        { name: "509 CG | 1v1 | Totally Random | 2.8.15", filename: "509 CG 1v1 Totally Random 2.8.15.txt", permission: PERMISSIONS.CONFIG_509CG_1V1 },
        { name: "509 CG | 2v2 | Totally Random | 2.8.15", filename: "509 CG 2v2 Totally Random 2.8.15.txt", permission: PERMISSIONS.CONFIG_HAWHAW_2V2 },
        { name: "Raizer's Russia vs USA | Totally Random", filename: "Raizer's Russia vs USA Totally Random.txt", permission: PERMISSIONS.CONFIG_1V1_RAIZER },
        { name: "Sony and Hoov's Hideout", filename: "Sony and Hoov's Hideout.txt", permission: PERMISSIONS.CONFIG_SONY_HOOV },
        { name: "New Players Welcome | Come and Play!", filename: "New Players Welcome Come and Play.txt", permission: PERMISSIONS.CONFIG_NOOB_FRIENDLY },
        { name: "DefconExpanded | 3v3 | Totally Random", filename: "DefconExpanded 3v3 Totally Random.txt", permission: PERMISSIONS.CONFIG_3V3_FFA },
        { name: "DefconExpanded | 3v3 | Totally Random Second Server", filename: "DefconExpanded 3v3 Totally Random Second Server.txt", permission: PERMISSIONS.CONFIG_3V3_FFA },
        { name: "MURICON | UK Mod", filename: "MURICON UK Mod.txt", permission: PERMISSIONS.CONFIG_MURICON_UK_MOD },
        { name: "DefconExpanded | Free For All | Random Cities", filename: "DefconExpanded Free For All Random Cities.txt", permission: PERMISSIONS.CONFIG_6PLAYER_FFA },
        { name: "DefconExpanded | Training Server", filename: "DefconExpanded Training.txt", permission: PERMISSIONS.CONFIG_TRAINING },
        { name: "DefconExpanded | Diplomacy | UK and Ireland", filename: "DefconExpanded Diplomacy UK and Ireland.txt", permission: PERMISSIONS.CONFIG_UK_MOD_DIPLO },
        { name: "DefconExpanded | 4v4 | Totally Random", filename: "DefconExpanded 4v4 Totally Random.txt", permission: PERMISSIONS.CONFIG_4V4_DEFAULT },
        { name: "DefconExpanded | 5v5 | FFA | Totally Random", filename: "DefconExpanded 5v5 Totally Random.txt", permission: PERMISSIONS.CONFIG_5V5_DEFAULT },
        { name: "DefconExpanded | 8 Player | Diplomacy", filename: "DefconExpanded 8 Player Diplomacy.txt", permission: PERMISSIONS.CONFIG_8PLAYER_DIPLO },
        { name: "DefconExpanded | 10 Player | Diplomacy", filename: "DefconExpanded 10 Player Diplomacy.txt", permission: PERMISSIONS.CONFIG_10PLAYER_DIPLO },
        { name: "DefconExpanded | 16 Player | Test Server", filename: "DefconExpanded 16 Player Test Server.txt", permission: PERMISSIONS.CONFIG_16PLAYER_DEFAULT }
    ];

    async function loadConfigFiles() {
        try {
            const files = await API.get('/api/config-files');
            const fileList = document.getElementById('file-list');
            if (!fileList) return;
            
            fileList.innerHTML = '';
           
            const allowedFiles = files.filter(filename => {
                const serverConfig = serverConfigs.find(config => config.filename === filename);
                return serverConfig && Auth.hasPermission(serverConfig.permission);
            });

            if (allowedFiles.length === 0) {
                const noAccessDiv = document.createElement('div');
                noAccessDiv.className = 'file-item no-access';
                noAccessDiv.style.color = '#ff6666';
                noAccessDiv.style.fontStyle = 'italic';
                noAccessDiv.textContent = 'No config files available - insufficient permissions';
                fileList.appendChild(noAccessDiv);
                return;
            }

            allowedFiles.forEach(file => {
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
            
            const editorTitle = document.getElementById('editor-title');
            if (editorTitle && editorTitle.textContent === 'Select a configuration file to edit') {
                const serverConfig = serverConfigs.find(config => config.filename === filename);
                if (serverConfig) {
                    editorTitle.textContent = `Editing: ${serverConfig.name}`;
                }
            }
            
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

    function cancelEdit() {
        const editorContent = document.getElementById('editor-content');
        if (editorContent) {
            editorContent.value = '';
        }
        
        const editorTitle = document.getElementById('editor-title');
        if (editorTitle) {
            editorTitle.textContent = 'Select a configuration file to edit';
        }
        
        document.querySelectorAll('#server-list li').forEach(item => {
            item.classList.remove('active');
        });
        
        selectedFile = null;
    }

    function populateServerList() {
        const serverList = document.getElementById('server-list');
        if (!serverList) return;
        
        serverList.innerHTML = '';

        const allowedServers = serverConfigs.filter(server => {
            return Auth.hasPermission(server.permission);
        });

        if (allowedServers.length === 0) {
            const noAccessItem = document.createElement('li');
            noAccessItem.textContent = 'No config files available - insufficient permissions';
            noAccessItem.classList.add('no-access');
            noAccessItem.style.color = '#ff6666';
            noAccessItem.style.fontStyle = 'italic';
            serverList.appendChild(noAccessItem);
            return;
        }

        allowedServers.forEach((server, index) => {
            const listItem = document.createElement('li');
            listItem.textContent = server.name;
            listItem.dataset.index = index;
            listItem.dataset.filename = server.filename;
            
            listItem.addEventListener('click', () => selectServer(index, server));
            serverList.appendChild(listItem);
        });
    }

    function selectServer(index, server) {
        
        document.querySelectorAll('#server-list li').forEach(item => {
            item.classList.remove('active');
        });
        
        document.querySelector(`#server-list li[data-index="${index}"]`).classList.add('active');
        
        const editorTitle = document.getElementById('editor-title');
        if (editorTitle) {
            editorTitle.textContent = `Editing: ${server.name}`;
        }
        
        loadFileContent(server.filename);
    }

    function init() {
        populateServerList();
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
        init,
        loadConfigFiles,
        populateServerList,
        selectServer,
        loadFileContent,
        saveFileContent,
        cancelEdit,
        setupEventListeners
    };
})();

export default ConfigModule;