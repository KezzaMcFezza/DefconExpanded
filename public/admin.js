let currentUserRole = 6; 
let serverStartTime;

function setupMobileMenu() {
    const sidebar = document.getElementById('sidebar');
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');
    const dropdowns = sidebar.querySelectorAll('.dropdown');

    if (title && listItems) {
        title.addEventListener('click', function(e) {
            e.preventDefault();
            console.log('Title clicked');
            listItems.classList.toggle('show');
            title.classList.toggle('menu-open'); 
            console.log('Menu toggled, show class:', listItems.classList.contains('show'));
        });

        listItems.addEventListener('click', function(e) {
            if (e.target.tagName === 'A' && !e.target.parentElement.classList.contains('dropdown')) {
                console.log('Link clicked, closing menu');
                listItems.classList.remove('show');
                title.classList.remove('menu-open'); 
            }
        });

        dropdowns.forEach(dropdown => {
            const dropdownLink = dropdown.querySelector('a');
            const dropdownContent = dropdown.querySelector('.dropdown-content');
            dropdownLink.addEventListener('click', function(e) {
                e.preventDefault();
                dropdownContent.classList.toggle('show');
                dropdowns.forEach(otherDropdown => {
                    if (otherDropdown !== dropdown) {
                        otherDropdown.querySelector('.dropdown-content').classList.remove('show');
                    }
                });
            });
        });
    } else {
        console.error('Could not find title or list items elements');
    }
}

function setupCustomDialog() {
    const dialogHTML = `
    <div id="custom-dialog" class="custom-dialog">
      <div class="dialog-content">
        <label id="dialog-message"></label>
        <div class="dialog-buttons">
          <button id="dialog-cancel" class="dialog-button">Cancel</button>
          <button id="dialog-confirm" class="dialog-button">OK</button>
        </div>
      </div>
    </div>
    `;

    document.body.insertAdjacentHTML('beforeend', dialogHTML);

    const styles = `
    <style>
      .custom-dialog {
        display: none;
        position: fixed;
        z-index: 1000;
        left: 0;
        top: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0,0,0,0.4);
      }
      .dialog-content {
        background-color: #101010;
        margin: 15% auto;
        padding: 20px;
        width: 350px;
        border-radius: 10px;
        text-align: center;
      }
      .dialog-message {
        color: #1a1a1a;
      }
      #dialog-cancel {
        background-color: #830000;
        color: #ffffff;
      }
      #dialog-confirm {
        background-color: #0067b8;
        color: #ffffff;
      }
      .dialog-button {
        font-size: 0.9rem;
        font-weight: bold;
        padding: 5px 20px;
        margin: 20px 10px 0px 10px;
        border-radius: 10px;
        cursor: pointer;
        border: none;
      }
    </style>
    `;

    document.head.insertAdjacentHTML('beforeend', styles);

    window.confirm = function(message) {
      return new Promise((resolve) => {
        const dialog = document.getElementById('custom-dialog');
        const messageElement = document.getElementById('dialog-message');
        const confirmButton = document.getElementById('dialog-confirm');
        const cancelButton = document.getElementById('dialog-cancel');

        messageElement.textContent = message;
        dialog.style.display = 'block';
        cancelButton.style.display = 'inline-block';

        const closeDialog = (result) => {
          dialog.style.display = 'none';
          resolve(result);
        };

        confirmButton.onclick = () => closeDialog(true);
        cancelButton.onclick = () => closeDialog(false);
      });
    };

    window.alert = function(message) {
      return new Promise((resolve) => {
        const dialog = document.getElementById('custom-dialog');
        const messageElement = document.getElementById('dialog-message');
        const confirmButton = document.getElementById('dialog-confirm');
        const cancelButton = document.getElementById('dialog-cancel');

        messageElement.textContent = message;
        dialog.style.display = 'block';
        cancelButton.style.display = 'none';

        const closeDialog = () => {
          dialog.style.display = 'none';
          resolve();
        };

        confirmButton.onclick = closeDialog;
      });
    };
}

document.addEventListener('DOMContentLoaded', function() {
    checkUserRoleAndInitialize();
});

async function checkUserRoleAndInitialize() {
    try {
        const response = await fetch('/api/current-user');
        const data = await response.json();
        if (data.user) {
            currentUserRole = data.user.role;
            console.log('Current user role:', currentUserRole);
            if (currentUserRole <= 5) { 
                setupCustomDialog(); 
                initializeAdminPanel();
                setupMobileMenu();
                refreshMonitoringData(); 
                loadMonitoringData();
                setActiveNavItem();
            } else {
                console.log('Insufficient permissions. Redirecting to login.');
                window.location.href = '/login';
            }
        } else {
            console.log('No user data. Redirecting to login.');
            window.location.href = '/login';
        }
    } catch (error) {
        console.error('Error checking user role:', error);
        window.location.href = '/login';
    }
}

function initializeAdminPanel() {
    document.querySelectorAll('[data-min-role]').forEach(element => {
        const minRole = parseInt(element.dataset.minRole);
        if (currentUserRole <= minRole) {
            element.style.display = 'block';
        } else {
            element.style.display = 'none';
        }
    });

    if (document.getElementById('add-player-form')) {
        document.getElementById('add-player-form').addEventListener('submit', addPlayer);
    }
    if (document.getElementById('edit-player-form')) {
        document.getElementById('edit-player-form').addEventListener('submit', saveEditPlayer);
    }
    if (document.getElementById('add-to-whitelist')) {
        document.getElementById('add-to-whitelist').addEventListener('submit', addToWhitelist);
    }
    if (document.getElementById('upload-demo')) {
        document.getElementById('upload-demo').addEventListener('submit', uploadDemo);
    }
    if (document.getElementById('edit-demo-form')) {
        document.getElementById('edit-demo-form').addEventListener('submit', saveEditDemo);
    }
    if (document.getElementById('cancel-edit')) {
        document.getElementById('cancel-edit').addEventListener('click', cancelEditDemo);
    }
    if (document.getElementById('edit-resource-form')) {
        document.getElementById('cancel-edit').addEventListener('click', cancelEditResource);
    }
    if (document.getElementById('edit-user-form')) {
        document.getElementById('edit-user-form').addEventListener('submit', saveEditUser);
    }

    loadLeaderboard();
    loadWhitelist();
    loadDemos();
    loadResources();
    loadPendingReports();
    loadPendingModReports();
    loadMods();
    loadUsers();
    updatePendingReportsCount();
    loadPendingRequests();
}

