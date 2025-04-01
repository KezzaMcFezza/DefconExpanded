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

const ReportsModule = (() => {
    async function loadPendingReports() {
        try {
            const reports = await API.get('/api/pending-reports');
            const reportsContainer = document.getElementById('pending-reports');
            if (!reportsContainer) return;
            
            reportsContainer.innerHTML = '';

            reports.forEach(report => {
                const reportElement = document.createElement('div');
                reportElement.className = 'report-card';
                reportElement.innerHTML = `
                    <p class="pabout2" style="text-align: left; margin: 0;">Reported by: ${report.username}</p>
                    <p class="pabout2" style="text-align: left; margin: 0;">Date: ${new Date(report.report_date).toLocaleString()}</p>
                    <p class="pabout2" style="text-align: left; margin: 0;">Issue: ${report.report_type}</p>
                    <button onclick="AdminApp.resolveReport(${report.id})" style="margin-top: 10px;">Resolve Report</button>
                `;

                
                API.get(`/api/demo/${report.demo_id}`)
                    .then(demo => {
                        const demoCard = displayReportedDemo(demo);
                        reportElement.insertBefore(demoCard, reportElement.lastElementChild);
                    })
                    .catch(error => console.error('Error fetching demo details:', error));

                reportsContainer.appendChild(reportElement);
            });

            updateUserRequests(reports.length);

            
            const cleanUrl = `${window.location.origin}${window.location.pathname}`;
            window.history.pushState({}, '', cleanUrl);

        } catch (error) {
            console.error('Error loading pending reports:', error);
            await UI.showAlert('Failed to load pending reports: ' + error.message);
        }
    }

    async function loadPendingModReports() {
        try {
            const reports = await API.get('/api/pending-mod-reports');
            const reportsContainer = document.getElementById('pending-mod-reports');
            if (!reportsContainer) return;
            
            reportsContainer.innerHTML = '';

            reports.forEach(report => {
                const reportElement = document.createElement('div');
                reportElement.className = 'report-card';
                reportElement.innerHTML = `
                    <h3>Report for ${report.mod_name}</h3>
                    <p class="pabout2" style="text-align: left; margin: 0;">Reported by: ${report.username}</p>
                    <p class="pabout2" style="text-align: left; margin: 0;">Date: ${new Date(report.report_date).toLocaleString()}</p>
                    <p class="pabout2" style="text-align: left; margin: 0;">Issue: ${report.report_type}</p>
                    <br>
                    <button onclick="AdminApp.resolveModReport(${report.id})" style="margin-top: 10px;">Resolve Report</button>
                `;

                API.get(`/api/mods/${report.mod_id}`)
                    .then(mod => {
                        const modCard = displayReportedMod(mod);
                        reportElement.insertBefore(modCard, reportElement.lastElementChild);
                    })
                    .catch(error => console.error('Error fetching mod details:', error));

                reportsContainer.appendChild(reportElement);
            });
        } catch (error) {
            console.error('Error loading pending mod reports:', error);
            await UI.showAlert('Failed to load pending mod reports: ' + error.message);
        }
    }

    async function loadPendingRequests() {
        try {
            const requests = await API.get('/api/pending-requests');

            document.getElementById('pending-blacklist-section').innerHTML = '';
            document.getElementById('pending-account-deletion-section').innerHTML = '';
            document.getElementById('pending-account-name-change-section').innerHTML = '';
            document.getElementById('pending-email-change-section').innerHTML = '';

            requests.forEach(request => {
                const requestElement = document.createElement('div');
                requestElement.className = 'request-card';
                let requestContent = `
                    <p class="pabout2" style="text-align: left; margin: 0;">Requested by: ${request.username}</p> 
                    <p class="pabout2" style="text-align: left; margin: 0;">Date: ${new Date(request.request_date).toLocaleString()}</p>
                    <p class="pabout2" style="text-align: left; margin: 0;">Type: ${request.type}</p>
                `;

                switch (request.type) {
                    case 'blacklist':
                        requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">User requests to be blacklisted from leaderboard</p>`;
                        break;
                    case 'account_deletion':
                        requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">User requests account deletion</p>`;
                        break;
                    case 'username_change':
                        requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">Requested Username: ${request.requested_username}</p>`;
                        break;
                    case 'email_change':
                        requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">Requested Email: ${request.requested_email}</p>`;
                        break;
                    default:
                        break;
                }

                requestContent += `
                    <button onclick="AdminApp.resolveRequest(${request.id}, '${request.type}', 'approved')" style="margin-top: 10px;">Approve</button>
                    <button onclick="AdminApp.resolveRequest(${request.id}, '${request.type}', 'rejected')" style="margin-top: 10px;">Reject</button>
                `;

                requestElement.innerHTML = requestContent;

                switch (request.type) {
                    case 'blacklist':
                        document.getElementById('pending-blacklist-section').appendChild(requestElement);
                        break;
                    case 'account_deletion':
                        document.getElementById('pending-account-deletion-section').appendChild(requestElement);
                        break;
                    case 'username_change':
                        document.getElementById('pending-account-name-change-section').appendChild(requestElement);
                        break;
                    case 'email_change':
                        document.getElementById('pending-email-change-section').appendChild(requestElement);
                        break;
                    default:
                        document.getElementById('user-requests').appendChild(requestElement);
                }
            });
        } catch (error) {
            console.error('Error loading pending requests:', error);
            await UI.showAlert('Failed to load pending requests: ' + error.message);
        }
    }

    function updateUserRequests(count) {
        const userRequestsElement = document.getElementById('user-requests');
        if (userRequestsElement) {
            if (count === 0) {
                userRequestsElement.textContent = `0 😊`;
                userRequestsElement.style.color = 'green';
                userRequestsElement.classList.remove('sad-face');
            } else {
                userRequestsElement.textContent = count;

                if (count === 1) {
                    userRequestsElement.style.color = 'orange';
                    userRequestsElement.classList.remove('sad-face');
                } else if (count >= 2) {
                    userRequestsElement.style.color = 'red';
                    userRequestsElement.classList.add('sad-face');
                }
            }
        } else {
            console.error('User requests element not found');
        }
    }

    async function updatePendingReportsCount() {
        try {
            const response = await API.get('/api/pending-reports-count');
            updateUserRequests(response.count);
        } catch (error) {
            console.error('Error updating pending reports count:', error);
        }
    }

    async function resolveReport(reportId) {
        const confirmed = await UI.showConfirm('Have you resolved this issue to the best of your ability?');
        if (confirmed) {
            try {
                await API.put(`/api/resolve-report/${reportId}`);
                loadPendingReports();
                updatePendingReportsCount();
                await UI.showAlert('Thank you for your help!');
            } catch (error) {
                console.error('Error resolving report:', error);
                await UI.showAlert('Failed to resolve report. Please try again.');
            }
        }
    }

    async function resolveModReport(reportId) {
        const confirmed = await UI.showConfirm('Have you resolved this issue to the best of your ability?');
        if (confirmed) {
            try {
                await API.put(`/api/resolve-mod-report/${reportId}`);
                loadPendingModReports();
                updatePendingReportsCount();
                await UI.showAlert('Thank you for your help!');
            } catch (error) {
                console.error('Error resolving report:', error);
                await UI.showAlert('Failed to resolve report. Please try again.');
            }
        }
    }

    async function resolveRequest(requestId, requestType, status) {
        const confirmed = await UI.showConfirm(`Are you sure you want to ${status} this request?`);
        if (confirmed) {
            try {
                await API.put(`/api/resolve-request/${requestId}/${requestType}`, { status });
                loadPendingRequests();
                updatePendingReportsCount();
                await UI.showAlert('Request resolved successfully!');
            } catch (error) {
                console.error('Error resolving request:', error);
                await UI.showAlert('Failed to resolve request. Please try again.');
            }
        }
    }

    function displayReportedDemo(demo) {
        const demoDate = new Date(demo.date);
        const demoCard = document.createElement('div');
        demoCard.className = 'demo-card';
        demoCard.dataset.demoId = demo.id;
    
        const isExpandedGame = (demo.game_type && (
            demo.game_type.includes('8 Player') || demo.game_type.includes('4v4') ||
            demo.game_type.includes('10 Player') || demo.game_type.includes('5v5') ||
            demo.game_type.includes('16 Player') || demo.game_type.includes('8v8') ||
            demo.game_type.includes('509') || demo.game_type.includes('CG') ||
            demo.game_type.includes('MURICON')
        ));
    
        const teamColors = {
            0: { color: '#00bf00', name: 'Green' },
            1: { color: '#ff4949', name: 'Red' },
            2: { color: '#00bf00', name: 'Green' },
            3: { color: '#2da6ff', name: 'Light Blue' },
            4: { color: '#ffa700', name: 'Orange' },
            5: { color: '#3d5cff', name: 'Blue' },
            6: { color: '#e5cb00', name: 'Yellow' },
            7: { color: '#00e5ff', name: 'Turq' },
            8: { color: '#e72de0', name: 'Pink' },
            9: { color: '#e5e5e5', name: 'White' },
            10: { color: '#1fc36a', name: 'Teal' }
        };
    
        const vanillaAllianceColors = {
            0: { color: '#ff4949', name: 'Red' },
            1: { color: '#00bf00', name: 'Green' },
            2: { color: '#3d5cff', name: 'Blue' },
            3: { color: '#e5cb00', name: 'Yellow' },
            4: { color: '#ffa700', name: 'Orange' },
            5: { color: '#00e5ff', name: 'Turq' }
        };
    
        const expandedAllianceColors = {
            0: { color: '#00bf00', name: 'Green' },
            1: { color: '#ff4949', name: 'Red' },
            2: { color: '#3d5cff', name: 'Blue' },
            3: { color: '#e5cb00', name: 'Yellow' },
            4: { color: '#00e5ff', name: 'Turq' },
            5: { color: '#e72de0', name: 'Pink' },
            6: { color: '#4c4c4c', name: 'Black' },
            7: { color: '#ffa700', name: 'Orange' },
            8: { color: '#28660a', name: 'Olive' },
            9: { color: '#660011', name: 'Scarlet' },
            10: { color: '#2a00ff', name: 'Indigo' },
            11: { color: '#4c4c00', name: 'Gold' },
            12: { color: '#004c3e', name: 'Teal' },
            13: { color: '#6a007f', name: 'Purple' },
            14: { color: '#e5e5e5', name: 'White' },
            15: { color: '#964B00', name: 'Brown' }
        };
    
        let parsedPlayers = [];
        let groupScores = {};
        let highestScore = 0;
        let usingAlliances = false;
    
        if (demo.players) {
            try {
                const playersData = typeof demo.players === 'string' ? JSON.parse(demo.players) : demo.players;
    
                parsedPlayers = Array.isArray(playersData) ? playersData : (playersData.players || []);
    
                usingAlliances = parsedPlayers.some(player => player.alliance !== undefined);
    
                const colorSystem = usingAlliances ?
                    (isExpandedGame ? expandedAllianceColors : vanillaAllianceColors) :
                    teamColors;
    
                parsedPlayers.forEach((player, index) => {
                    const groupId = usingAlliances ? player.alliance : player.team;
    
                    if (!groupScores[groupId]) {
                        groupScores[groupId] = 0;
                    }
    
                    groupScores[groupId] += player.score || 0;
    
                    if ((player.score || 0) > highestScore) {
                        highestScore = player.score;
                    }
                });
            } catch (e) {
                console.error('Error parsing players data:', e);
                parsedPlayers = [];
            }
        }
    
        const sortedGroups = Object.entries(groupScores).sort((a, b) => b[1] - a[1]);
        let winningMessage = 'No winning team determined.';
    
        const colorSystem = usingAlliances ? (isExpandedGame ? expandedAllianceColors : vanillaAllianceColors) : teamColors;
    
        if (sortedGroups.length === 2) {
            const [winningGroupId, winningScore] = sortedGroups[0];
            const [secondGroupId, secondScore] = sortedGroups[1];
            const scoreDifference = winningScore - secondScore;
            const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
            const secondGroupName = colorSystem[secondGroupId]?.name || `Team ${secondGroupId}`;
            const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';
            const secondGroupColor = colorSystem[secondGroupId]?.color || '#b8b8b8';
    
            winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won against <span style="color: ${secondGroupColor}">${secondGroupName}</span> by ${scoreDifference} points.`;
        }
    
        let demoCardHtml = `
            <div class="demo-content">
                <h3 class="game-type">${demo.game_type || 'Unknown'}</h3>
                <div style="display: flex; justify-content: space-between;">
                    <p class="time-info">
                        <span class="time-ago">${Utils.getTimeAgo(demo.date)}</span><br>
                        <span class="game-added">Game Added - ${demoDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</span><br>
                        <span class="game-date">Date - ${demoDate.toLocaleDateString()}</span><br>
                        <span class="game-duration">Game Duration - ${Utils.formatDuration(demo.duration)}</span>
                    </p>
                    <p class="winning-team-info" style="text-align: right; color: #b8b8b8;">
                        ${winningMessage}
                    </p>
                </div>`;
    
        demoCardHtml += `
            <div class="territory-map-container" style="position: relative;">
                <img src="/images/base_map.png" class="base-map" alt="Base Map" style="width: 100%; position: relative; z-index: 1;">`;
    
        const territoryImages = {
            'North America': 'northamerica',
            'South America': 'southamerica',
            'Europe': 'europe',
            'Africa': 'africa',
            'Asia': 'asia',
            'Russia': 'russia',
            'North Africa': 'northafrica',
            'South Africa': 'southafrica',
            'East Asia': 'eastasia',
            'West Asia': 'westasia',
            'South Asia': 'southasia',
            'Australasia': 'australasia',
            'Antartica': 'antartica'
        };
    
        parsedPlayers.forEach((player, index) => {
            if (territoryImages[player.territory]) {
                const groupId = usingAlliances ? player.alliance : player.team;
                let colorValue = colorSystem[groupId]?.color || '#00bf00';
                colorValue = colorValue.replace('#', '');
    
                const overlayImage = `/images/${territoryImages[player.territory]}${colorValue}.png`;
    
                demoCardHtml += `
                    <img src="${overlayImage}" class="territory-overlay" alt="${player.territory} Overlay" 
                         style="position: absolute; top: 0; left: 0; width: 100%; z-index: ${index + 2};">`;
            }
        });
    
        demoCardHtml += `</div>`;
    
        demoCardHtml += `
                <table class="game-results">
                    <tr>
                        <th>Player</th>
                        <th>Territory</th>
                        <th>Score</th>
                    </tr>`;
    
        if (parsedPlayers.length > 0) {
            sortedGroups.forEach(([groupId, _]) => {
                const groupColor = colorSystem[groupId]?.color || '#00bf00';
                parsedPlayers
                    .filter(player => {
                        const playerGroupId = usingAlliances ? player.alliance : player.team;
                        return playerGroupId === Number(groupId);
                    })
                    .sort((a, b) => b.score - a.score)
                    .forEach(player => {
                        const playerNameWithCrown = `${player.name}${player.score === highestScore ? ' 👑' : ''}`;
                        const playerIconLink = player.profileUrl
                            ? `<a href="${player.profileUrl}" target="_blank"><i class="fa-solid fa-square-up-right"></i></a>`
                            : '';
    
                        demoCardHtml += `
                            <tr>
                                <td style="color: ${groupColor}">
                                    ${playerIconLink} ${playerNameWithCrown}
                                </td>
                                <td>${player.territory || 'undefined'}</td>
                                <td>${player.score}</td>
                            </tr>`;
                    });
            });
        } else {
            demoCardHtml += '<tr><td colspan="3">No player data available</td></tr>';
        }
    
        demoCardHtml += `</table>`;
    
        if (demo.spectators) {
            try {
                const parsedSpectators = JSON.parse(demo.spectators);
                if (parsedSpectators.length > 0) {
                    demoCardHtml += `
                        <div class="spectators-section">
                            <button class="spectators-toggle" onclick="toggleSpectators(this)">
                                <i class="fas fa-eye"></i> Show Spectators (${parsedSpectators.length})
                            </button>
                            <div class="spectators-list">
                                <table class="spectators-table" style="margin-bottom: 0;">
                                    <tr>
                                    </tr>
                                    ${parsedSpectators.map(spectator => `
                                        <tr>
                                            <td style="color: #919191;">${spectator.name}</td>
                                        </tr>
                                    `).join('')}
                                </table>
                            </div>
                        </div>`;
                }
            } catch (e) {
                console.error('Error parsing spectators JSON:', e);
            }
        }
    
        demoCardHtml += `
                <div class="demo-actions">
                    <a href="/api/download/${demo.name}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                    <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
                </div>
            </div>`;
    
        demoCard.innerHTML = demoCardHtml;
        return demoCard;
    }

    function displayReportedMod(mod) {
        const modCard = document.createElement('div');
        modCard.className = 'reported-mod-card';
    
        const headerImagePath = mod.preview_image_path
            ? '/' + mod.preview_image_path.split('/').slice(-2).join('/')
            : '/modpreviews/icon3.png';
    
        modCard.innerHTML = `
            <div class="mod-header">
                <div class="mod-header-background" style="background-image: url('${headerImagePath}')"></div>
                <div class="mod-header-overlay"></div>
                <div class="mod-title-container">
                    <h3 class="mod-title">${mod.name}</h3>
                    <h3 class="mod-author">By (${mod.creator})</h3>
                    <span class="file-size"><i class="fas fa-file-archive"></i> ${Utils.formatBytes(mod.size)}</span>
                    <p class="mod-subtitle">${mod.description}</p>
                </div>
            </div>
                <div class="mod-info">
                    <div class="mod-meta">
                        <span class="downloads"><i class="fas fa-download"></i> ${mod.download_count || 0}</span>
                        <span class="release-date"><i class="fas fa-calendar-alt"></i> ${new Date(mod.release_date).toLocaleDateString()}</span>
                        <span class="compatibility">${mod.compatibility || 'Unknown'}</span>
                        <a href="/api/download-mod/${mod.id}" class="download-btn"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                    </div>
                </div>
            </div>
        `;
    
        return modCard;
    }

    function setupEventListeners() {

    }

    return {
        loadPendingReports,
        loadPendingModReports,
        loadPendingRequests,
        updatePendingReportsCount,
        resolveReport,
        resolveModReport,
        resolveRequest,
        setupEventListeners
    };
})();

export default ReportsModule;