
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

const DemosModule = (() => {
    let allDemos = [];

    async function loadDemos() {
        if (!Auth.hasPermission(5)) return;

        try {
            const demos = await API.get('/api/all-demos');
            allDemos = demos;
            displayFilteredDemos(demos);
        } catch (error) {
            console.error('Error loading demos:', error);
            await UI.showAlert('Failed to load demos: ' + error.message);
        }
    }

    function displayFilteredDemos(demos) {
        const tbody = document.querySelector('#demo-table tbody');
        if (!tbody) return;

        tbody.innerHTML = '';

        demos.forEach(demo => {
            const row = tbody.insertRow();
            let actionsHtml = '';

            if (Auth.hasPermission(5)) {
                actionsHtml += `<button onclick="AdminApp.editDemo(${demo.id})">Edit</button>`;
            }

            if (Auth.hasPermission(2)) {
                actionsHtml += `<button onclick="AdminApp.deleteDemo(${demo.id})">Delete</button>`;
            }

            row.innerHTML = `
                <td>${demo.game_type}</td>
                <td>${demo.id}</td>
                <td>${new Date(demo.date).toLocaleString()}</td>
                <td>${demo.name}</td>
                <td>${actionsHtml}</td>
            `;
        });

        const searchTerm = document.getElementById('demo-search')?.value;
        if (searchTerm) {
            const searchContainer = document.querySelector('.search-container');
            if (searchContainer) {
                const existingCount = searchContainer.querySelector('.results-count');
                if (existingCount) {
                    existingCount.remove();
                }

                const countElement = document.createElement('div');
                countElement.className = 'results-count';
                countElement.style.marginTop = '10px';
                countElement.style.color = '#b8b8b8';
                countElement.textContent = `Found ${demos.length} match${demos.length !== 1 ? 'es' : ''}`;
                searchContainer.appendChild(countElement);
            }
        }
    }

    function searchDemos() {
        const searchTerm = document.getElementById('demo-search')?.value.toLowerCase();
        if (!searchTerm) {
            displayFilteredDemos(allDemos);
            return;
        }

        const filteredDemos = allDemos.filter(demo => {
            if (demo.id.toString().includes(searchTerm) ||
                demo.game_type.toLowerCase().includes(searchTerm) ||
                demo.name.toLowerCase().includes(searchTerm)) {
                return true;
            }

            try {
                const playersData = JSON.parse(demo.players);
                const players = Array.isArray(playersData) ? playersData : (playersData.players || []);

                return players.some(player => {
                    if (!player) return false;

                    if (player.name && player.name.toLowerCase().includes(searchTerm)) {
                        return true;
                    }

                    if (player.key_id && player.key_id.toLowerCase().includes(searchTerm)) {
                        return true;
                    }

                    if (player.ip && player.ip.toLowerCase().includes(searchTerm)) {
                        return true;
                    }

                    return false;
                });
            } catch (error) {
                console.error('Error parsing players data:', error);
                return false;
            }
        });

        displayFilteredDemos(filteredDemos);
    }

    async function uploadDemo(event) {
        event.preventDefault();

        if (!Auth.requirePermission(4, 'upload demos')) return;

        const formData = new FormData(event.target);

        try {
            await API.post('/api/upload-demo', formData, true);
            loadDemos();
            event.target.reset();
            await UI.showAlert('Demo uploaded successfully!');
        } catch (error) {
            console.error('Error uploading demo:', error);
            await UI.showAlert('Error uploading demo: ' + error.message);
        }
    }

    async function editDemo(demoId) {
        if (!Auth.requirePermission(5, 'edit demos')) return;


        try {
            const demo = await API.get(`/api/demo/${demoId}`);

            document.getElementById('edit-demo-id').value = demo.id;
            document.getElementById('edit-demo-name').value = demo.name;
            document.getElementById('edit-game-type').value = demo.game_type;
            document.getElementById('edit-duration').value = demo.duration;
            document.getElementById('edit-players-json').value = demo.players;

            const playersDiv = document.getElementById('edit-players');
            if (!playersDiv) return;

            playersDiv.innerHTML = '';

            let playersData;
            try {
                const parsedData = JSON.parse(demo.players);

                playersData = Array.isArray(parsedData) ? parsedData : parsedData.players;

                const usingAlliances = playersData.some(player => player.alliance !== undefined);
                const isExpandedGame = (demo.game_type && (
                    demo.game_type.includes('8 Player') ||
                    demo.game_type.includes('4v4') ||
                    demo.game_type.includes('10 Player') ||
                    demo.game_type.includes('5v5') ||
                    demo.game_type.includes('16 Player') ||
                    demo.game_type.includes('8v8') ||
                    demo.game_type.includes('509') ||
                    demo.game_type.includes('CG') ||
                    demo.game_type.includes('MURICON')
                ));

                playersData.forEach((player, index) => {

                    let colorOptions;
                    if (usingAlliances) {
                        if (isExpandedGame) {
                            colorOptions = `
                                        <div class="alliance-section">
                                            <label>Alliance (Group Color):</label>
                                            <select id="edit-player${index + 1}-alliance" class="alliance-select">
                                                <option value="0" ${player.alliance === 0 ? 'selected' : ''}>Green</option>
                                                <option value="1" ${player.alliance === 1 ? 'selected' : ''}>Red</option>
                                                <option value="2" ${player.alliance === 2 ? 'selected' : ''}>Blue</option>
                                                <option value="3" ${player.alliance === 3 ? 'selected' : ''}>Yellow</option>
                                                <option value="4" ${player.alliance === 4 ? 'selected' : ''}>Turq</option>
                                                <option value="5" ${player.alliance === 5 ? 'selected' : ''}>Pink</option>
                                                <option value="6" ${player.alliance === 6 ? 'selected' : ''}>Black</option>
                                                <option value="7" ${player.alliance === 7 ? 'selected' : ''}>Orange</option>
                                                <option value="8" ${player.alliance === 8 ? 'selected' : ''}>Olive</option>
                                                <option value="9" ${player.alliance === 9 ? 'selected' : ''}>Scarlet</option>
                                                <option value="10" ${player.alliance === 10 ? 'selected' : ''}>Indigo</option>
                                                <option value="11" ${player.alliance === 11 ? 'selected' : ''}>Gold</option>
                                                <option value="12" ${player.alliance === 12 ? 'selected' : ''}>Teal</option>
                                                <option value="13" ${player.alliance === 13 ? 'selected' : ''}>Purple</option>
                                                <option value="14" ${player.alliance === 14 ? 'selected' : ''}>White</option>
                                                <option value="15" ${player.alliance === 15 ? 'selected' : ''}>Brown</option>
                                            </select>
                                        </div>`;
                        } else {
                            colorOptions = `
                                        <div class="alliance-section">
                                            <label>Alliance (Group Color):</label>
                                            <select id="edit-player${index + 1}-alliance" class="alliance-select">
                                                <option value="0" ${player.alliance === 0 ? 'selected' : ''}>Red</option>
                                                <option value="1" ${player.alliance === 1 ? 'selected' : ''}>Green</option>
                                                <option value="2" ${player.alliance === 2 ? 'selected' : ''}>Blue</option>
                                                <option value="3" ${player.alliance === 3 ? 'selected' : ''}>Yellow</option>
                                                <option value="4" ${player.alliance === 4 ? 'selected' : ''}>Orange</option>
                                                <option value="5" ${player.alliance === 5 ? 'selected' : ''}>Turq</option>
                                            </select>
                                        </div>`;
                        }
                    } else {
                        colorOptions = `
                                    <div class="team-section">
                                        <label>Team Color:</label>
                                        <select id="edit-player${index + 1}-team" class="team-select">
                                            <option value="0" ${player.team === 0 ? 'selected' : ''}>Green</option>
                                            <option value="1" ${player.team === 1 ? 'selected' : ''}>Red</option>
                                            <option value="2" ${player.team === 2 ? 'selected' : ''}>Green</option>
                                            <option value="3" ${player.team === 3 ? 'selected' : ''}>Light Blue</option>
                                            <option value="4" ${player.team === 4 ? 'selected' : ''}>Orange</option>
                                            <option value="5" ${player.team === 5 ? 'selected' : ''}>Blue</option>
                                            <option value="6" ${player.team === 6 ? 'selected' : ''}>Yellow</option>
                                            <option value="7" ${player.team === 7 ? 'selected' : ''}>Turq</option>
                                            <option value="8" ${player.team === 8 ? 'selected' : ''}>Pink</option>
                                            <option value="9" ${player.team === 9 ? 'selected' : ''}>White</option>
                                            <option value="10" ${player.team === 10 ? 'selected' : ''}>Teal</option>
                                        </select>
                                    </div>`;
                    }

                    playersDiv.innerHTML += `
                                <div class="player-edit-row">
                                    <input type="text" id="edit-player${index + 1}-name" value="${player.name}" placeholder="Player Name">
                                    <input type="number" id="edit-player${index + 1}-score" value="${player.score}" placeholder="Score">
                                    <input type="text" id="edit-player${index + 1}-territory" value="${player.territory}" placeholder="Territory">
                                    <input type="text" id="edit-player${index + 1}-key_id" value="${player.key_id}" placeholder="Key ID" readonly>
                                    <input type="text" id="edit-player${index + 1}-version" value="${player.version}" placeholder="Version" readonly>
                                    <input type="text" id="edit-player${index + 1}-platform" value="${player.platform}" placeholder="Platform" readonly>
                                    <div class="color-selection">
                                        ${colorOptions}
                                    </div>
                                </div>
                            `;
                });

            } catch (error) {
                console.error('Error parsing game data:', error);
                await UI.showAlert('Error parsing game data: ' + error.message);
                return;
            }

            UI.showElement('demo-edit');
        } catch (error) {
            console.error('Error in editDemo:', error);
            await UI.showAlert('Error loading demo editor: ' + error.message);
        }
    }

    async function saveEditDemo(event) {
        event.preventDefault();

        if (!Auth.requirePermission(5, 'save demo edits')) return;

        const demoId = document.getElementById('edit-demo-id')?.value;
        let originalData;

        try {
            originalData = JSON.parse(document.getElementById('edit-players-json')?.value);
        } catch (error) {
            console.error('Error parsing original data:', error);
            await UI.showAlert('Error parsing game data');
            return;
        }

        const playerRows = document.querySelectorAll('.player-edit-row');
        const updatedPlayers = Array.from(playerRows).map((row, index) => {
            const nameInput = document.getElementById(`edit-player${index + 1}-name`);
            const scoreInput = document.getElementById(`edit-player${index + 1}-score`);
            const territoryInput = document.getElementById(`edit-player${index + 1}-territory`);
            const keyIdInput = document.getElementById(`edit-player${index + 1}-key_id`);
            const versionInput = document.getElementById(`edit-player${index + 1}-version`);
            const platformInput = document.getElementById(`edit-player${index + 1}-platform`);
            const allianceInput = document.getElementById(`edit-player${index + 1}-alliance`);
            const teamInput = document.getElementById(`edit-player${index + 1}-team`);

            const basePlayer = Array.isArray(originalData) ?
                originalData[index] :
                originalData.players[index];

            const updatedPlayer = {
                ...basePlayer,
                name: nameInput.value,
                score: parseInt(scoreInput.value) || 0,
                territory: territoryInput.value,
                key_id: keyIdInput.value,
                version: versionInput.value,
                platform: platformInput.value
            };

            if (allianceInput) {
                updatedPlayer.alliance = parseInt(allianceInput.value);
                updatedPlayer.team = parseInt(allianceInput.value);
            } else if (teamInput) {
                updatedPlayer.team = parseInt(teamInput.value);
            }

            return updatedPlayer;
        });

        const updatedData = Array.isArray(originalData) ?
            updatedPlayers :
            {
                players: updatedPlayers,
                spectators: originalData.spectators || []
            };

        const updatedDemo = {
            name: document.getElementById('edit-demo-name').value,
            game_type: document.getElementById('edit-game-type').value,
            duration: document.getElementById('edit-duration').value,
            players: JSON.stringify(updatedData)
        };

        try {
            const response = await fetch(`/api/demo/${demoId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(updatedDemo),
                credentials: 'include'
            });

            if (response.ok) {
                const result = await confirm('Demo updated successfully!');
                if (result) {
                    window.location.reload();
                }
            } else {
                const error = await response.json();
                await confirm('Failed to update demo: ' + (error.error || 'Unknown error'));
            }
        } catch (error) {
            console.error('Error updating demo:', error);
            await confirm('Error updating demo: ' + error.message);
        }
    }

    async function deleteDemo(demoId) {
        if (!Auth.requirePermission(2, 'delete demos')) return;

        const confirmed = await UI.showConfirm('Are you sure you want to delete this demo?');
        if (confirmed) {
            try {
                await API.del(`/api/demo/${demoId}`);
                await UI.showAlert('Demo deleted successfully');
                loadDemos();
            } catch (error) {
                console.error('Error deleting demo:', error);
                await UI.showAlert('Error deleting demo: ' + error.message);
            }
        }
    }

    function cancelEditDemo() {
        UI.hideElement('demo-edit');
        document.getElementById('edit-demo-form')?.reset();
    }

    function setupEventListeners() {
        const uploadDemoForm = document.getElementById('upload-demo');
        if (uploadDemoForm) {
            uploadDemoForm.addEventListener('submit', uploadDemo);
        }

        const editDemoForm = document.getElementById('edit-demo-form');
        if (editDemoForm) {
            editDemoForm.addEventListener('submit', saveEditDemo);
        }

        const cancelEditButton = document.getElementById('cancel-edit-demo');
        if (cancelEditButton) {
            cancelEditButton.addEventListener('click', cancelEditDemo);
        }

        const searchBtn = document.getElementById('demo-search-btn');
        if (searchBtn) {
            searchBtn.addEventListener('click', searchDemos);
        }

        const searchInput = document.getElementById('demo-search');
        if (searchInput) {
            searchInput.addEventListener('keypress', function (e) {
                if (e.key === 'Enter') {
                    searchDemos();
                }
            });
        }

        const resetBtn = document.getElementById('demo-reset-btn');
        if (resetBtn) {
            resetBtn.addEventListener('click', function () {
                const searchInput = document.getElementById('demo-search');
                if (searchInput) searchInput.value = '';
                displayFilteredDemos(allDemos);

                const resultsCount = document.querySelector('.results-count');
                if (resultsCount) {
                    resultsCount.remove();
                }
            });
        }
    }

    return {
        loadDemos,
        uploadDemo,
        editDemo,
        saveEditDemo,
        deleteDemo,
        cancelEditDemo,
        searchDemos,
        setupEventListeners
    };
})();

export default DemosModule;