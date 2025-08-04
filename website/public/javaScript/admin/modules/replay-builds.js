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
//Last Edited 04-08-2025


import API from '../api.js';
import Auth from '../auth.js';
import UI from '../ui.js';
import Utils from '../utils.js';
import PERMISSIONS from '../permission-index.js';

const ReplayBuildsModule = (() => {
    async function loadReplayBuilds() {
        try {
            if (window.location.pathname.includes('/replayviewermanagment')) {
                const builds = await API.get('/api/admin/replay-builds');
                displayReplayBuildsManagement(builds);
            } else {
                const builds = await API.get('/api/replay-builds');
                displayReplayBuildsMain(builds);
            }
        } catch (error) {
            console.error('Error loading replay builds:', error);
            await UI.showAlert('Failed to load Replay Viewer builds: ' + error.message);
        }
    }

    function displayReplayBuildsManagement(builds) {
        const tbody = document.querySelector('#replay-table tbody');
        if (!tbody) return;

        tbody.innerHTML = '';
        builds.forEach(build => {
            const row = tbody.insertRow();
            let actionsHtml = '';

            if (Auth.hasPermission(PERMISSIONS.REPLAY_EDIT)) {
                actionsHtml += `<button onclick="AdminApp.editReplayBuild(${build.id})">Edit</button>`;
            }
            
            if (Auth.hasPermission(PERMISSIONS.REPLAY_DELETE)) {
                actionsHtml += `<button onclick="AdminApp.deleteReplayBuild(${build.id})">Delete</button>`;
            }

            row.innerHTML = `
                <td>${build.name}</td>
                <td>${build.version}</td>
                <td>${new Date(build.release_date).toLocaleDateString()}</td>
                <td>${Utils.formatBytes(build.size)}</td>
                <td>${build.platform}</td>
                <td><span style="color: #52e000; font-weight: bold;">Stable</span></td>
                <td>${actionsHtml}</td>
            `;
        });
    }

    async function uploadReplayBuild(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(PERMISSIONS.REPLAY_ADD, 'upload replay builds')) return;

        const formData = new FormData(event.target);
        const platform = event.target.id.replace('upload-', '').replace('-replay', '');
        formData.append('platform', platform);

        try {
            const response = await API.post('/api/upload-replay-build', formData, true);
            await UI.showAlert(`${platform.charAt(0).toUpperCase() + platform.slice(1)} replay build uploaded successfully`);
            loadReplayBuilds();
            event.target.reset();
        } catch (error) {
            console.error('Error uploading replay build:', error);
            await UI.showAlert('Error uploading replay build: ' + error.message);
        }
    }

    async function editReplayBuild(buildId) {
        if (!Auth.requirePermission(PERMISSIONS.REPLAY_EDIT, 'edit replay builds')) return;
        
        try {
            const build = await API.get(`/api/replay-build/${buildId}`);
            
            const idField = document.getElementById('edit-replay-resource-id');
            const nameField = document.getElementById('edit-replay-resource-name');
            const versionField = document.getElementById('edit-replay-resource-version');
            const dateField = document.getElementById('edit-replay-resource-date');
            const platformField = document.getElementById('edit-replay-resource-platform');
            
            if (idField) idField.value = build.id;
            if (nameField) nameField.value = build.name;
            if (versionField) versionField.value = build.version;
            if (dateField) dateField.value = new Date(build.release_date).toISOString().split('T')[0];
            if (platformField) platformField.value = build.platform;
            
            UI.showElement('replay-build-edit');
        } catch (error) {
            console.error('Error fetching replay build details:', error);
            await UI.showAlert('Error fetching replay build details: ' + error.message);
        }
    }

    async function saveEditReplayBuild(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(PERMISSIONS.REPLAY_EDIT, 'save replay build edits')) return;
        
        const buildId = document.getElementById('edit-replay-resource-id')?.value;
        
        const updatedBuild = {
            name: document.getElementById('edit-replay-resource-name')?.value,
            version: document.getElementById('edit-replay-resource-version')?.value,
            release_date: document.getElementById('edit-replay-resource-date')?.value,
            platform: document.getElementById('edit-replay-resource-platform')?.value
        };

        try {
            await API.put(`/api/replay-build/${buildId}`, updatedBuild);
            await UI.showAlert('Replay build updated successfully');
            loadReplayBuilds();
            cancelEditReplay();
        } catch (error) {
            console.error('Error updating replay build:', error);
            await UI.showAlert('Error updating replay build: ' + error.message);
        }
    }

    async function deleteReplayBuild(buildId) {
        if (!Auth.requirePermission(PERMISSIONS.REPLAY_DELETE, 'delete replay builds')) return;
        
        const confirmed = await UI.showConfirm('Are you sure you want to delete this replay build?');
        if (confirmed) {
            try {
                await API.del(`/api/replay-build/${buildId}`);
                await UI.showAlert('Replay build deleted successfully');
                loadReplayBuilds();
            } catch (error) {
                console.error('Error deleting replay build:', error);
                await UI.showAlert('Error deleting replay build: ' + error.message);
            }
        }
    }

    function cancelEditReplay() {
        UI.hideElement('replay-build-edit');
        document.getElementById('edit-replay-resource-form')?.reset();
    }

    function setupEventListeners() {
        const uploadForms = [
            document.getElementById('upload-windows-replay'),
            document.getElementById('upload-linux-replay'),
            document.getElementById('upload-macos-intel-replay'),
            document.getElementById('upload-macos-arm64-replay')
        ];
        
        uploadForms.forEach(form => {
            if (form) {
                form.addEventListener('submit', uploadReplayBuild);
            }
        });
        
        const editForm = document.getElementById('edit-replay-resource-form');
        if (editForm) {
            editForm.addEventListener('submit', saveEditReplayBuild);
        }
        
        const cancelEditButton = document.getElementById('cancel-edit-replay');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditReplay);
        }
    }

    return {
        loadReplayBuilds,
        uploadReplayBuild,
        editReplayBuild,
        deleteReplayBuild,
        saveEditReplayBuild,
        cancelEditReplay,
        setupEventListeners
    };
})();

export default ReplayBuildsModule;