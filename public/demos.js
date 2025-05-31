let allDemos = [];
let currentPage = 1;
let totalPages = 1;
let currentSort = 'latest';
let currentServerFilter = '';

function initializeDemos() {
    const urlParams = new URLSearchParams(window.location.search);
    currentPage = parseInt(urlParams.get('page') || '1');
    currentSort = urlParams.get('sort') || 'latest';
    currentServerFilter = urlParams.get('server') || '';
    
    // Set initial values for form elements
    const serverFilter = document.getElementById('server-filter');
    if (serverFilter && currentServerFilter) {
        serverFilter.value = currentServerFilter;
    }

    const sortSelect = document.querySelector('.sort-container select');
    if (sortSelect && currentSort) {
        sortSelect.value = currentSort;
    }

    const playerSearchInput = document.getElementById('player-search-input');
    const playerName = urlParams.get('playerName');
    if (playerSearchInput && playerName) {
        playerSearchInput.value = playerName;
    }

    updateDemoList(playerName);
}

function updateDemoList(playerName = '') {
    const timestamp = new Date().getTime();
    const serverFilter = currentServerFilter;

    let apiUrl = `/api/demos?page=${currentPage}&sortBy=${currentSort}`;
    if (playerName) {
        apiUrl += `&playerName=${encodeURIComponent(playerName)}`;
    }
    if (serverFilter) {
        apiUrl += `&serverName=${encodeURIComponent(serverFilter)}`;
    }
    apiUrl += `&t=${timestamp}`;

    fetch(apiUrl)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            allDemos = data.demos;
            totalPages = data.totalPages;
            currentPage = data.currentPage;

            const gamesPlayed = document.getElementById('games-played');
            if (gamesPlayed) {
                gamesPlayed.textContent = `Total Games Played: ${data.totalDemos}`;
            }

            displayDemos(allDemos);
            updatePagination();
            updateURL();
        })
        .catch(error => {
            console.error('Failed to fetch demo data:', error);
            alert('Failed to fetch demo data. Please try again later.');
        });
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


function createDemoCard(demo) {
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
            parsedPlayers.forEach((player, index) => {
                const teamId = player.team || 0;
                if (!teamScores[teamId]) {
                    teamScores[teamId] = 0;
                }
                teamScores[teamId] += player.score;
                if (player.score > highestScore) {
                    highestScore = player.score;
                }
                // Add profileUrl back in for each player
                player.profileUrl = demo[`player${index + 1}_name_profileUrl`] || null;
            });
        } catch (e) {
            console.error('Error parsing players JSON:', e);
        }
    }

    // Sort teams by total score
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
                <p class="winning-team-info" style="text-align: right; color: #b8b8b8;">
                    ${winningMessage}
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
        // Sort players within each team by their individual score
        sortedTeams.forEach(([teamId, _]) => {
            const teamColor = teamColors[teamId]?.color || '#00bf00';
            parsedPlayers
                .filter(player => player.team === Number(teamId))
                .sort((a, b) => b.score - a.score)
                .forEach(player => {
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
        });
    } else {
        demoCardHtml += '<tr><td colspan="3">No player data available</td></tr>';
    }

    demoCardHtml += `
            </table>
            <div class="demo-actions">
                <a href="/api/download/${demo.name}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                <button class="btn-report" onclick="showReportOptions(${demo.id}, event)">Report</button>
                <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
            </div>
        </div>`;

    demoCard.innerHTML = demoCardHtml;

    return demoCard;
}



function loadDemos(demoList) {
    demoList.forEach(demo => {
        const demoCard = createDemoCard(demo);
        document.querySelector('#demo-container').appendChild(demoCard);
    });
}

function showReportOptions(demoId, event) {
    event.preventDefault();
    event.stopPropagation();

    if (!isUserLoggedIn()) {
        window.location.href = '/login';
        return;
    }

    const options = [
        'Incorrect scores',
        'Incorrect territories',
        'Incorrect names',
        'Incorrect team colors',
        'Missing data'
    ];

    const dropdown = document.createElement('div');
    dropdown.className = 'report-dropdown';
    
    options.forEach(option => {
        const button = document.createElement('button');
        button.textContent = option;
        button.onclick = () => confirmReport(demoId, option);
        dropdown.appendChild(button);
    });

    const existingDropdown = document.querySelector('.report-dropdown');
    if (existingDropdown) {
        existingDropdown.remove();
    }

    const reportButton = event.target;
    const demoCard = reportButton.closest('.demo-card');
    demoCard.style.position = 'relative';
    demoCard.appendChild(dropdown);

    document.addEventListener('click', closeDropdown);
}

function closeDropdown(event) {
    if (!event.target.matches('.btn-report')) {
        const dropdowns = document.getElementsByClassName('report-dropdown');
        for (let i = 0; i < dropdowns.length; i++) {
            dropdowns[i].remove();
        }
        document.removeEventListener('click', closeDropdown);
    }
}

async function confirmReport(demoId, reportType) {
    const confirmed = await confirm(`Are you sure you want to report this demo for ${reportType}?`);
    if (confirmed) {
        submitReport(demoId, reportType);
    }
}

async function submitReport(demoId, reportType) {
    try {
        const response = await fetch('/api/report-demo', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ demoId, reportType }),
        });

        if (response.ok) {
            alert('Report successfully sent to the team!');
        } else {
            throw new Error('You need to be logged in to submit a demo report.');
        }
    } catch (error) {
        console.error('Error submitting report:', error);
        alert('You need to be logged in to submit a demo report.');
    }
}


