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

import Auth from '../auth.js';
import UI from '../ui.js';
import API from '../api.js';
import PERMISSIONS from '../permission-index.js';

const UsersModule = (() => {
    let permissionManifest = {};
    
    const PERMISSION_PRESETS = {
        none: [],
        basic: [
            
            'PAGE_ADMIN_PANEL'
        ],
        moderator: [
            
            'PAGE_ADMIN_PANEL',
            'PAGE_DEMO_MANAGE', 
            'PAGE_PLAYER_LOOKUP',
            'DEMO_VIEW', 
            'DEMO_EDIT', 
            'DEMO_VIEW_ID',
            'BLACKLIST_VIEW',
            'ADMIN_VIEW_REPORTS'
        ],
        admin: [
            'PAGE_ADMIN_PANEL', 
            'PAGE_BLACKLIST_MANAGE', 
            'PAGE_DEMO_MANAGE', 
            'PAGE_PLAYER_LOOKUP', 
            'PAGE_ACCOUNT_MANAGE', 
            'PAGE_MODLIST_MANAGE',
            'PAGE_RESOURCE_MANAGE', 
            'PAGE_DEDCON_MANAGEMENT',
            'USER_VIEW_LIST', 
            'USER_VIEW_DETAILS', 
            'USER_EDIT_BASIC', 
            'USER_APPROVE_REQUESTS',
            'DEMO_VIEW', 
            'DEMO_UPLOAD', 
            'DEMO_EDIT', 
            'DEMO_DELETE', 
            'DEMO_VIEW_ID',
            'BLACKLIST_VIEW', 
            'BLACKLIST_ADD', 
            'BLACKLIST_REMOVE',
            'BLACKLIST_APPROVE_REQUESTS',
            'MOD_VIEW', 
            'MOD_ADD', 
            'MOD_EDIT', 
            'MOD_DELETE',
            'RESOURCE_VIEW', 
            'RESOURCE_ADD', 
            'RESOURCE_EDIT', 
            'RESOURCE_DELETE',
            'DEDCON_VIEW', 
            'DEDCON_ADD', 
            'DEDCON_EDIT', 
            'DEDCON_DELETE',
            'ADMIN_VIEW_REPORTS',
            'ADMIN_VIEW_STATISTICS'
        ],
        superadmin: [
            'PAGE_ADMIN_PANEL', 
            'PAGE_BLACKLIST_MANAGE', 
            'PAGE_DEMO_MANAGE', 
            'PAGE_PLAYER_LOOKUP', 
            'PAGE_DEFCON_SERVERS', 
            'PAGE_ACCOUNT_MANAGE', 
            'PAGE_MODLIST_MANAGE', 
            'PAGE_DEDCON_MANAGEMENT', 
            'PAGE_RESOURCE_MANAGE', 
            'PAGE_SERVER_CONSOLE', 
            'PAGE_RCON_CONSOLE',
            'USER_VIEW_LIST', 
            'USER_VIEW_DETAILS', 
            'USER_EDIT_BASIC', 
            'USER_EDIT_PERMISSIONS', 
            'USER_DELETE', 
            'USER_APPROVE_REQUESTS',
            'DEMO_VIEW', 
            'DEMO_UPLOAD', 
            'DEMO_EDIT', 
            'DEMO_DELETE', 
            'DEMO_VIEW_ID',
            'BLACKLIST_VIEW', 
            'BLACKLIST_ADD', 
            'BLACKLIST_REMOVE',
            'BLACKLIST_APPROVE_REQUESTS',
            'MOD_VIEW', 
            'MOD_ADD', 
            'MOD_EDIT', 
            'MOD_DELETE',
            'RESOURCE_VIEW', 
            'RESOURCE_ADD', 
            'RESOURCE_EDIT', 
            'RESOURCE_DELETE',
            'DEDCON_VIEW', 
            'DEDCON_ADD', 
            'DEDCON_EDIT', 
            'DEDCON_DELETE',
            'SERVER_CONSOLE_VIEW', 
            'SERVER_CONSOLE_EXECUTE',
            'RCON_MANUAL_CONNECTION', 
            'RCON_TEST_SERVER', 
            'RCON_1V1_RANDOM',
            'RCON_1V1_DEFAULT', 
            'RCON_1V1_BEST_SETUPS_1', 
            'RCON_1V1_BEST_SETUPS_2',
            'RCON_1V1_CURSED', 
            'RCON_1V1_LOTS_UNITS', 
            'RCON_1V1_UK_IRELAND',
            'RCON_MURICON_UK', 
            'RCON_2V2_UK_IRELAND', 
            'RCON_2V2_RANDOM',
            'RCON_DIPLOMACY_UK', 
            'RCON_RAIZER_RUSSIA_USA', 
            'RCON_NEW_PLAYER',
            'RCON_2V2_TOURNAMENT', 
            'RCON_SONY_HOOV', 
            'RCON_3V3_RANDOM',
            'RCON_MURICON_DEFAULT', 
            'RCON_MURICON_RANDOM', 
            'RCON_509CG_1V1',
            'RCON_FFA_RANDOM', 
            'RCON_8PLAYER_DIPLOMACY', 
            'RCON_4V4_RANDOM',
            'RCON_10PLAYER_DIPLOMACY',
            'CONFIG_1V1_RANDOM',
            'CONFIG_1V1_DEFAULT',
            'CONFIG_1V1_CURSED',
            'CONFIG_1V1_RAIZER',
            'CONFIG_1V1_BEST_SETUPS_1',
            'CONFIG_1V1_BEST_SETUPS_2',
            'CONFIG_1V1_TEST',
            'CONFIG_1V1_VARIABLE',
            'CONFIG_1V1_UK_BALANCED',
            'CONFIG_2V2_DEFAULT',
            'CONFIG_2V2_UK_BALANCED',
            'CONFIG_2V2_TOURNAMENT',
            'CONFIG_6PLAYER_FFA',
            'CONFIG_3V3_FFA',
            'CONFIG_4V4_DEFAULT',
            'CONFIG_5V5_DEFAULT',
            'CONFIG_8PLAYER_DIPLO',
            'CONFIG_10PLAYER_DIPLO',
            'CONFIG_16PLAYER_DEFAULT',
            'CONFIG_UK_MOD_DIPLO',
            'CONFIG_MURICON_UK_MOD',
            'CONFIG_SONY_HOOV',
            'CONFIG_NOOB_FRIENDLY',
            'CONFIG_MURICON_RANDOM',
            'CONFIG_MURICON_DEFAULT',
            'CONFIG_HAWHAW_2V2',
            'ADMIN_VIEW_REPORTS',
            'ADMIN_VIEW_STATISTICS',
            'ADMIN_SYSTEM_MAINTENANCE',
            'ADMIN_CONFIGURE_WEBSITE'
        ]
    };

    async function loadUsers() {
        if (!Auth.requirePermission(PERMISSIONS.USER_VIEW_LIST, 'view users')) return;

        try {
            
            const manifestResponse = await API.get('/api/permissions/manifest');
            permissionManifest = manifestResponse.manifest;
            
            const users = await API.get('/api/users');
            const userTableBody = document.querySelector('#user-table tbody');
            
            if (!userTableBody) return;
            
            userTableBody.innerHTML = '';
            
            users.forEach(user => {
                const row = document.createElement('tr');
                const permissionCount = user.permissions ? user.permissions.length : 0;
                const permissionSummary = permissionCount > 0 
                    ? `${permissionCount} permission${permissionCount !== 1 ? 's' : ''}`
                    : 'No permissions';
                
                
                let actionsHtml = '';
                if (Auth.hasPermission(PERMISSIONS.USER_EDIT_BASIC)) {
                    actionsHtml += `<button class="edit-button" onclick="AdminApp.editUser(${user.id})">Edit</button>`;
                }
                if (Auth.hasPermission(PERMISSIONS.USER_DELETE)) {
                    actionsHtml += `<button class="cancel-button" onclick="AdminApp.deleteUser(${user.id})">Delete</button>`;
                }
                
                row.innerHTML = `
                    <td>${user.username}</td>
                    <td>${user.email}</td>
                    <td>
                        <div class="permissions-summary" title="${getPermissionTooltip(user.permissions)}">
                            ${permissionSummary}
                        </div>
                    </td>
                    <td>${actionsHtml}</td>
                `;
                
                userTableBody.appendChild(row);
            });
        } catch (error) {
            console.error('Error loading users:', error);
            await UI.showAlert('Failed to load users: ' + error.message);
        }
    }

    function getPermissionTooltip(userPermissions) {
        if (!userPermissions || userPermissions.length === 0) {
            return 'No permissions assigned';
        }
        
        const permissionNames = userPermissions.map(permId => {
            const permName = Object.keys(permissionManifest).find(name => 
                permissionManifest[name] === permId
            );
            return permName || `Unknown (${permId})`;
        });
        
        return permissionNames.join(', ');
    }

    async function editUser(userId) {
        if (!Auth.requirePermission(PERMISSIONS.USER_EDIT_BASIC, 'edit users')) return;

        try {
            const user = await API.get(`/api/users/${userId}`);
            const idField = document.getElementById('edit-user-id');
            const usernameField = document.getElementById('edit-user-username');
            const emailField = document.getElementById('edit-user-email');

            if (idField) idField.value = user.id;
            if (usernameField) usernameField.value = user.username;
            if (emailField) emailField.value = user.email;

            const permissionsSection = document.querySelector('[data-permission="USER_EDIT_PERMISSIONS"]');
            if (permissionsSection) {
                if (Auth.hasPermission(PERMISSIONS.USER_EDIT_PERMISSIONS)) {
                    permissionsSection.style.display = 'block';
                    loadUserPermissions(user.permissions || []);
                } else {
                    permissionsSection.style.display = 'none';
                }
            }

            UI.showElement('edit-user');
        } catch (error) {
            console.error('Error fetching user details:', error);
            await UI.showAlert('Error fetching user details: ' + error.message);
        }
    }

    function loadUserPermissions(userPermissions) {
        const checkboxes = document.querySelectorAll('input[name="permission"]');
        checkboxes.forEach(checkbox => {
            checkbox.checked = false;
        });

        userPermissions.forEach(permissionId => {
            
            const permissionName = Object.keys(permissionManifest).find(name => 
                permissionManifest[name] === permissionId
            );
            
            if (permissionName) {
                const checkbox = document.querySelector(`input[name="permission"][value="${permissionName}"]`);
                if (checkbox) {
                    checkbox.checked = true;
                }
            }
        });
    }

    async function saveEditUser(event) {
        event.preventDefault();

        if (!Auth.requirePermission(PERMISSIONS.USER_EDIT_BASIC, 'save user edits')) return;

        const userId = document.getElementById('edit-user-id')?.value;
        const username = document.getElementById('edit-user-username')?.value;
        const email = document.getElementById('edit-user-email')?.value;

        if (!userId || !username || !email) {
            await UI.showAlert('Username and email are required');
            return;
        }

        const updatedUser = {
            username,
            email
        };

        if (Auth.hasPermission(PERMISSIONS.USER_EDIT_PERMISSIONS)) {
            const selectedPermissions = [];
            const checkboxes = document.querySelectorAll('input[name="permission"]:checked');
            
            checkboxes.forEach(checkbox => {
                const permissionName = checkbox.value;
                const permissionId = permissionManifest[permissionName];
                if (permissionId !== undefined) {
                    selectedPermissions.push(permissionId);
                }
            });
            
            updatedUser.permissions = selectedPermissions;
        }

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
        if (!Auth.requirePermission(PERMISSIONS.USER_DELETE, 'delete users')) return;

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
        
        const checkboxes = document.querySelectorAll('input[name="permission"]');
        checkboxes.forEach(checkbox => {
            checkbox.checked = false;
        });
    }

    function applyPermissionPreset(presetName) {
        if (!Auth.hasPermission(PERMISSIONS.USER_EDIT_PERMISSIONS)) {
            UI.showAlert('You do not have permission to edit user permissions');
            return;
        }

        const permissions = PERMISSION_PRESETS[presetName] || [];
        const checkboxes = document.querySelectorAll('input[name="permission"]');
        checkboxes.forEach(checkbox => {
            checkbox.checked = false;
        });

        
        permissions.forEach(permissionName => {
            const checkbox = document.querySelector(`input[name="permission"][value="${permissionName}"]`);
            if (checkbox) {
                checkbox.checked = true;
            }
        });

        const presetNames = {
            none: 'All permissions cleared',
            basic: 'Basic user permissions applied',
            moderator: 'Moderator permissions applied',
            admin: 'Admin permissions applied',
            superadmin: 'Super Admin permissions applied'
        };
        
        UI.showAlert(presetNames[presetName] || 'Permissions applied');
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

        const playerSearchButton = document.getElementById('search-players-btn');
        if (playerSearchButton) {
            playerSearchButton.addEventListener('click', searchPlayers);
        }

        const playerSearchInput = document.getElementById('player-search');
        if (playerSearchInput) {
            playerSearchInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    searchPlayers();
                }
            });
        }
    }

    return {
        loadUsers,
        editUser,
        saveEditUser,
        deleteUser,
        cancelEditUser,
        applyPermissionPreset,
        loadTopPlayers,
        searchPlayers,
        setupEventListeners
    };
})();

export default UsersModule;