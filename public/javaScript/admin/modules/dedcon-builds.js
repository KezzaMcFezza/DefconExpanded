
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
import Utils from '../utils.js';

const DedconBuildsModule = (() => {
    async function loadDedconBuilds() {
        try {
            if (window.location.pathname.includes('/dedconmanagment')) {
                const builds = await API.get('/api/admin/dedcon-builds');
                displayDedconBuildsManagement(builds);
            } else {
                const builds = await API.get('/api/dedcon-builds');
                displayDedconBuildsMain(builds);
            }
        } catch (error) {
            console.error('Error loading dedcon builds:', error);
            await UI.showAlert('Failed to load DedCon builds: ' + error.message);
        }
    }

    function displayDedconBuildsManagement(builds) {
        const tbody = document.querySelector('#dedcon-table tbody');
        if (!tbody) return;

        tbody.innerHTML = '';
        builds.forEach(build => {
            const row = tbody.insertRow();
            let actionsHtml = '';

            if (Auth.hasPermission(4)) {
                actionsHtml += `<button onclick="AdminApp.editDedconBuild(${build.id})">Edit</button>`;
            }
            
            if (Auth.hasPermission(2)) {
                actionsHtml += `<button onclick="AdminApp.deleteDedconBuild(${build.id})">Delete</button>`;
            }

            row.innerHTML = `
                <td>${build.name}</td>
                <td>${build.version}</td>
                <td>${new Date(build.release_date).toLocaleDateString()}</td>
                <td>${Utils.formatBytes(build.size)}</td>
                <td>${build.platform}</td>
                <td>${actionsHtml}</td>
            `;
        });
    }

    function displayDedconBuildsMain(builds) {

    }

    async function uploadDedconBuild(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(2, 'upload builds')) return;

        const formData = new FormData(event.target);
        const platform = event.target.id.replace('upload-', '').replace('-dedcon', '');
        formData.append('platform', platform);

        try {
            const response = await API.post('/api/upload-dedcon-build', formData, true);
            await UI.showAlert(`${platform.charAt(0).toUpperCase() + platform.slice(1)} build uploaded successfully`);
            loadDedconBuilds();
            event.target.reset();
        } catch (error) {
            console.error('Error uploading build:', error);
            await UI.showAlert('Error uploading build: ' + error.message);
        }
    }

    async function editDedconBuild(buildId) {
        if (!Auth.requirePermission(2, 'edit builds')) return;
        
        try {
            const build = await API.get(`/api/dedcon-build/${buildId}`);
            
            const idField = document.getElementById('edit-dedcon-resource-id');
            const nameField = document.getElementById('edit-dedcon-resource-name');
            const versionField = document.getElementById('edit-dedcon-resource-version');
            const dateField = document.getElementById('edit-dedcon-resource-date');
            const platformField = document.getElementById('edit-resource-platform');
            
            if (idField) idField.value = build.id;
            if (nameField) nameField.value = build.name;
            if (versionField) versionField.value = build.version;
            if (dateField) dateField.value = new Date(build.release_date).toISOString().split('T')[0];
            if (platformField) platformField.value = build.platform;
            
            UI.showElement('dedcon-build-edit');
        } catch (error) {
            console.error('Error fetching build details:', error);
            await UI.showAlert('Error fetching build details: ' + error.message);
        }
    }

    async function saveEditDedconBuild(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(2, 'save build edits')) return;
        
        const buildId = document.getElementById('edit-dedcon-resource-id')?.value;
        const updatedBuild = {
            name: document.getElementById('edit-dedcon-resource-name')?.value,
            version: document.getElementById('edit-dedcon-resource-version')?.value,
            release_date: document.getElementById('edit-dedcon-resource-date')?.value,
            platform: document.getElementById('edit-resource-platform')?.value
        };

        try {
            await API.put(`/api/dedcon-build/${buildId}`, updatedBuild);
            await UI.showAlert('Build updated successfully');
            loadDedconBuilds();
            cancelEditDedcon();
        } catch (error) {
            console.error('Error updating build:', error);
            await UI.showAlert('Error updating build: ' + error.message);
        }
    }

    async function deleteDedconBuild(buildId) {
        if (!Auth.requirePermission(2, 'delete builds')) return;
        
        const confirmed = await UI.showConfirm('Are you sure you want to delete this build?');
        if (confirmed) {
            try {
                await API.del(`/api/dedcon-build/${buildId}`);
                await UI.showAlert('Build deleted successfully');
                loadDedconBuilds();
            } catch (error) {
                console.error('Error deleting build:', error);
                await UI.showAlert('Error deleting build: ' + error.message);
            }
        }
    }

    function cancelEditDedcon() {
        UI.hideElement('dedcon-build-edit');
        document.getElementById('edit-dedcon-resource-form')?.reset();
    }

    function setupEventListeners() {
        const uploadForms = [
            document.getElementById('upload-windows-dedcon'),
            document.getElementById('upload-linux-dedcon'),
            document.getElementById('upload-macos-intel-dedcon'),
            document.getElementById('upload-macos-arm64-dedcon')
        ];
        
        uploadForms.forEach(form => {
            if (form) {
                form.addEventListener('submit', uploadDedconBuild);
            }
        });
        
        const editForm = document.getElementById('edit-dedcon-resource-form');
        if (editForm) {
            editForm.addEventListener('submit', saveEditDedconBuild);
        }
        
        const cancelEditButton = document.getElementById('cancel-edit-dedcon');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditDedcon);
        }
    }

    return {
        loadDedconBuilds,
        uploadDedconBuild,
        editDedconBuild,
        deleteDedconBuild,
        saveEditDedconBuild,
        cancelEditDedcon,
        setupEventListeners
    };
})();

export default DedconBuildsModule;