function displayDemos(demos) {

    const demoContainer = document.getElementById('demo-container');
    if (!demoContainer) {
        console.error('Demo container not found.');
        return;
    }

    demoContainer.innerHTML = ''; 

    let columnCount;
    if (window.innerWidth < 500) {
        columnCount = 1;
    } else if (window.innerWidth < 1024) {
        columnCount = 2;
    } else {
        columnCount = 3;
    }

    const columns = Array.from({ length: columnCount }, () => document.createElement('div'));
    columns.forEach(column => column.className = 'demo-column');

    demos.forEach((demo, index) => {
        const columnIndex = index % columnCount;

        try {
            const demoCard = createDemoCard(demo);
            columns[columnIndex].appendChild(demoCard);
        } catch (error) {
            console.error(`Error creating demo card for demo ID ${demo.id}:`, error);
        }
    });

    columns.forEach(column => demoContainer.appendChild(column));
}

function loadDemos(demoList) {
    demoList.forEach(demo => {
        const demoCard = createDemoCard(demo);
        document.querySelector('#demo-container').appendChild(demoCard);
    });
}

function formatDuration(duration) {
    if (!duration) return 'Unknown';
    const [hours, minutes] = duration.split(':').map(Number);
    if (hours === 0) {
        return `${minutes} min`;
    } else {
        return `${hours} hr ${minutes} min`;
    }
}

function getTimeAgo(date) {
    const seconds = Math.floor((new Date() - new Date(date)) / 1000);
    let interval = seconds / 31536000;
    if (interval > 1) return Math.floor(interval) + " years ago";
    interval = seconds / 2592000;
    if (interval > 1) return Math.floor(interval) + " months ago";
    interval = seconds / 86400;
    if (interval > 1) return Math.floor(interval) + " days ago";
    interval = seconds / 3600;
    if (interval > 1) return Math.floor(interval) + " hours ago";
    interval = seconds / 60;
    if (interval > 1) return Math.floor(interval) + " minutes ago";
    return Math.floor(seconds) + " seconds ago";
}

