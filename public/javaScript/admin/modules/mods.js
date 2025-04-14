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

const ModsModule = (() => {
    async function loadMods(type = '', sort = '') {
        if (!Auth.hasPermission(5)) return;
        
        try {
            let url = '/api/mods';
            if (type) url += `?type=${encodeURIComponent(type)}`;
            if (sort) url += `${type ? '&' : '?'}sort=${encodeURIComponent(sort)}`;

            const mods = await API.get(url);
            
            const tbody = document.querySelector('#mod-table tbody');
            if (!tbody) return;
            
            tbody.innerHTML = '';
            mods.forEach(mod => {
                const row = tbody.insertRow();
                let actionsHtml = '';
                
                if (Auth.hasPermission(5)) {
                    actionsHtml += `<button onclick="AdminApp.editMod(${mod.id})">Edit</button>`;
                }
                
                if (Auth.hasPermission(1)) {
                    actionsHtml += `<button onclick="AdminApp.deleteMod(${mod.id})">Delete</button>`;
                }
                
                row.innerHTML = `
                    <td>${mod.name}</td>
                    <td>${mod.type}</td>
                    <td>${mod.creator}</td>
                    <td>${new Date(mod.release_date).toLocaleDateString()}</td>
                    <td>${actionsHtml}</td>
                `;
            });
        } catch (error) {
            console.error('Error loading mods:', error);
            await UI.showAlert('Failed to load mods: ' + error.message);
        }
    }

    async function addMod(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(5, 'add mods')) return;
        
        const formData = new FormData(event.target);
        
        try {
            await API.post('/api/mods', formData, true);
            await UI.showAlert('Mod added successfully');
            loadMods();
            event.target.reset();
        } catch (error) {
            console.error('Error adding mod:', error);
            await UI.showAlert('Error adding mod: ' + error.message);
        }
    }

    async function editMod(modId) {
        if (!Auth.requirePermission(5, 'edit mods')) return;
        
        try {
            const mod = await API.get(`/api/mods/${modId}`);
            
            const idField = document.getElementById('edit-mod-id');
            const nameField = document.getElementById('edit-mod-name');
            const typeField = document.getElementById('edit-mod-type');
            const creatorField = document.getElementById('edit-mod-creator');
            const versionField = document.getElementById('edit-mod-version');
            const compatibilityField = document.getElementById('edit-mod-compatibility');
            const dateField = document.getElementById('edit-mod-release-date');
            const descriptionField = document.getElementById('edit-mod-description');
            
            if (idField) idField.value = mod.id;
            if (nameField) nameField.value = mod.name;
            if (typeField) typeField.value = mod.type;
            if (creatorField) creatorField.value = mod.creator;
            if (versionField) versionField.value = mod.version;
            if (compatibilityField) compatibilityField.value = mod.compatibility;
            if (dateField) dateField.value = new Date(mod.release_date).toISOString().split('T')[0];
            if (descriptionField) descriptionField.value = mod.description;
            
            UI.showElement('mod-edit');
            UI.scrollToTop();
            document.getElementById('edit-mod-name')?.focus();
        } catch (error) {
            console.error('Error fetching mod details:', error);
            await UI.showAlert('Error fetching mod details: ' + error.message);
        }
    }

    async function saveEditMod(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(5, 'save mod edits')) return;
        
        const modId = document.getElementById('edit-mod-id')?.value;
        const formData = new FormData(event.target);
        
        try {
            await API.put(`/api/mods/${modId}`, formData, true);
            await UI.showAlert('Mod updated successfully');
            loadMods();
            cancelEditMod();
        } catch (error) {
            console.error('Error updating mod:', error);
            await UI.showAlert('Error updating mod: ' + error.message);
        }
    }

    async function deleteMod(modId) {
        if (!Auth.requirePermission(2, 'delete mods')) return;
        
        const confirmed = await UI.showConfirm('Are you sure you want to delete this mod?');
        if (confirmed) {
            try {
                await API.del(`/api/mods/${modId}`);
                await UI.showAlert('Mod deleted successfully');
                loadMods();
            } catch (error) {
                console.error('Error deleting mod:', error);
                await UI.showAlert('Error deleting mod: ' + error.message);
            }
        }
    }

    function cancelEditMod() {
        UI.hideElement('mod-edit');
        document.getElementById('edit-mod-form')?.reset();
    }

    function setupEventListeners() {
        const typeFilter = document.getElementById('mod-type-filter');
        if (typeFilter) {
            typeFilter.addEventListener('change', (e) => {
                loadMods(e.target.value);
            });
        }
        
        const sortFilter = document.getElementById('mod-sort');
        if (sortFilter) {
            sortFilter.addEventListener('change', (e) => {
                const typeFilter = document.getElementById('mod-type-filter');
                loadMods(typeFilter ? typeFilter.value : '', e.target.value);
            });
        }
        
        const addModForm = document.getElementById('add-mod-form');
        if (addModForm) {
            addModForm.addEventListener('submit', addMod);
        }
        
        const editModForm = document.getElementById('edit-mod-form');
        if (editModForm) {
            editModForm.addEventListener('submit', saveEditMod);
        }
        
        const cancelEditButton = document.getElementById('cancel-edit-mod');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditMod);
        }
    }

    return {
        loadMods,
        addMod,
        editMod,
        saveEditMod,
        deleteMod,
        cancelEditMod,
        setupEventListeners
    };
})();

export default ModsModule;