function updatePagination() {
    const paginationContainer = document.getElementById('pagination');
    if (!paginationContainer) return;
    paginationContainer.innerHTML = '';

    if (currentPage > 1) {
        const prevButton = createPaginationButton('Prev', currentPage - 1);
        paginationContainer.appendChild(prevButton);
    }

    let startPage = Math.max(1, currentPage - 4);
    let endPage = Math.min(totalPages, startPage + 8);
    
    if (totalPages > 9 && endPage - startPage < 8) {
        startPage = Math.max(1, endPage - 8);
    }

    if (startPage > 1) {
        const firstButton = createPaginationButton('1', 1);
        paginationContainer.appendChild(firstButton);
        if (startPage > 2) {
            const ellipsis = document.createElement('span');
            ellipsis.textContent = '...';
            ellipsis.className = 'pagination-ellipsis';
            paginationContainer.appendChild(ellipsis);
        }
    }

    for (let i = startPage; i <= endPage; i++) {
        const pageButton = createPaginationButton(i.toString(), i);
        if (i === currentPage) {
            pageButton.classList.add('active');
            pageButton.disabled = true;
        }
        paginationContainer.appendChild(pageButton);
    }

    if (endPage < totalPages) {
        if (endPage < totalPages - 1) {
            const ellipsis = document.createElement('span');
            ellipsis.textContent = '...';
            ellipsis.className = 'pagination-ellipsis';
            paginationContainer.appendChild(ellipsis);
        }
        const lastButton = createPaginationButton(totalPages.toString(), totalPages);
        paginationContainer.appendChild(lastButton);
    }

    if (currentPage < totalPages) {
        const nextButton = createPaginationButton('Next', currentPage + 1);
        paginationContainer.appendChild(nextButton);
    }

    if (totalPages > 9) {
        const customPageContainer = document.createElement('div');
        customPageContainer.className = 'custom-page-container';
        
        const customPageInput = document.createElement('input');
        customPageInput.type = 'number';
        customPageInput.min = 1;
        customPageInput.max = totalPages;
        customPageInput.placeholder = 'Go to page...';
        customPageInput.className = 'custom-page-input';
        
        const goButton = document.createElement('button');
        goButton.textContent = 'Go';
        goButton.className = 'custom-page-go';
        goButton.onclick = () => {
            const pageNum = parseInt(customPageInput.value);
            if (pageNum >= 1 && pageNum <= totalPages) {
                const url = new URL(window.location);
                url.searchParams.set('page', pageNum);
                window.location.href = url.toString(); // Refresh the page
            } else {
                alert(`Please enter a page number between 1 and ${totalPages}`);
            }
        };

        customPageInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                goButton.click();
            }
        });

        customPageContainer.appendChild(customPageInput);
        customPageContainer.appendChild(goButton);
        paginationContainer.appendChild(customPageContainer);
    }
}

function createPaginationButton(text, page) {
    const button = document.createElement('button');
    button.textContent = text;
    button.addEventListener('click', () => {
        const url = new URL(window.location);
        url.searchParams.set('page', page);
        window.location.href = url.toString(); // Refresh the page
    });
    return button;
}

function changePage(newPage) {
    if (newPage >= 1 && newPage <= totalPages) {
        currentPage = newPage;
        updateDemoList();
    }
}

function updateURL() {
    const url = new URL(window.location);
    url.searchParams.set('page', currentPage);
    url.searchParams.set('sort', currentSort);
    if (currentServerFilter) {
        url.searchParams.set('server', currentServerFilter);
    } else {
        url.searchParams.delete('server');
    }
    window.history.pushState({}, '', url);
}

function performPlayerSearch() {
    const playerSearchInput = document.getElementById('player-search-input');
    if (!playerSearchInput) return;

    const playerName = playerSearchInput.value.trim();
    const url = new URL(window.location);
    
    if (playerName) {
        url.searchParams.set('playerName', playerName);
    } else {
        url.searchParams.delete('playerName');
    }
    
    url.searchParams.set('page', '1');
    window.location.href = url.toString(); // Refresh the page
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

document.addEventListener('DOMContentLoaded', () => {
    initializeDemos();

    // Event listener for search button
    const searchButton = document.getElementById('search-button2');
    if (searchButton) {
        searchButton.addEventListener('click', performPlayerSearch);
    }

    // Event listener for search input enter key
    const playerSearchInput = document.getElementById('player-search-input');
    if (playerSearchInput) {
        playerSearchInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                performPlayerSearch();
            }
        });
    }

    // Event listener for sort selection
    const sortSelect = document.querySelector('.sort-container select');
    if (sortSelect) {
        sortSelect.addEventListener('change', (e) => {
            const url = new URL(window.location);
            url.searchParams.set('sort', e.target.value);
            url.searchParams.set('page', '1');
            window.location.href = url.toString(); // Refresh the page
        });
    }

    // Event listener for server filter
    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) {
        serverFilter.addEventListener('change', (e) => {
            const url = new URL(window.location);
            if (e.target.value) {
                url.searchParams.set('server', e.target.value);
            } else {
                url.searchParams.delete('server');
            }
            url.searchParams.set('page', '1');
            window.location.href = url.toString(); // Refresh the page
        });
    }
});


window.addEventListener('popstate', () => {
    window.location.reload(); 
    window.updateDemoList = updateDemoList;
    window.performPlayerSearch = performPlayerSearch;
});