function displayReportedDemo(demo) {
    const demoDate = new Date(demo.date);
    const demoCard = document.createElement('div');
    demoCard.className = 'demo-card';
    demoCard.dataset.demoId = demo.id;

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

    let parsedPlayers = [];
    let teamScores = {};
    let highestScore = 0;

    if (demo.players) {
        try {
            parsedPlayers = JSON.parse(demo.players);
            parsedPlayers.forEach(player => {
                const teamId = player.team || 0;
                if (!teamScores[teamId]) {
                    teamScores[teamId] = 0;
                }
                teamScores[teamId] += player.score;
                if (player.score > highestScore) {
                    highestScore = player.score;
                }
            });
        } catch (e) {
            console.error('Error parsing players JSON:', e);
        }
    }

    const sortedTeams = Object.entries(teamScores).sort((a, b) => b[1] - a[1]);
    let winningMessage = 'No winning team determined.';

    if (sortedTeams.length === 2) {
        const [winningTeamId, winningScore] = sortedTeams[0];
        const [secondTeamId, secondScore] = sortedTeams[1];
        const scoreDifference = winningScore - secondScore;
        const winningTeamName = teamColors[winningTeamId]?.name || `Team ${winningTeamId}`;
        const secondTeamName = teamColors[secondTeamId]?.name || `Team ${secondTeamId}`;
        const winningTeamColor = teamColors[winningTeamId]?.color || '#b8b8b8';
        const secondTeamColor = teamColors[secondTeamId]?.color || '#b8b8b8';
        
        winningMessage = `<span style="color: ${winningTeamColor}">${winningTeamName}</span> won against <span style="color: ${secondTeamColor}">${secondTeamName}</span> by ${scoreDifference} points.`;
    } else if (sortedTeams.length === 1) {
        const [winningTeamId, winningScore] = sortedTeams[0];
        const winningTeamName = teamColors[winningTeamId]?.name || `Team ${winningTeamId}`;
        const winningTeamColor = teamColors[winningTeamId]?.color || '#b8b8b8';
        
        winningMessage = `<span style="color: ${winningTeamColor}">${winningTeamName}</span> won with ${winningScore} points.`;
    }

    let demoCardHtml = `
        <div class="demo-content">
            <h3 class="game-type">${demo.game_type || 'Unknown'}</h3>
            <div style="display: flex; justify-content: space-between;">
                <p class="time-info">
                    <span class="time-ago">${getTimeAgo(demo.date)}</span><br>
                    <span class="game-added">Game Added - ${demoDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</span><br>
                    <span class="game-date">Date - ${demoDate.toLocaleDateString()}</span><br>
                    <span class="game-duration">Game Duration - ${formatDuration(demo.duration)}</span>
                </p>
            </div>`;

    demoCardHtml += `
        <div class="territory-map-container" style="position: relative;">
            <!-- Base map -->
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
            const teamNumber = player.team || 0; 
            let colorName = teamColors[teamNumber]?.color || '#00bf00';

            colorName = colorName.replace('#', '');

            const overlayImage = `/images/${territoryImages[player.territory]}${colorName}.png`;

            demoCardHtml += `
                <img src="${overlayImage}" class="territory-overlay" alt="${player.territory} Overlay" 
                     style="position: absolute; top: 0; left: 0; width: 100%; z-index: ${index + 2};">`;
        } else {
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
        parsedPlayers.sort((a, b) => b.score - a.score).forEach(player => {
            const teamColor = teamColors[player.team || 0]?.color || '#00bf00';
            const playerNameWithCrown = `${player.name}${player.score === highestScore ? ' 👑' : ''}`;
            const playerIconLink = player.profileUrl
                ? `<a href="${player.profileUrl}" target="_blank"><i class="fa-solid fa-square-up-right"></i></a>`
                : '';

            demoCardHtml += `
                <tr>
                    <td style="color: ${teamColor}">
                        ${playerIconLink} ${playerNameWithCrown}
                    </td>
                    <td>${player.territory || 'undefined'}</td>
                    <td>${player.score}</td>
                </tr>`;
        });
    } else {
        demoCardHtml += '<tr><td colspan="3">No player data available</td></tr>';
    }

    demoCardHtml += `
            </table>
            <div class="demo-actions">
                <a href="/api/download/${demo.name}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
            </div>
        </div>`;

    demoCard.innerHTML = demoCardHtml;

    return demoCard;
}

async function loadPendingReports() {
    try {
        const response = await fetch('/api/pending-reports');
        const reports = await response.json();
        const reportsContainer = document.getElementById('pending-reports');
        reportsContainer.innerHTML = '';

        reports.forEach(report => {
            const reportElement = document.createElement('div');
            reportElement.className = 'report-card';
            reportElement.innerHTML = `
                <p class="pabout2" style="text-align: left; margin: 0;">Reported by: ${report.username}</p>
                <p class="pabout2" style="text-align: left; margin: 0;">Date: ${new Date(report.report_date).toLocaleString()}</p>
                <p class="pabout2" style="text-align: left; margin: 0;">Issue: ${report.report_type}</p>
                <button onclick="resolveReport(${report.id})" style="margin-top: 10px;">Resolve Report</button>
            `;

            fetch(`/api/demo/${report.demo_id}`)
                .then(response => response.json())
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
    }
}

async function loadPendingRequests() {
    try {
        const response = await fetch('/api/pending-requests');
        const requests = await response.json();

        document.getElementById('pending-blacklist-section').innerHTML = '';
        document.getElementById('pending-leaderboard-name-change-section').innerHTML = '';
        document.getElementById('pending-account-name-change-section').innerHTML = '';  
        document.getElementById('pending-email-change-section').innerHTML = '';
        document.getElementById('user-requests').innerHTML = '';  

        requests.forEach(request => {
            const requestElement = document.createElement('div');
            requestElement.className = 'request-card';
            let requestContent = `
                <p class="pabout2" style="text-align: left; margin: 0;">Requested by: ${request.username}</p> <!-- Use request.username -->
                <p class="pabout2" style="text-align: left; margin: 0;">Date: ${new Date(request.request_date).toLocaleString()}</p>
                <p class="pabout2" style="text-align: left; margin: 0;">Type: ${request.type}</p>
            `;
        
            switch(request.type) {
                case 'leaderboard_name_change':
                    requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">Requested Name: ${request.requested_name}</p>`;
                    document.getElementById('pending-leaderboard-name-change-section').appendChild(requestElement);
                    break;
                case 'blacklist':
                    requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">User requests to be blacklisted from leaderboard</p>`;
                    document.getElementById('pending-blacklist-section').appendChild(requestElement);
                    break;
                case 'account_deletion':
                    requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">User requests account deletion</p>`;
                    document.getElementById('pending-account-deletion-section').appendChild(requestElement);
                    break;
                case 'username_change':
                    requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">Requested Username: ${request.requested_username}</p>`;
                    document.getElementById('pending-account-name-change-section').appendChild(requestElement);  
                    break;
                case 'email_change':
                    requestContent += `<p class="pabout2" style="text-align: left; margin: 0;">Requested Email: ${request.requested_email}</p>`;
                    document.getElementById('pending-email-change-section').appendChild(requestElement);
                    break;
                default:
                    document.getElementById('user-requests').appendChild(requestElement);  
            }
        
            requestContent += `
                <button onclick="resolveRequest(${request.id}, '${request.type}', 'approved')" style="margin-top: 10px;">Approve</button>
                <button onclick="resolveRequest(${request.id}, '${request.type}', 'rejected')" style="margin-top: 10px;">Reject</button>
            `;
            requestElement.innerHTML = requestContent;
        });

    } catch (error) {
        console.error('Error loading pending requests:', error);
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

async function resolveRequest(requestId, requestType, status) {
    const confirmed = await confirm(`Are you sure you want to ${status} this request?`);
    if (confirmed) {
        try {
            const response = await fetch(`/api/resolve-request/${requestId}/${requestType}`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ status })
            });
            if (response.ok) {
                loadPendingRequests();
                updatePendingReportsCount();
                await alert('Request resolved successfully!');
            } else {
                throw new Error('Failed to resolve request');
            }
        } catch (error) {
            console.error('Error resolving request:', error);
            await alert('Failed to resolve request. Please try again.');
        }
    }
}

async function loadPendingModReports() {
    try {
        const response = await fetch('/api/pending-mod-reports');
        const reports = await response.json();
        const reportsContainer = document.getElementById('pending-mod-reports');
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
                <button onclick="resolveModReport(${report.id})" style="margin-top: 10px;">Resolve Report</button>
            `;

            fetch(`/api/mods/${report.mod_id}`)
                .then(response => response.json())
                .then(mod => {
                    const modCard = displayReportedMod(mod);
                    reportElement.insertBefore(modCard, reportElement.lastElementChild);
                })
                .catch(error => console.error('Error fetching mod details:', error));

            reportsContainer.appendChild(reportElement);
        });
    } catch (error) {
        console.error('Error loading pending mod reports:', error);
    }
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
                <span class="file-size"><i class="fas fa-file-archive"></i> ${formatBytes(mod.size)}</span>
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

async function resolveReport(reportId) {
    const confirmed = await confirm('Have you resolved this issue to the best of your ability?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/resolve-report/${reportId}`, {
                method: 'PUT'
            });
            if (response.ok) {
                loadPendingReports();
                updatePendingReportsCount();
                await alert('Thank you for your help!');
            } else {
                throw new Error('Failed to resolve report');
            }
        } catch (error) {
            console.error('Error resolving report:', error);
            await alert('Failed to resolve report. Please try again.');
        }
    }
}

