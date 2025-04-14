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

const UsersModule = (() => {
    async function loadUsers() {
        if (!Auth.hasPermission(2)) return;

        try {
            const users = await API.get('/api/users');
            const tbody = document.querySelector('#user-table tbody');
            if (!tbody) return;

            tbody.innerHTML = '';
            users.forEach(user => {
                const row = tbody.insertRow();
                let actionsHtml = '';

                if (Auth.hasPermission(2)) {
                    actionsHtml += `<button onclick="AdminApp.editUser(${user.id})">Edit</button>`;
                }

                if (Auth.hasPermission(1)) {
                    actionsHtml += `<button onclick="AdminApp.deleteUser(${user.id})">Delete</button>`;
                }

                row.innerHTML = `
                    <td>${user.username}</td>
                    <td>${user.email}</td>
                    <td>${Auth.getUserRoleName(user.role)}</td>
                    <td>${actionsHtml}</td>
                `;
            });
        } catch (error) {
            console.error('Error loading users:', error);
            await UI.showAlert('Failed to load users: ' + error.message);
        }
    }

    async function editUser(userId) {
        if (!Auth.requirePermission(2, 'edit users')) return;

        try {
            const user = await API.get(`/api/users/${userId}`);

            const idField = document.getElementById('edit-user-id');
            const usernameField = document.getElementById('edit-user-username');
            const emailField = document.getElementById('edit-user-email');
            const roleField = document.getElementById('edit-user-role');

            if (idField) idField.value = user.id;
            if (usernameField) usernameField.value = user.username;
            if (emailField) emailField.value = user.email;
            if (roleField) roleField.value = user.role;

            UI.showElement('edit-user');
        } catch (error) {
            console.error('Error fetching user details:', error);
            await UI.showAlert('Error fetching user details: ' + error.message);
        }
    }

    async function saveEditUser(event) {
        event.preventDefault();

        if (!Auth.requirePermission(2, 'save user edits')) return;

        const userId = document.getElementById('edit-user-id')?.value;
        const username = document.getElementById('edit-user-username')?.value;
        const email = document.getElementById('edit-user-email')?.value;
        const role = document.getElementById('edit-user-role')?.value;

        if (!userId || !username || !email || !role) {
            await UI.showAlert('All fields are required');
            return;
        }

        const updatedUser = {
            username,
            email,
            role: parseInt(role)
        };

        try {
            await API.put(`/api/users/${userId}`, updatedUser);
            await UI.showAlert('User updated successfully');
            loadUsers();
            UI.hideElement('edit-user');
        } catch (error) {
            console.error('Error updating user:', error);
            await UI.showAlert('Error updating user: ' + error.message);
        }
    }

    async function deleteUser(userId) {
        if (!Auth.requirePermission(1, 'delete users')) return;

        const confirmed = await UI.showConfirm('Are you sure you want to delete this user? This action cannot be undone.');
        if (confirmed) {
            try {
                await API.del(`/api/users/${userId}`);
                await UI.showAlert('User deleted successfully');
                loadUsers();
            } catch (error) {
                console.error('Error deleting user:', error);
                await UI.showAlert('Error deleting user: ' + error.message);
            }
        }
    }

    function cancelEditUser() {
        UI.hideElement('edit-user');
        document.getElementById('edit-user-form')?.reset();
    }

    async function loadTopPlayers() {
        try {
            const response = await fetch('/api/all-demos');
            const demos = await response.json();
            const playerStats = new Map();
            const keyIdMap = new Map();
            const ipMap = new Map();
    
            demos.forEach(demo => {
                try {
                    const parsedData = JSON.parse(demo.players);
                    const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
    
                    players.forEach(player => {
                        if (!player.name) return;
    
                        if (!playerStats.has(player.name)) {
                            playerStats.set(player.name, {
                                gamesPlayed: 0,
                                infractions: new Map(),
                                totalInfractions: 0,
                                ips: new Set(),
                                keyIds: new Set(),
                                games: new Set(),
                                alternateNames: new Set(),
                                ratingReasons: []
                            });
                        }
    
                        const stats = playerStats.get(player.name);
                        stats.gamesPlayed++;
                        stats.games.add(demo.id);
    
                        if (player.ip && player.ip.trim() !== '') {
                            stats.ips.add(player.ip);
                            if (!ipMap.has(player.ip)) {
                                ipMap.set(player.ip, new Set());
                            }
                            ipMap.get(player.ip).add(player.name);
                        }
    
                        if (player.key_id && player.key_id.trim() !== '' && player.key_id !== 'DEMO') {
                            stats.keyIds.add(player.key_id);
                            if (!keyIdMap.has(player.key_id)) {
                                keyIdMap.set(player.key_id, new Set());
                            }
                            keyIdMap.get(player.key_id).add(player.name);
                        }
                    });
                } catch (error) {
                    console.error('Error processing demo:', error);
                }
            });
    
            playerStats.forEach((stats, playerName) => {
                stats.keyIds.forEach(keyId => {
                    const relatedNames = keyIdMap.get(keyId);
                    if (relatedNames) {
                        relatedNames.forEach(relatedName => {
                            if (relatedName !== playerName) {
                                stats.alternateNames.add(relatedName);
                                demos.forEach(demo => {
                                    try {
                                        const parsedData = JSON.parse(demo.players);
                                        const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                                        if (players.some(p => p.name === relatedName && p.key_id === keyId)) {
                                            const key = `Same KeyID (${keyId}) used by: ${relatedName}`;
                                            if (!stats.infractions.has(key)) {
                                                stats.infractions.set(key, { count: 0, details: [] });
                                            }
                                            stats.infractions.get(key).count++;
                                            stats.infractions.get(key).details.push({
                                                demoId: demo.id,
                                                date: demo.date
                                            });
                                            stats.totalInfractions++;
                                            stats.ratingReasons.push(key);
                                        }
                                    } catch (error) {
                                        console.error('Error processing demo for infractions:', error);
                                    }
                                });
                            }
                        });
                    }
                });
    
                stats.ips.forEach(ip => {
                    const relatedNames = ipMap.get(ip);
                    if (relatedNames) {
                        relatedNames.forEach(relatedName => {
                            if (relatedName !== playerName) {
                                stats.alternateNames.add(relatedName);
                                demos.forEach(demo => {
                                    try {
                                        const parsedData = JSON.parse(demo.players);
                                        const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                                        if (players.some(p => p.name === relatedName && p.ip === ip)) {
                                            const key = `Same IP (${ip}) used by: ${relatedName}`;
                                            if (!stats.infractions.has(key)) {
                                                stats.infractions.set(key, { count: 0, details: [] });
                                            }
                                            stats.infractions.get(key).count++;
                                            stats.infractions.get(key).details.push({
                                                demoId: demo.id,
                                                date: demo.date
                                            });
                                            stats.totalInfractions++;
                                            stats.ratingReasons.push(key);
                                        }
                                    } catch (error) {
                                        console.error('Error processing demo for infractions:', error);
                                    }
                                });
                            }
                        });
                    }
                });
            });
    
            const topPlayers = Array.from(playerStats.entries())
                .map(([name, stats]) => ({
                    name,
                    gamesPlayed: stats.gamesPlayed,
                    totalInfractions: stats.totalInfractions,
                    infractions: Array.from(stats.infractions.entries()).map(([type, data]) => ({
                        type,
                        count: data.count,
                        details: data.details
                    })),
                    alternateNames: Array.from(stats.alternateNames),
                    ratingReasons: stats.ratingReasons
                }))
                .sort((a, b) => b.gamesPlayed - a.gamesPlayed)
                .slice(0, 10);
    
            displayTopPlayers(topPlayers);
        } catch (error) {
            console.error('Error loading top players:', error);
        }
    }

    function displayTopPlayers(players) {
        const container = document.getElementById('top-players-container');
        if (!container) return;
    
        container.innerHTML = `
            <h2 style="font-size: 24px; margin-bottom: 1rem;">Top 10 Active Players</h2>
            <div id="top-players-list"></div>
        `;
    
        const listContainer = document.getElementById('top-players-list');
    
        players.forEach((player, index) => {
            const playerCard = document.createElement('div');
            playerCard.className = 'player-card';
            playerCard.style.marginBottom = '0.5rem';
            playerCard.style.padding = '0.5rem';
            playerCard.style.borderRadius = '8px';
    
            let nameColor;
            if (player.totalInfractions > 4) {
                nameColor = '#ff0000';
            } else if (player.totalInfractions >= 2) {
                nameColor = '#ff8c00';
            } else if (player.totalInfractions >= 1) {
                nameColor = '#ffdd00';
            } else {
                nameColor = '#27ff00';
            }
    
            const headerHtml = `
                <div class="player-header" style="display: flex; justify-content: space-between; align-items: center; cursor: pointer;">
                    <div>
                        <span style="font-weight: bold; font-size: 18px; color: ${nameColor};">${index + 1}. ${player.name}</span>
                        <span style="margin-left: 1rem; color: #b8b8b8;">${player.gamesPlayed} games</span>
                    </div>
                    <div style="display: flex; align-items: center;">
                        ${player.totalInfractions > 0 ?
                    `<span style="color: ${nameColor}; margin-right: 1rem;">
                                <i class="fas fa-exclamation-triangle"></i> ${player.totalInfractions}
                            </span>` : ''}
                        <i class="fas fa-chevron-down"></i>
                    </div>
                </div>
            `;
    
            playerCard.innerHTML = headerHtml;
    
            if (player.infractions.length > 0 || player.alternateNames.length > 0) {
                const infractionsList = document.createElement('div');
                infractionsList.className = 'infractions-list';
                infractionsList.style.display = 'none';
                infractionsList.style.marginTop = '1rem';
                infractionsList.style.paddingLeft = '1rem';
                infractionsList.style.borderLeft = '2px solid #333';
    
                if (player.alternateNames.length > 0) {
                    infractionsList.innerHTML += `
                        <div style="margin-bottom: 1rem;">
                            <p style="color: ${nameColor}; margin: 0;">Alternative Names:</p>
                            <p style="color: #b8b8b8; margin: 0.5rem 0;">
                                ${player.alternateNames.join(', ')}
                            </p>
                        </div>
                    `;
                }
    
                player.infractions.forEach(infraction => {
                    const infractionHtml = `
                        <div style="margin-bottom: 1rem;">
                            <p style="color: ${nameColor}; margin: 0;">${infraction.type}</p>
                            <p style="color: #b8b8b8; font-size: 14px; margin: 0.5rem 0;">
                                Occurred ${infraction.count} times
                            </p>
                            <div style="font-size: 12px; color: #666;">
                                ${infraction.details.slice(0, 3).map(detail => `
                                    <p style="margin: 0.2rem 0;">
                                        Demo ID: ${detail.demoId} - ${new Date(detail.date).toLocaleDateString()}
                                    </p>
                                `).join('')}
                                ${infraction.details.length > 3 ?
                            `<p style="margin: 0.2rem 0;">+ ${infraction.details.length - 3} more occurrences</p>` :
                            ''}
                            </div>
                        </div>
                    `;
                    infractionsList.innerHTML += infractionHtml;
                });
    
                playerCard.appendChild(infractionsList);
    
                playerCard.querySelector('.player-header').addEventListener('click', () => {
                    const isExpanded = infractionsList.style.display === 'block';
                    infractionsList.style.display = isExpanded ? 'none' : 'block';
                    playerCard.querySelector('.fa-chevron-down').style.transform =
                        isExpanded ? 'rotate(0deg)' : 'rotate(180deg)';
                });
            }
    
            listContainer.appendChild(playerCard);
        });
    }

    async function searchPlayers() {
        const searchTerm = document.getElementById('player-search').value.trim();
        if (!searchTerm) return;
    
        try {
            const response = await fetch('/api/all-demos');
            const demos = await response.json();
    
            const playerMap = new Map();
            const keyIdMap = new Map();
            const ipMap = new Map();
    
            demos.forEach(demo => {
                try {
                    const parsedData = JSON.parse(demo.players);
                    const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                    const spectators = Array.isArray(parsedData) ? [] : (parsedData.spectators || []);
    
                    players.forEach(player => {
                        if (!player.name) return;
    
                        if (!playerMap.has(player.name)) {
                            playerMap.set(player.name, {
                                ips: new Set(),
                                keyIds: new Set(),
                                games: new Set(),
                                alternateNames: new Set(),
                                spectatorNames: new Set(),
                                rating: 50,
                                ratingReasons: [],
                                infractionDemos: new Map()
                            });
                        }
    
                        const playerData = playerMap.get(player.name);
    
                        if (player.ip && player.ip.trim() !== '') {
                            playerData.ips.add(player.ip);
                            if (!ipMap.has(player.ip)) {
                                ipMap.set(player.ip, new Set());
                            }
                            ipMap.get(player.ip).add(player.name);
                        }
    
                        if (player.key_id && player.key_id.trim() !== '' && player.key_id !== 'DEMO') {
                            playerData.keyIds.add(player.key_id);
                            if (!keyIdMap.has(player.key_id)) {
                                keyIdMap.set(player.key_id, new Set());
                            }
                            keyIdMap.get(player.key_id).add(player.name);
                        }
    
                        playerData.games.add(demo.id);
                    });
    
                    spectators.forEach(spectator => {
                        if (!spectator.name || !spectator.key_id) return;
    
                        players.forEach(player => {
                            if (player.key_id === spectator.key_id || player.ip === spectator.ip) {
                                const playerData = playerMap.get(player.name);
                                if (playerData) {
                                    playerData.spectatorNames.add(spectator.name);
                                }
                            }
                        });
                    });
                } catch (error) {
                    console.error('Error processing demo:', error);
                }
            });
    
            playerMap.forEach((data, playerName) => {
                const uniqueAlternateNames = new Set();
    
                data.keyIds.forEach(keyId => {
                    const relatedNames = keyIdMap.get(keyId);
                    if (relatedNames) {
                        relatedNames.forEach(relatedName => {
                            if (relatedName !== playerName && playerMap.has(relatedName)) {
                                uniqueAlternateNames.add(relatedName);
                                data.alternateNames.add(relatedName);
    
                                demos.forEach(demo => {
                                    const parsedData = JSON.parse(demo.players);
                                    const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                                    if (players.some(p => p.name === relatedName && p.key_id === keyId)) {
                                        data.infractionDemos.set(demo.id, {
                                            data: parsedData,
                                            reason: `Same KeyID (${keyId}) used by: ${relatedName}`,
                                            game_type: demo.game_type
                                        });
                                    }
                                });
    
                                data.ratingReasons.push(`Same KeyID (${keyId}) used by: ${relatedName}`);
                            }
                        });
                    }
                });
    
                data.ips.forEach(ip => {
                    const relatedNames = ipMap.get(ip);
                    if (relatedNames) {
                        relatedNames.forEach(relatedName => {
                            if (relatedName !== playerName && playerMap.has(relatedName)) {
                                uniqueAlternateNames.add(relatedName);
                                data.alternateNames.add(relatedName);
    
                                demos.forEach(demo => {
                                    const parsedData = JSON.parse(demo.players);
                                    const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                                    if (players.some(p => p.name === relatedName && p.ip === ip)) {
                                        data.infractionDemos.set(demo.id, {
                                            data: parsedData,
                                            reason: `Same IP (${ip}) used by: ${relatedName}`,
                                            game_type: demo.game_type
                                        });
                                    }
                                });
    
                                data.ratingReasons.push(`Same IP (${ip}) used by: ${relatedName}`);
                            }
                        });
                    }
                });
    
                data.rating = 50 - (uniqueAlternateNames.size * 7);
                data.rating = Math.max(0, data.rating);
            });
    
            const matches = new Set();
    
            playerMap.forEach((data, name) => {
                if (name.toLowerCase().includes(searchTerm.toLowerCase())) {
                    matches.add(name);
                }
            });
    
            keyIdMap.forEach((names, keyId) => {
                if (keyId.includes(searchTerm)) {
                    names.forEach(name => matches.add(name));
                }
            });
    
            ipMap.forEach((names, ip) => {
                if (ip.includes(searchTerm)) {
                    names.forEach(name => matches.add(name));
                }
            });
    
            displaySearchResults(Array.from(matches), playerMap);
    
        } catch (error) {
            console.error('Error searching players:', error);
        }
    }

    function displaySearchResults(matchingPlayers, playerMap) {
        const resultsDiv = document.getElementById('search-results');
        const jsonContainer = document.querySelector('.json-data-container');
        const jsonContent = document.getElementById('json-data-content');

        jsonContainer.style.display = 'none';
        resultsDiv.innerHTML = '';
        jsonContent.innerHTML = '';

        if (matchingPlayers.length === 0) {
            resultsDiv.innerHTML = '<p style="margin-left: 25px;">No matches found</p>';
            return;
        }

        let hasInfractions = false;

        matchingPlayers.forEach(playerName => {
            const data = playerMap.get(playerName);
            const playerDiv = document.createElement('div');
            playerDiv.className = 'player-result';

            let ratingStatus, ratingColor;
            if (data.rating === 50) {
                ratingStatus = "Not a smurf";
                ratingColor = "#00ff00";
            } else if (data.rating >= 36) {
                ratingStatus = "Probably a smurf";
                ratingColor = "#ffff00";
            } else {
                ratingStatus = "Most likely a smurf";
                ratingColor = "#ff0000";
            }

            playerDiv.innerHTML = `
                <h3 style="color: #27ff00; font-size: 27px; margin: 0 !important;">${playerName}</h3>
                <p class="pabout2" style="text-align: left;"><strong>Rating:</strong> <span style="color: ${ratingColor}">${data.rating} (${ratingStatus})</span></p>
                <p class="pabout2" style="text-align: left;"><strong>IPs used:</strong> ${Array.from(data.ips).join(', ') || 'None recorded'}</p>
                <p class="pabout2" style="text-align: left;"><strong>Key IDs used:</strong> ${Array.from(data.keyIds).join(', ') || 'None recorded'}</p>
                <p class="pabout2" style="text-align: left;"><strong>Found in ${data.games.size} games</strong></p>
                ${data.alternateNames.size > 0 ? `
                    <p class="pabout2" style="text-align: left;"><strong style="color: #ffff00;">Names using same KeyID/IP:</strong> ${Array.from(data.alternateNames).join(', ')}</p>
                ` : ''}
                ${data.spectatorNames.size > 0 ? `
                    <p class="pabout2" style="text-align: left;"><strong>Spectator names:</strong> ${Array.from(data.spectatorNames).join(', ')}</p>
                ` : ''}
                ${data.ratingReasons.length > 0 ? `
                    <div class="rating-reasons">
                        <p class="pabout2" style="text-align: left;"><strong style="color: #ff4444;">Rating reduction reasons:</strong></p>
                        <ul>
                            ${data.ratingReasons.map(reason => `<li>${reason}</li>`).join('')}
                        </ul>
                    </div>
                ` : ''}
            `;
            resultsDiv.appendChild(playerDiv);

            let jsonContentHtml = '';
            data.infractionDemos.forEach((infractionData, demoId) => {
                hasInfractions = true;
                jsonContentHtml += `
                    <div class="json-entry">
                        <div class="infraction-title">Demo ID: ${demoId}</div>
                        <div class="infraction-title">Server Name: </div>
                        <p class="pabout2" style="text-align: left;">${infractionData.game_type || 'Unknown'}</p>
                        <div class="infraction-title">Reason: </div>
                        <p class="pabout2" style="text-align: left;">${infractionData.reason}</p>
                        <div class="infraction-title">Raw JSON:</div>
                        <p class="pabout2" style="text-align: left;">${JSON.stringify(infractionData.data, null, 2)}</p>
                    </div>
                `;
            });

            if (jsonContentHtml) {
                jsonContent.innerHTML += jsonContentHtml;
            }

            resultsDiv.appendChild(playerDiv);
        });

        if (hasInfractions) {
            jsonContainer.style.display = 'block';
        }
    }

    function setupEventListeners() {
        const editUserForm = document.getElementById('edit-user-form');
        if (editUserForm) {
            editUserForm.addEventListener('submit', saveEditUser);
        }

        const cancelEditButton = document.getElementById('cancel-edit-user');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditUser);
        }

        const playerSearchBtn = document.getElementById('player-search-btn');
        if (playerSearchBtn) {
            playerSearchBtn.addEventListener('click', searchPlayers);
        }

        const playerSearchInput = document.getElementById('player-search');
        if (playerSearchInput) {
            playerSearchInput.addEventListener('keypress', function (e) {
                if (e.key === 'Enter') {
                    searchPlayers();
                }
            });
        }
    }

    return {
        loadUsers,
        loadTopPlayers,
        editUser,
        saveEditUser,
        deleteUser,
        cancelEditUser,
        searchPlayers,
        setupEventListeners
    };
})();

export default UsersModule;