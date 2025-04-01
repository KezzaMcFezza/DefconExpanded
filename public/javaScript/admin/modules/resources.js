
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

const ResourcesModule = (() => {
    async function loadResources() {
        if (!Auth.hasPermission(5)) return;
        
        try {
            const resources = await API.get('/api/resources');
            displayResourcesManagement(resources);
        } catch (error) {
            console.error('Error loading resources:', error);
            await UI.showAlert('Failed to load resources: ' + error.message);
        }
    }

    function displayResourcesManagement(resources) {
        const tbody = document.querySelector('#resource-table tbody');
        if (!tbody) return;
        
        tbody.innerHTML = '';

        const sortedResources = resources.sort((a, b) => new Date(b.date) - new Date(a.date));

        sortedResources.forEach(resource => {
            const row = tbody.insertRow();
            let actionsHtml = '';

            if (Auth.hasPermission(4)) {
                actionsHtml += `<button onclick="AdminApp.editResource(${resource.id})">Edit</button>`;
            }
            
            if (Auth.hasPermission(1)) {
                actionsHtml += `<button onclick="AdminApp.deleteResource(${resource.id})">Delete</button>`;
            }

            let platformDisplay = resource.platform;
            if (!platformDisplay || platformDisplay === 'NULL') {
                platformDisplay = '<span style="color: #ff4444;">Not Set</span>';
            } else {
                switch (resource.platform) {
                    case 'macos-intel':
                        platformDisplay = 'MacOS Intel';
                        break;
                    case 'macos-arm64':
                        platformDisplay = 'MacOS ARM64';
                        break;
                    case 'macos':
                        platformDisplay = 'MacOS';
                        break;
                    default:
                        platformDisplay = resource.platform.charAt(0).toUpperCase() + resource.platform.slice(1);
                }
            }

            row.innerHTML = `
                <td>${resource.name}</td>
                <td>${resource.version || '<span style="color: #ff4444;">Not Set</span>'}</td>
                <td>${resource.date ? new Date(resource.date).toLocaleDateString() : '<span style="color: #ff4444;">Not Set</span>'}</td>
                <td>${Utils.formatBytes(resource.size)}</td>
                <td>${platformDisplay}</td>
                <td>${actionsHtml}</td>
            `;
        });
    }

    async function uploadResource(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(3, 'upload resources')) return;
        
        const formData = new FormData(event.target);
        const platform = event.target.id.replace('upload-', '').replace('-resource', '');
        formData.append('platform', platform);

        try {
            await API.post('/api/upload-resource', formData, true);
            await UI.showAlert(`${platform.charAt(0).toUpperCase() + platform.slice(1)} resource uploaded successfully`);
            loadResources();
            event.target.reset();
        } catch (error) {
            console.error('Error uploading resource:', error);
            await UI.showAlert('Error uploading resource: ' + error.message);
        }
    }

    async function editResource(resourceId) {
        if (!Auth.requirePermission(3, 'edit resources')) return;
        
        try {
            const resource = await API.get(`/api/resource/${resourceId}`);
            
            const idField = document.getElementById('edit-resource-id');
            const nameField = document.getElementById('edit-resource-name');
            const versionField = document.getElementById('edit-resource-version');
            const dateField = document.getElementById('edit-resource-date');
            const platformField = document.getElementById('edit-resource-platform');
            
            if (idField) idField.value = resource.id;
            if (nameField) nameField.value = resource.name;
            if (versionField) versionField.value = resource.version;
            if (dateField) dateField.value = new Date(resource.date).toISOString().split('T')[0];
            if (platformField) platformField.value = resource.platform;
            
            UI.showElement('resource-edit');
        } catch (error) {
            console.error('Error fetching resource details:', error);
            await UI.showAlert('Error fetching resource details: ' + error.message);
        }
    }

    async function saveEditResource(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(3, 'save resource edits')) return;
        
        const resourceId = document.getElementById('edit-resource-id')?.value;
        const name = document.getElementById('edit-resource-name')?.value;
        const version = document.getElementById('edit-resource-version')?.value;
        const date = document.getElementById('edit-resource-date')?.value;
        const platform = document.getElementById('edit-resource-platform')?.value;
        
        if (!resourceId || !name) {
            await UI.showAlert('Resource name is required');
            return;
        }

        const updatedResource = { name, version, date, platform };

        try {
            await API.put(`/api/resource/${resourceId}`, updatedResource);
            await UI.showAlert('Resource updated successfully');
            loadResources();
            cancelEditResource();
        } catch (error) {
            console.error('Error updating resource:', error);
            await UI.showAlert('Error updating resource: ' + error.message);
        }
    }

    function cancelEditResource() {
        UI.hideElement('resource-edit');
        document.getElementById('edit-resource-form')?.reset();
    }

    async function deleteResource(resourceId) {
        if (!Auth.requirePermission(1, 'delete resources')) return;
        
        const confirmed = await UI.showConfirm('Are you sure you want to delete this resource?');
        if (confirmed) {
            try {
                await API.del(`/api/resource/${resourceId}`);
                await UI.showAlert('Resource deleted successfully');
                loadResources();
            } catch (error) {
                console.error('Error deleting resource:', error);
                await UI.showAlert('Error deleting resource: ' + error.message);
            }
        }
    }

    function setupEventListeners() {
        const uploadForms = [
            document.getElementById('upload-windows-resource'),
            document.getElementById('upload-linux-resource'),
            document.getElementById('upload-macos-resource'),
            document.getElementById('upload-macos-intel-resource'),
            document.getElementById('upload-macos-arm64-resource')
        ];
        
        uploadForms.forEach(form => {
            if (form) {
                form.addEventListener('submit', uploadResource);
            }
        });
        
        const editForm = document.getElementById('edit-resource-form');
        if (editForm) {
            editForm.addEventListener('submit', saveEditResource);
        }
        
        const cancelEditButton = document.getElementById('cancel-edit-resource');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditResource);
        }
    }

    return {
        loadResources,
        uploadResource,
        editResource,
        saveEditResource,
        deleteResource,
        cancelEditResource,
        setupEventListeners
    };
})();

export default ResourcesModule;