async function resolveModReport(reportId) {
    const confirmed = await confirm('Have you resolved this issue to the best of your ability?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/resolve-mod-report/${reportId}`, {
                method: 'PUT'
            });
            if (response.ok) {
                loadPendingModReports();
                updatePendingReportsCount();
                await alert('Thank you for your help!');
            } else {
                throw new Error('Failed to resolve report');
            }
        } catch (error) {
            console.error('Error resolving report:', error);
            await alert('Failed to resolve report. Please try again.');
        }
    }
}

async function updatePendingReportsCount() {

}

function showElement(id) {
    const element = document.getElementById(id);
    if (element) element.style.display = 'block';
}

function hideElement(id) {
    const element = document.getElementById(id);
    if (element) element.style.display = 'none';
}

async function loadMonitoringData() {
    try {
        const response = await fetch('/api/monitoring-data');
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }

        const data = await response.json();
        console.log('Monitoring data fetched:', data);

        document.getElementById('server-uptime').textContent = formatUptime(data.uptime);
        document.getElementById('total-demos').textContent = data.totalDemos;

        const userRequestsElement = document.getElementById('user-requests');
        if (userRequestsElement) {
            if (data.userRequests === 0) {
                userRequestsElement.textContent = `0 😊`;
                userRequestsElement.style.color = 'green';
                userRequestsElement.classList.remove('sad-face');
            } else {
                userRequestsElement.textContent = data.userRequests;
                
                if (data.userRequests === 1) {
                    userRequestsElement.style.color = 'orange';
                    userRequestsElement.classList.remove('sad-face');
                } else if (data.userRequests >= 2) {
                    userRequestsElement.style.color = 'red';
                    userRequestsElement.classList.add('sad-face');
                }
            }
        } else {
            console.error('User requests element not found');
        }

    } catch (error) {
        console.error('Error loading monitoring data:', error);
    }
}

function updateMonitoringUI(data) {
    document.getElementById('server-uptime').textContent = formatUptime(data.uptime);
    document.getElementById('total-demos').textContent = data.totalDemos;
    document.getElementById('user-requests').textContent = data.userRequests;
    document.getElementById('demos-processed').textContent = data.demosProcessedToday;

    serverStartTime = new Date(Date.now() - data.uptime * 1000);
    updateUptimeClock();
}

function updateMonitoringUIError() {
    document.getElementById('server-uptime').textContent = 'Error loading data';
    document.getElementById('total-demos').textContent = 'Error';
    document.getElementById('user-requests').textContent = 'Error';
    document.getElementById('demos-processed').textContent = 'Error';
}

function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const remainingSeconds = seconds % 60;

    return `${days}d ${hours}h ${minutes}m ${remainingSeconds}s`;
}

function updateUptimeClock() {
    if (serverStartTime) {
        const now = new Date();
        const uptime = Math.floor((now - serverStartTime) / 1000);
        document.getElementById('server-uptime').textContent = formatUptime(uptime);
    }
}

function refreshMonitoringData() {
    if (currentUserRole <= 5) {
        loadMonitoringData();
    }
}

setInterval(updateUptimeClock, 1000);

async function loadLeaderboard() {
    if (currentUserRole > 5) return;
    try {
        const response = await fetch('/api/leaderboard');
        const leaderboard = await response.json();
        const tbody = document.querySelector('#leaderboard-table tbody');
        tbody.innerHTML = '';
        leaderboard.forEach(player => {
            const row = tbody.insertRow();
            let actionsHtml = '';
            if (currentUserRole <= 5) {
                actionsHtml += `<button onclick="editPlayer(${player.id})">Edit</button>`;
            }
            if (currentUserRole <= 2) {
                actionsHtml += `<button onclick="removePlayer(${player.id})">Remove</button>`;
            }
            row.innerHTML = `
                <td>${player.player_name}</td>
                <td>${player.games_played}</td>
                <td>${player.wins}</td>
                <td>${player.losses}</td>
                <td>${player.total_score}</td>
                <td>${actionsHtml}</td>
            `;
        });
    } catch (error) {
        console.error('Error loading leaderboard:', error);
    }
}

async function editPlayer(playerId) {
    if (currentUserRole > 5) {
        alert('You do not have permission to edit players');
        return;
    }
    try {
        const response = await fetch(`/api/leaderboard/${playerId}`);
        const player = await response.json();
        
        document.getElementById('edit-player-id').value = player.id;
        document.getElementById('edit-player-name').value = player.player_name;
        document.getElementById('edit-player-games').value = player.games_played;
        document.getElementById('edit-player-wins').value = player.wins;
        document.getElementById('edit-player-losses').value = player.losses;
        document.getElementById('edit-player-score').value = player.total_score;

        document.getElementById('edit-player').style.display = 'block';
    } catch (error) {
        console.error('Error fetching player details:', error);
        alert('Error fetching player details: ' + error.message);
    }
}

