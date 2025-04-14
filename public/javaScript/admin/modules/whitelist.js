
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

const WhitelistModule = (() => {
    async function loadWhitelist() {
        if (!Auth.hasPermission(5)) return;
        
        try {
            const whitelist = await API.get('/api/whitelist');
            const tbody = document.querySelector('#whitelist-table tbody');
            if (!tbody) return;
            
            tbody.innerHTML = '';
            whitelist.forEach(player => {
                const row = tbody.insertRow();
                let actionsHtml = '';
                
                if (Auth.hasPermission(5)) {
                    actionsHtml = `<button onclick="AdminApp.removeFromWhitelist('${player.player_name}')">Remove</button>`;
                }
                
                row.innerHTML = `
                    <td>${player.player_name}</td>
                    <td>${player.reason}</td>
                    <td>${actionsHtml}</td>
                `;
            });
        } catch (error) {
            console.error('Error loading whitelist:', error);
            await UI.showAlert('Failed to load whitelist: ' + error.message);
        }
    }

    async function addToWhitelist(event) {
        event.preventDefault();
        
        if (!Auth.requirePermission(5, 'add to the whitelist')) return;
        
        const playerName = document.getElementById('player-name')?.value;
        const reason = document.getElementById('whitelist-reason')?.value;
        
        if (!playerName || !reason) {
            await UI.showAlert('Player name and reason are required');
            return;
        }

        try {
            await API.post('/api/whitelist', { playerName, reason });
            
            loadWhitelist();
            document.getElementById('add-to-whitelist')?.reset();
            await UI.showAlert('Player added to whitelist successfully');
        } catch (error) {
            console.error('Error adding to whitelist:', error);
            await UI.showAlert('Error adding to whitelist: ' + error.message);
        }
    }

    async function removeFromWhitelist(playerName) {
        if (!Auth.requirePermission(5, 'remove from the whitelist')) return;
        
        const confirmed = await UI.showConfirm(`Are you sure you want to remove ${playerName} from the whitelist?`);
        if (confirmed) {
            try {
                await API.del(`/api/whitelist/${encodeURIComponent(playerName)}`);
                loadWhitelist();
                await UI.showAlert('Player removed from whitelist successfully');
            } catch (error) {
                console.error('Error removing from whitelist:', error);
                await UI.showAlert('Failed to remove player from whitelist: ' + error.message);
            }
        }
    }

    function setupEventListeners() {
        const addForm = document.getElementById('add-to-whitelist');
        if (addForm) {
            addForm.addEventListener('submit', addToWhitelist);
        }
    }

    return {
        loadWhitelist,
        addToWhitelist,
        removeFromWhitelist,
        setupEventListeners
    };
})();

export default WhitelistModule;