async function saveEditPlayer(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        alert('You do not have permission to save player edits');
        return;
    }
    const playerId = document.getElementById('edit-player-id').value;
    const updatedPlayer = {
        player_name: document.getElementById('edit-player-name').value,
        games_played: parseInt(document.getElementById('edit-player-games').value),
        wins: parseInt(document.getElementById('edit-player-wins').value),
        losses: parseInt(document.getElementById('edit-player-losses').value),
        total_score: parseInt(document.getElementById('edit-player-score').value)
    };

    try {
        const response = await fetch(`/api/leaderboard/${playerId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updatedPlayer),
            credentials: 'include'
        });
        if (response.ok) {
            loadLeaderboard();
            document.getElementById('edit-player').style.display = 'none';
        } else {
            const error = await response.json();
            console.error('Failed to update player:', error);
            alert('Failed to update player: ' + (error.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating player:', error);
        alert('Error updating player: ' + error.message);
    }
}

async function removePlayer(playerId) {
    if (currentUserRole > 2) {
        await alert('You do not have permission to remove players');
        return;
    }
    const confirmed = await confirm('Are you sure you want to remove this player from the leaderboard?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/leaderboard/${playerId}`, {
                method: 'DELETE',
                credentials: 'include'
            });
            if (response.ok) {
                loadLeaderboard();
            } else {
                await alert('Failed to remove player from leaderboard');
            }
        } catch (error) {
            console.error('Error removing player from leaderboard:', error);
        }
    }
}

async function addPlayer(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        await alert('You do not have permission to add players');
        return;
    }
    const playerName = document.getElementById('add-player-name').value;
    const gamesPlayed = parseInt(document.getElementById('add-player-games').value);
    const wins = parseInt(document.getElementById('add-player-wins').value);
    const losses = parseInt(document.getElementById('add-player-losses').value);
    const totalScore = parseInt(document.getElementById('add-player-score').value);

    if (!playerName || isNaN(gamesPlayed) || isNaN(wins) || isNaN(losses) || isNaN(totalScore)) {
        alert('Please fill in all fields with valid values');
        return;
    }

    const playerData = {
        player_name: playerName,
        games_played: gamesPlayed,
        wins: wins,
        losses: losses,
        total_score: totalScore
    };

    try {
        const response = await fetch('/api/leaderboard', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(playerData),
            credentials: 'include'
        });
        const result = await response.json();
        if (response.ok) {
            console.log('Player added successfully:', result);
            loadLeaderboard();
            event.target.reset();
        } else {
            console.error('Failed to add player:', result.error);
            alert('Failed to add player: ' + (result.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error adding player:', error);
        alert('Error adding player: ' + error.message);
    }
}

async function loadWhitelist() {
    if (currentUserRole > 5) return;
    try {
        const response = await fetch('/api/whitelist');
        const whitelist = await response.json();
        const tbody = document.querySelector('#whitelist-table tbody');
        tbody.innerHTML = '';
        whitelist.forEach(player => {
            const row = tbody.insertRow();
            let actionsHtml = '';
            if (currentUserRole <= 5) {
                actionsHtml = `<button onclick="removeFromWhitelist('${player.player_name}')">Remove</button>`;
            }
            row.innerHTML = `
                <td>${player.player_name}</td>
                <td>${player.reason}</td>
                <td>${actionsHtml}</td>
            `;
        });
    } catch (error) {
        console.error('Error loading whitelist:', error);
    }
}

async function addToWhitelist(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        alert('You do not have permission to add to the whitelist');
        return;
    }
    const playerName = document.getElementById('player-name').value;
    const reason = document.getElementById('whitelist-reason').value;
    
    try {
        const response = await fetch('/api/whitelist', {
            method: 'POST',
            headers: { 
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            body: JSON.stringify({ playerName, reason }),
            credentials: 'include'
        });
        
        const responseData = await response.json();
        
        if (response.ok) {
            console.log('Successfully added to whitelist');
            loadWhitelist();
            document.getElementById('add-to-whitelist').reset();
        } else {
            console.error('Failed to add player to whitelist:', responseData.error);
            alert('Failed to add player to whitelist: ' + (responseData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error adding to whitelist:', error);
        alert('Error adding to whitelist: ' + error.message);
    }
}

async function removeFromWhitelist(playerName) {
    if (currentUserRole > 5) {
        await alert('You do not have permission to remove from the whitelist');
        return;
    }
    const confirmed = await confirm(`Are you sure you want to remove ${playerName} from the whitelist?`);
    if (confirmed) {
        try {
            const response = await fetch(`/api/whitelist/${encodeURIComponent(playerName)}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                loadWhitelist();
            } else {
                await alert('Failed to remove player from whitelist');
            }
        } catch (error) {
            console.error('Error removing from whitelist:', error);
        }
    }
}

async function loadDemos() { 
    if (currentUserRole > 5) return;
    try {
      const response = await fetch('/api/all-demos');
      const demos = await response.json();
      const tbody = document.querySelector('#demo-table tbody');
      tbody.innerHTML = '';
  
      demos.forEach(demo => {
        const row = tbody.insertRow();
        let actionsHtml = '';
        if (currentUserRole <= 5) {
          actionsHtml += `<button onclick="editDemo(${demo.id})">Edit</button>`;
        }
        if (currentUserRole <= 2) {
          actionsHtml += `<button onclick="deleteDemo(${demo.id})">Delete</button>`;
        }
        row.innerHTML = `
          <td>${demo.game_type}</td>
          <td>${new Date(demo.date).toLocaleString()}</td>
          <td>${demo.name}</td>
          <td>${actionsHtml}</td>
        `;
      });
    } catch (error) {
      console.error('Error loading demos:', error);
    }
  }

async function uploadDemo(event) {
    event.preventDefault();
    if (currentUserRole > 4) {
        alert('You do not have permission to upload demos');
        return;
    }
    console.log('Upload demo function called');
    const formData = new FormData(event.target);
    try {
        const response = await fetch('/api/upload-demo', {
            method: 'POST',
            body: formData,
            credentials: 'include'
        });
        if (response.ok) {
            console.log('Demo uploaded successfully');
            loadDemos();
            event.target.reset();
        } else {
            const errorData = await response.json();
            console.error('Failed to upload demo:', errorData);
            alert('Failed to upload demo: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error uploading demo:', error);
        alert('Error uploading demo: ' + error.message);
    }
}

async function deleteDemo(demoId) {
    if (currentUserRole > 2) {
        await alert('You do not have permission to delete demos');
        return;
    }
    const confirmed = await confirm('Are you sure you want to delete this demo?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/demo/${demoId}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                loadDemos();
            } else {
                await alert('Failed to delete demo');
            }
        } catch (error) {
            console.error('Error deleting demo:', error);
        }
    }
}

function editDemo(demoId) {
    if (currentUserRole > 5) {
        alert('You do not have permission to edit demos');
        return;
    }
    console.log('Edit demo function called for demo ID:', demoId);
    try {
        fetch(`/api/demo/${demoId}`)
            .then(response => response.json())
            .then(demo => {
                document.getElementById('edit-demo-id').value = demo.id;
                document.getElementById('edit-demo-name').value = demo.name;
                document.getElementById('edit-game-type').value = demo.game_type;
                document.getElementById('edit-duration').value = demo.duration;
                document.getElementById('edit-players-json').value = demo.players;

                const playersDiv = document.getElementById('edit-players');
                playersDiv.innerHTML = '';
                
                const players = JSON.parse(demo.players);
                players.forEach((player, index) => {
                    playersDiv.innerHTML += `
                        <div>
                            <input type="text" id="edit-player${index+1}-name" value="${player.name}" placeholder="Player ${index+1} Name">
                            <input type="number" id="edit-player${index+1}-score" value="${player.score}" placeholder="Score">
                            <input type="text" id="edit-player${index+1}-territory" value="${player.territory}" placeholder="Territory">
                            <select id="edit-player${index+1}-team">
                                <option value="1" ${player.team == 1 ? 'selected' : ''}>Red</option>
                                <option value="2" ${player.team == 2 ? 'selected' : ''}>Green</option>
                                <option value="3" ${player.team == 3 ? 'selected' : ''}>Light Blue</option>
                                <option value="4" ${player.team == 4 ? 'selected' : ''}>Orange</option>
                                <option value="5" ${player.team == 5 ? 'selected' : ''}>Dark Blue</option>
                                <option value="6" ${player.team == 6 ? 'selected' : ''}>Yellow</option>
                                <option value="7" ${player.team == 7 ? 'selected' : ''}>Turq</option>
                                <option value="8" ${player.team == 8 ? 'selected' : ''}>Pink</option>
                                <option value="9" ${player.team == 9 ? 'selected' : ''}>White</option>
                                <option value="10" ${player.team == 10 ? 'selected' : ''}>Mint Green</option>
                            </select>
                        </div>
                    `;
                });

                document.getElementById('demo-edit').style.display = 'block';
            });
    } catch (error) {
        console.error('Error fetching demo details:', error);
        alert('Error fetching demo details: ' + error.message);
    }
}

async function saveEditDemo(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        alert('You do not have permission to save demo edits');
        return;
    }
    console.log('Save edit demo function called');
    const demoId = document.getElementById('edit-demo-id').value;
    const updatedDemo = {
        name: document.getElementById('edit-demo-name').value,
        game_type: document.getElementById('edit-game-type').value,
        duration: document.getElementById('edit-duration').value,
        players: []
    };

    let existingPlayers = [];
    try {
        existingPlayers = JSON.parse(document.getElementById('edit-players-json').value);
    } catch (error) {
        console.error('Error parsing existing players JSON:', error);
        alert('Error parsing existing players data. Please check the format.');
        return;
    }

    for (let i = 0; i < existingPlayers.length; i++) {
        const nameInput = document.getElementById(`edit-player${i+1}-name`);
        const scoreInput = document.getElementById(`edit-player${i+1}-score`);
        const territoryInput = document.getElementById(`edit-player${i+1}-territory`);
        const teamInput = document.getElementById(`edit-player${i+1}-team`);
        
        if (nameInput && scoreInput && territoryInput && teamInput) {
            existingPlayers[i].name = nameInput.value;
            existingPlayers[i].score = parseInt(scoreInput.value);
            existingPlayers[i].territory = territoryInput.value;
            existingPlayers[i].team = parseInt(teamInput.value);
        }
    }

    updatedDemo.players = JSON.stringify(existingPlayers);

    console.log('Sending update request with data:', updatedDemo);

    try {
        const response = await fetch(`/api/demo/${demoId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updatedDemo),
            credentials: 'include'
        });
        
        console.log('Response status:', response.status);
        
        const result = await response.json();
        console.log('Response data:', result);

        if (response.ok) {
            console.log('Demo updated successfully');
            loadDemos();
            cancelEditDemo();
        } else {
            console.error('Failed to update demo:', result);
            alert('Failed to update demo: ' + (result.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating demo:', error);
        alert('Error updating demo: ' + error.message);
    }
}

function cancelEditDemo() {
    document.getElementById('demo-edit').style.display = 'none';
    document.getElementById('edit-demo-form').reset();
}

async function uploadResource(event) {
    event.preventDefault();
    if (currentUserRole > 3) {
        alert('You do not have permission to upload resources');
        return;
    }
    const formData = new FormData(event.target);
    const platform = event.target.id === 'upload-windows-resource' ? 'windows' : 'linux';
    formData.append('platform', platform);

    try {
        const response = await fetch('/api/upload-resource', {
            method: 'POST',
            body: formData
        });
        if (response.ok) {
            alert(`${platform.charAt(0).toUpperCase() + platform.slice(1)} resource uploaded successfully`);
            loadResources();
            event.target.reset();
        } else {
            const errorData = await response.json();
            alert('Failed to upload resource: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error uploading resource:', error);
        alert('Error uploading resource: ' + error.message);
    }
}

function displayResourcesManagement(resources) {
    const tbody = document.querySelector('#resource-table tbody');
    tbody.innerHTML = ''; 
  
    resources.forEach(resource => {
        const row = tbody.insertRow();
        let actionsHtml = '';
        
        if (currentUserRole <= 4) {
            actionsHtml += `<button onclick="editResource(${resource.id})">Edit</button>`;
        }
        if (currentUserRole === 1) {
            actionsHtml += `<button onclick="deleteResource(${resource.id})">Delete</button>`;
        }
        
        row.innerHTML = `
            <td>${resource.name}</td>
            <td>${resource.version}</td>
            <td>${new Date(resource.date).toLocaleDateString()}</td>
            <td>${formatBytes(resource.size)}</td>
            <td>${resource.platform}</td>
            <td>${actionsHtml}</td>
        `;
    });
}

async function editResource(resourceId) {
    if (currentUserRole > 3) {
        alert('You do not have permission to edit resources');
        return;
    }
    try {
        const response = await fetch(`/api/resource/${resourceId}`);
        const resource = await response.json();
        document.getElementById('edit-resource-id').value = resource.id;
        document.getElementById('edit-resource-name').value = resource.name;
        document.getElementById('edit-resource-version').value = resource.version;
        document.getElementById('edit-resource-date').value = new Date(resource.date).toISOString().split('T')[0];
        document.getElementById('edit-resource-platform').value = resource.platform;
        document.getElementById('resource-edit').style.display = 'block';
    } catch (error) {
        console.error('Error fetching resource details:', error);
        alert('Error fetching resource details: ' + error.message);
    }
}

async function saveEditResource(event) {
    event.preventDefault();
    if (currentUserRole > 3) {
        alert('You do not have permission to save resource edits');
        return;
    }
    const resourceId = document.getElementById('edit-resource-id').value;
    const updatedResource = {
        name: document.getElementById('edit-resource-name').value,
        version: document.getElementById('edit-resource-version').value,
        date: document.getElementById('edit-resource-date').value,
        platform: document.getElementById('edit-resource-platform').value
    };
    try {
        const response = await fetch(`/api/resource/${resourceId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updatedResource)
        });
        if (response.ok) {
            alert('Resource updated successfully');
            loadResources();
            cancelEditResource();
        } else {
            const errorData = await response.json();
            alert('Failed to update resource: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating resource:', error);
        alert('Error updating resource: ' + error.message);
    }
}

function cancelEditResource() {
    document.getElementById('resource-edit').style.display = 'none';
    document.getElementById('edit-resource-form').reset();
}

async function deleteResource(resourceId) {
    if (currentUserRole !== 1) {
        await alert('You do not have permission to delete resources');
        return;
    }
    const confirmed = await confirm('Are you sure you want to delete this resource?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/resource/${resourceId}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                await alert('Resource deleted successfully');
                loadResources();
            } else {
                await alert('Failed to delete resource');
            }
        } catch (error) {
            console.error('Error deleting resource:', error);
            await alert('Error deleting resource: ' + error.message);
        }
    }
}

async function addMod(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        alert('You do not have permission to add mods');
        return;
    }
    const formData = new FormData(event.target);
    try {
        const response = await fetch('/api/mods', {
            method: 'POST',
            body: formData
        });
        if (response.ok) {
            alert('Mod added successfully');
            loadMods();
            event.target.reset();
        } else {
            const errorData = await response.json();
            alert('Failed to add mod: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error adding mod:', error);
        alert('Error adding mod: ' + error.message);
    }
}

async function saveEditMod(event) {
    event.preventDefault();
    if (currentUserRole > 5) {
        alert('You do not have permission to save mod edits');
        return;
    }
    const modId = document.getElementById('edit-mod-id').value;
    const formData = new FormData(event.target);
    try {
        const response = await fetch(`/api/mods/${modId}`, {
            method: 'PUT',
            body: formData
        });
        if (response.ok) {
            alert('Mod updated successfully');
            loadMods();
            cancelEditMod();
        } else {
            const errorData = await response.json();
            alert('Failed to update mod: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating mod:', error);
        alert('Error updating mod: ' + error.message);
    }
}

async function loadMods(type = '', sort = '') {
    if (currentUserRole > 5) return;
    try {
        let url = '/api/mods';
        if (type) url += `?type=${encodeURIComponent(type)}`;
        if (sort) url += `${type ? '&' : '?'}sort=${encodeURIComponent(sort)}`;
        
        const response = await fetch(url);
        const mods = await response.json();
        const tbody = document.querySelector('#mod-table tbody');
        tbody.innerHTML = '';
        mods.forEach(mod => {
            const row = tbody.insertRow();
            let actionsHtml = '';
            if (currentUserRole <= 5) {
                actionsHtml += `<button onclick="editMod(${mod.id})">Edit</button>`;
            }
            if (currentUserRole === 1) {
                actionsHtml += `<button onclick="deleteMod(${mod.id})">Delete</button>`;
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
    }
}

function scrollToTop() {
    const pageContainer = document.getElementById('page-container');
    pageContainer.classList.add('scrolling');
    
    setTimeout(() => {
        window.scrollTo({
            top: 0,
            behavior: 'instant'
        });
        
        pageContainer.classList.remove('scrolling');
    }, 500);
}

async function editMod(modId) {
    if (currentUserRole > 5) {
        alert('You do not have permission to edit mods');
        return;
    }
    try {
        const response = await fetch(`/api/mods/${modId}`);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const contentType = response.headers.get("content-type");
        if (!contentType || !contentType.includes("application/json")) {
            throw new TypeError("Oops, we haven't got JSON!");
        }
        const mod = await response.json();
        document.getElementById('edit-mod-id').value = mod.id;
        document.getElementById('edit-mod-name').value = mod.name;
        document.getElementById('edit-mod-type').value = mod.type;
        document.getElementById('edit-mod-creator').value = mod.creator;
        document.getElementById('edit-mod-version').value = mod.version;
        document.getElementById('edit-mod-compatibility').value = mod.compatibility;
        document.getElementById('edit-mod-release-date').value = new Date(mod.release_date).toISOString().split('T')[0];
        document.getElementById('edit-mod-description').value = mod.description;
        document.getElementById('mod-edit').style.display = 'block';
        scrollToTop(800);
        document.getElementById('edit-mod-name').focus();
    } catch (error) {
        console.error('Error fetching mod details:', error);
        console.log('Response:', error.response);
        alert('Error fetching mod details: ' + error.message);
    }
}

async function deleteMod(modId) {
    if (currentUserRole > 2) {
        await alert('You do not have permission to delete mods');
        return;
    }
    const confirmed = await confirm('Are you sure you want to delete this mod?');
    if (confirmed) {
        try {
            const response = await fetch(`/api/mods/${modId}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                await alert('Mod deleted successfully');
                loadMods();
            } else {
                await alert('Failed to delete mod');
            }
        } catch (error) {
            console.error('Error deleting mod:', error);
            await alert('Error deleting mod: ' + error.message);
        }
    }
}

function cancelEditMod() {
    document.getElementById('mod-edit').style.display = 'none';
    document.getElementById('edit-mod-form').reset();
}

async function loadUsers() {
    if (currentUserRole > 2) return;
    try {
        const response = await fetch('/api/users');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const users = await response.json();
        console.log('Loaded users:', users.length);
        const tbody = document.querySelector('#user-table tbody');
        tbody.innerHTML = '';
        users.forEach(user => {
            const row = tbody.insertRow();
            let actionsHtml = '';
            if (currentUserRole <= 2) {
                actionsHtml += `<button onclick="editUser(${user.id})">Edit</button>`;
            }
            if (currentUserRole === 1) {
                actionsHtml += `<button onclick="deleteUser(${user.id})">Delete</button>`;
            }
            row.innerHTML = `
                <td>${user.username}</td>
                <td>${user.email}</td>
                <td>${getUserRoleName(user.role)}</td>
                <td>${actionsHtml}</td>
            `;
        });
    } catch (error) {
        console.error('Error loading users:', error);
    }
}

async function editUser(userId) {
    if (currentUserRole > 2) {
        alert('You do not have permission to edit users');
        return;
    }
    try {
        const response = await fetch(`/api/users/${userId}`);
        const user = await response.json();
        document.getElementById('edit-user-id').value = user.id;
        document.getElementById('edit-user-username').value = user.username;
        document.getElementById('edit-user-email').value = user.email;
        document.getElementById('edit-user-role').value = user.role;
        document.getElementById('edit-user').style.display = 'block';
    } catch (error) {
        console.error('Error fetching user details:', error);
        alert('Error fetching user details: ' + error.message);
    }
}

function getUserRoleName(roleNumber) {
    const roles = {
        1: 'Super Admin',
        2: 'Admin',
        3: 'Super Mod',
        4: 'Moderator',
        5: 'Helper',
        6: 'User'
    };
    return roles[roleNumber] || 'Unknown';
}

async function saveEditUser(event) {
    event.preventDefault();
    if (currentUserRole > 2) {
        alert('You do not have permission to save user edits');
        return;
    }
    const userId = document.getElementById('edit-user-id').value;
    const updatedUser = {
        username: document.getElementById('edit-user-username').value,
        email: document.getElementById('edit-user-email').value,
        role: parseInt(document.getElementById('edit-user-role').value)
    };
    try {
        const response = await fetch(`/api/users/${userId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updatedUser)
        });
        if (response.ok) {
            alert('User updated successfully');
            loadUsers();
            document.getElementById('edit-user').style.display = 'none';
        } else {
            const errorData = await response.json();
            alert('Failed to update user: ' + (errorData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating user:', error);
        alert('Error updating user: ' + error.message);
    }
}

async function deleteUser(userId) {
    if (currentUserRole !== 1) {
        await alert('Only superadmins can delete users');
        return;
    }
    const confirmed = await confirm('Are you sure you want to delete this user? This action cannot be undone.');
    if (confirmed) {
        try {
            const response = await fetch(`/api/users/${userId}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                await alert('User deleted successfully');
                loadUsers();
            } else {
                await alert('Failed to delete user');
            }
        } catch (error) {
            console.error('Error deleting user:', error);
            await alert('Error deleting user: ' + error.message);
        }
    }
}

function cancelEditUser() {
    document.getElementById('edit-user').style.display = 'none';
    document.getElementById('edit-user-form').reset();
}

async function deleteUser(userId) {
    if (currentUserRole !== 1) {
        alert('Only superadmins can delete users');
        return;
    }
    if (confirm('Are you sure you want to delete this user? This action cannot be undone.')) {
        try {
            const response = await fetch(`/api/users/${userId}`, {
                method: 'DELETE'
            });
            if (response.ok) {
                alert('User deleted successfully');
                loadUsers();
            } else {
                alert('Failed to delete user');
            }
        } catch (error) {
            console.error('Error deleting user:', error);
            alert('Error deleting user: ' + error.message);
        }
    }
}

function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('mod-type-filter').addEventListener('change', (e) => {
        loadMods(e.target.value);
    });

    document.getElementById('mod-sort').addEventListener('change', (e) => {
        loadMods(document.getElementById('mod-type-filter').value, e.target.value);
    });

    if (document.getElementById('add-mod-form')) {
        document.getElementById('add-mod-form').addEventListener('submit', addMod);
    }
    if (document.getElementById('edit-mod-form')) {
        document.getElementById('edit-mod-form').addEventListener('submit', saveEditMod);
    }
    if (document.getElementById('edit-user-form')) {
        document.getElementById('edit-user-form').addEventListener('submit', saveEditUser);
    }
});

document.addEventListener('DOMContentLoaded', function() {
    console.log('Admin page loaded, setting up mobile menu');
    setupMobileMenu();
    setActiveNavItem();
});

initializeAdminPanel();