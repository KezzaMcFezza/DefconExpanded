let allDemos = [];
let currentPage = 1;
let totalPages = 1;
let currentSort = 'latest';
let currentServerFilter = '';
window.toggleSpectators = toggleSpectators;

const filterState = {
    territories: new Set(),
    players: [],
    combineMode: false,
    scoreFilter: '',
    durationFilter: '',
    scoreDifferenceFilter: '',
    dateRange: {
        start: null,
        end: null
    },
    gamesPlayed: ''
};

const territoryMapping = {
    'na': 'North America',
    'sa': 'South America', 
    'eu': 'Europe',
    'ru': 'Russia',
    'af': 'Africa',
    'as': 'Asia',
    'au': 'Australasia',
    'we': 'West Asia',
    'ea': 'East Asia',
    'ant': 'Antartica',
    'naf': 'North Africa',
    'saf': 'South Africa'
};

function initializeDemos() {
    const urlParams = new URLSearchParams(window.location.search);
    currentPage = parseInt(urlParams.get('page') || '1');
    currentSort = urlParams.get('sort') || 'latest';
    currentServerFilter = urlParams.get('server') || '';
    
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

    if (urlParams.get('territories')) {
        const savedTerritories = urlParams.get('territories').split(',');
        savedTerritories.forEach(territory => {
            const territoryId = Object.entries(territoryMapping)
                .find(([id, name]) => name === territory)?.[0];
            if (territoryId) {
                const checkbox = document.querySelector(`#${territoryId}`);
                if (checkbox) {
                    checkbox.checked = true;
                    filterState.territories.add(territoryId);
                }
            }
        });
    }

    if (urlParams.get('players')) {
        const savedPlayers = urlParams.get('players').split(',');
        const playerInputs = document.querySelectorAll('.player-input');
        savedPlayers.forEach((player, index) => {
            if (playerInputs[index]) {
                playerInputs[index].value = player;
                filterState.players[index] = player;
            }
        });
    }

    const combineMode = urlParams.get('combineMode') === 'true';
    const combineModeCheckbox = document.querySelector('.combine-territory-checkbox');
    if (combineModeCheckbox) {
        combineModeCheckbox.checked = combineMode;
        filterState.combineMode = combineMode;
    }

    const scoreFilter = urlParams.get('scoreFilter');
    if (scoreFilter) {
        const scoreFilterSelect = document.querySelector('.score-filter');
        if (scoreFilterSelect) {
            scoreFilterSelect.value = scoreFilter;
            filterState.scoreFilter = scoreFilter;
        }
    }

    const durationFilter = urlParams.get('gameDuration');
    if (durationFilter) {
        const durationSelect = document.querySelector('.duration-select');
        if (durationSelect) {
            durationSelect.value = durationFilter;
            filterState.durationFilter = durationFilter;
        }
    }

    const scoreDifferenceFilter = urlParams.get('scoreDifference');
    if (scoreDifferenceFilter) {
        const scoreDiffSelect = document.querySelector('.score-difference-select');
        if (scoreDiffSelect) {
            scoreDiffSelect.value = scoreDifferenceFilter;
            filterState.scoreDifferenceFilter = scoreDifferenceFilter;
        }
    }

    const dateInputs = document.querySelectorAll('.date-input');
    if (urlParams.get('startDate') && dateInputs[0]) {
        dateInputs[0].value = urlParams.get('startDate');
        filterState.dateRange.start = urlParams.get('startDate');
    }
    if (urlParams.get('endDate') && dateInputs[1]) {
        dateInputs[1].value = urlParams.get('endDate');
        filterState.dateRange.end = urlParams.get('endDate');
    }

    const gamesPlayed = urlParams.get('gamesPlayed');
    if (gamesPlayed) {
        const gamesPlayedSelect = document.querySelector('.games-played-filter');
        if (gamesPlayedSelect) {
            gamesPlayedSelect.value = gamesPlayed;
            filterState.gamesPlayed = gamesPlayed;
        }
    }

    const hasAdvancedFilters = [
        'territories', 'combineMode', 'players', 'scoreFilter',
        'gameDuration', 'scoreDifference', 'startDate', 'endDate',
        'gamesPlayed'
    ].some(param => urlParams.has(param));

    if (hasAdvancedFilters) {
        const advancedFilters = document.getElementById('advancedFilters');
        const toggleButton = document.getElementById('advancedFiltersToggle');
        if (advancedFilters && toggleButton) {
            advancedFilters.classList.add('show');
            toggleButton.classList.add('active');
        }
    }

    updateDemoList(playerName);
}

function updateDemoList(playerName = '') {
    const timestamp = new Date().getTime();
    const serverFilter = currentServerFilter;

    const queryParams = new URLSearchParams();
    queryParams.set('page', currentPage);
    queryParams.set('sortBy', currentSort);
    
    if (playerName) {
        queryParams.set('playerName', playerName);
    }
    if (serverFilter) {
        queryParams.set('serverName', serverFilter);
    }

    const validPlayers = filterState.players.filter(player => player && player.trim());
    if (validPlayers.length > 0) {
        queryParams.set('players', validPlayers.join(','));
    }

    if (filterState.territories.size > 0) {
        const territoryNames = Array.from(filterState.territories)
            .map(id => territoryMapping[id])
            .filter(Boolean);
        queryParams.set('territories', territoryNames.join(','));
        queryParams.set('combineMode', filterState.combineMode);
    }

    if (filterState.scoreFilter) queryParams.set('scoreFilter', filterState.scoreFilter);
    if (filterState.durationFilter) queryParams.set('gameDuration', filterState.durationFilter);
    if (filterState.scoreDifferenceFilter) queryParams.set('scoreDifference', filterState.scoreDifferenceFilter);
    if (filterState.dateRange.start) queryParams.set('startDate', filterState.dateRange.start);
    if (filterState.dateRange.end) queryParams.set('endDate', filterState.dateRange.end);
    if (filterState.gamesPlayed) queryParams.set('gamesPlayed', filterState.gamesPlayed);

    queryParams.set('t', timestamp);

    fetch(`/api/demos?${queryParams.toString()}`)
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
            parsedPlayers = JSON.parse(demo.players);
            usingAlliances = parsedPlayers.some(player => player.alliance !== undefined);
            
            const allianceColors = isExpandedGame ? expandedAllianceColors : vanillaAllianceColors;
            const colorSystem = usingAlliances ? allianceColors : teamColors;

            parsedPlayers.forEach((player, index) => {
                const groupId = usingAlliances ? player.alliance : player.team;
                
                if (!groupScores[groupId]) {
                    groupScores[groupId] = 0;
                }
                
                groupScores[groupId] += player.score;
                
                if (player.score > highestScore) {
                    highestScore = player.score;
                }
                
                player.profileUrl = demo[`player${index + 1}_name_profileUrl`] || null;
            });
        } catch (e) {
            console.error('Error parsing players JSON:', e);
        }
    }

    const sortedGroups = Object.entries(groupScores).sort((a, b) => b[1] - a[1]);
    let winningMessage = 'No winning team determined.';
    const colorSystem = usingAlliances ? (isExpandedGame ? expandedAllianceColors : vanillaAllianceColors) : teamColors;

    if (parsedPlayers.length > 0) {
        const playersPerGroup = {};
        parsedPlayers.forEach(player => {
            const groupId = usingAlliances ? player.alliance : player.team;
            playersPerGroup[groupId] = (playersPerGroup[groupId] || 0) + 1;
        });
        
        const uniqueGroups = Object.keys(playersPerGroup).length;
        const isAllSoloPlayers = Object.values(playersPerGroup).every(count => count === 1);

        if (uniqueGroups === 2) {
            const [winningGroupId, winningScore] = sortedGroups[0];
            const [secondGroupId, secondScore] = sortedGroups[1];
            const scoreDifference = winningScore - secondScore;
            const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
            const secondGroupName = colorSystem[secondGroupId]?.name || `Team ${secondGroupId}`;
            const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';
            const secondGroupColor = colorSystem[secondGroupId]?.color || '#b8b8b8';

            winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won against <span style="color: ${secondGroupColor}">${secondGroupName}</span> by ${scoreDifference} points.`;
        }
        else if (isAllSoloPlayers) {
            const sortedPlayers = [...parsedPlayers].sort((a, b) => b.score - a.score);
            const winner = sortedPlayers[0];
            const secondPlace = sortedPlayers[1];
            const scoreDifference = winner.score - secondPlace.score;
            const winnerGroupId = usingAlliances ? winner.alliance : winner.team;
            const secondPlaceGroupId = usingAlliances ? secondPlace.alliance : secondPlace.team;
            const winnerColor = colorSystem[winnerGroupId]?.color || '#b8b8b8';
            const secondPlaceColor = colorSystem[secondPlaceGroupId]?.color || '#b8b8b8';

            winningMessage = `<span style="color: ${winnerColor}">${winner.name}</span> won with ${scoreDifference} more points than <span style="color: ${secondPlaceColor}">${secondPlace.name}</span>.`;
        }
        else {
            const [winningGroupId, winningScore] = sortedGroups[0];
            const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
            const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';
            
            if (uniqueGroups >= 3) {
                winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won with ${winningScore} points.`;
            }
        }
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
        <button class="btn-report" onclick="showReportOptions(${demo.id}, event)">Report</button>
        ${window.userRole !== undefined && window.userRole <= 5 ? `
            <span style="color: #888888b0; text-shadow: unset; text-shadow: 0px 0px 0px currentColor; margin-left: auto;">Demo ID: ${demo.id}</span>
        ` : ''}
        <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
    </div>
  </div>`;

    demoCard.innerHTML = demoCardHtml;
    return demoCard;
}

function toggleSpectators(button) {
    const spectatorsList = button.nextElementSibling;
    const isHidden = !spectatorsList.classList.contains('show');
    
    spectatorsList.classList.toggle('show', isHidden);
    button.innerHTML = isHidden ? 
        `<i class="fas fa-eye-slash"></i> Hide Spectators` :
        `<i class="fas fa-eye"></i> Show Spectators`;
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
                window.location.href = url.toString();
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
        window.location.href = url.toString();
    });
    return button;
}

function changePage(newPage) {
    if (newPage >= 1 && newPage <= totalPages) {
        const url = new URL(window.location);
        url.searchParams.set('page', newPage);
        window.location.href = url.toString();
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

    if (filterState.territories.size > 0) {
        const territoryNames = Array.from(filterState.territories)
            .map(id => territoryMapping[id])
            .filter(Boolean);
        url.searchParams.set('territories', territoryNames.join(','));
        url.searchParams.set('combineMode', filterState.combineMode);
    }

    if (filterState.scoreFilter) url.searchParams.set('scoreFilter', filterState.scoreFilter);
    if (filterState.durationFilter) url.searchParams.set('gameDuration', filterState.durationFilter);
    if (filterState.scoreDifferenceFilter) url.searchParams.set('scoreDifference', filterState.scoreDifferenceFilter);
    if (filterState.dateRange.start) url.searchParams.set('startDate', filterState.dateRange.start);
    if (filterState.dateRange.end) url.searchParams.set('endDate', filterState.dateRange.end);
    if (filterState.gamesPlayed) url.searchParams.set('gamesPlayed', filterState.gamesPlayed);

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
    window.location.href = url.toString();
}

function resetFilters() {
    const url = new URL(window.location);
    const hasAppliedFilters = [
        'territories', 'players', 'combineMode', 'scoreFilter',
        'gameDuration', 'scoreDifference', 'startDate', 'endDate',
        'gamesPlayed', 'playerName'
    ].some(param => url.searchParams.has(param));

    document.querySelectorAll('.territory-checkbox').forEach(checkbox => {
        checkbox.checked = false;
        filterState.territories.clear();
    });

    const combineCheckbox = document.querySelector('.combine-territory-checkbox');
    if (combineCheckbox) {
        combineCheckbox.checked = false;
        filterState.combineMode = false;
    }

    const scoreFilter = document.querySelector('.score-filter');
    if (scoreFilter) {
        scoreFilter.value = '';
        filterState.scoreFilter = '';
    }

    const durationSelect = document.querySelector('.duration-select');
    if (durationSelect) {
        durationSelect.value = '';
        filterState.durationFilter = '';
    }

    const scoreDiffSelect = document.querySelector('.score-difference-select');
    if (scoreDiffSelect) {
        scoreDiffSelect.value = '';
        filterState.scoreDifferenceFilter = '';
    }

    document.querySelectorAll('.date-input').forEach((input, index) => {
        input.value = '';
        if (index === 0) {
            filterState.dateRange.start = null;
        } else {
            filterState.dateRange.end = null;
        }
    });

    document.querySelectorAll('.player-input').forEach((input, index) => {
        input.value = '';
        filterState.players[index] = '';
    });

    const gamesPlayedSelect = document.querySelector('.games-played-filter');
    if (gamesPlayedSelect) {
        gamesPlayedSelect.value = '';
        filterState.gamesPlayed = '';
    }

    const playerSearchInput = document.getElementById('player-search-input');
    if (playerSearchInput) {
        playerSearchInput.value = '';
    }

    if (hasAppliedFilters) {
        const newUrl = new URL(window.location);
        for (const param of newUrl.searchParams.keys()) {
            if (param !== 'sort' && param !== 'page' && param !== 'server') {
                newUrl.searchParams.delete(param);
            }
        }
        window.location.href = newUrl.toString();
    }
}

document.addEventListener('DOMContentLoaded', () => {
    initializeDemos();

    const resetFiltersBtn = document.querySelector('.reset-filters-btn');
    if (resetFiltersBtn) {
        resetFiltersBtn.addEventListener('click', resetFilters);
    }

    const searchButton = document.getElementById('search-button2');
    if (searchButton) {
        searchButton.addEventListener('click', performPlayerSearch);
    }

    const playerSearchInput = document.getElementById('player-search-input');
    if (playerSearchInput) {
        playerSearchInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                performPlayerSearch();
            }
        });
    }

    const sortSelect = document.querySelector('.sort-container select');
    if (sortSelect) {
        sortSelect.addEventListener('change', (e) => {
            const url = new URL(window.location);
            url.searchParams.set('sort', e.target.value);
            url.searchParams.set('page', '1');
            window.location.href = url.toString(); 
        });
    }

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
            window.location.href = url.toString(); 
        });
    }

    document.querySelectorAll('.territory-checkbox').forEach(checkbox => {
        checkbox.addEventListener('change', (e) => {
            if (e.target.checked) {
                filterState.territories.add(e.target.id);
            } else {
                filterState.territories.delete(e.target.id);
            }
        });
    });

    document.querySelector('.combine-territory-checkbox')?.addEventListener('change', (e) => {
        filterState.combineMode = e.target.checked;
    });

    document.querySelector('.score-filter')?.addEventListener('change', (e) => {
        filterState.scoreFilter = e.target.value;
        if (e.target.value) {
            document.querySelector('.score-difference-select').value = '';
            document.querySelector('.duration-select').value = '';
            filterState.scoreDifferenceFilter = '';
            filterState.durationFilter = '';
        }
    });

    document.querySelector('.duration-select')?.addEventListener('change', (e) => {
        filterState.durationFilter = e.target.value;
        if (e.target.value) {
            document.querySelector('.score-filter').value = '';
            document.querySelector('.score-difference-select').value = '';
            filterState.scoreFilter = '';
            filterState.scoreDifferenceFilter = '';
        }
    });

    document.querySelector('.score-difference-select')?.addEventListener('change', (e) => {
        filterState.scoreDifferenceFilter = e.target.value;
        if (e.target.value) {
            document.querySelector('.score-filter').value = '';
            document.querySelector('.duration-select').value = '';
            filterState.scoreFilter = '';
            filterState.durationFilter = '';
        }
    });

    document.querySelectorAll('.player-input').forEach((input, index) => {
        input.addEventListener('input', (e) => {
            filterState.players[index] = e.target.value.trim();
        });
    });

    document.querySelectorAll('.date-input').forEach((input, index) => {
        input.addEventListener('change', (e) => {
            if (index === 0) {
                filterState.dateRange.start = e.target.value;
            } else {
                filterState.dateRange.end = e.target.value;
            }
        });
    });

    document.querySelector('.games-played-filter')?.addEventListener('change', (e) => {
        filterState.gamesPlayed = e.target.value;
    });

    document.querySelector('.apply-filters-btn')?.addEventListener('click', () => {
        const url = new URL(window.location);
        
        url.searchParams.set('page', '1');

        const validPlayers = filterState.players.filter(player => player && player.trim());
        if (validPlayers.length > 0) {
            url.searchParams.set('players', validPlayers.join(','));
        } else {
            url.searchParams.delete('players');
        }
        
        if (filterState.territories.size > 0) {
            const territoryNames = Array.from(filterState.territories)
                .map(id => territoryMapping[id])
                .filter(Boolean);
            url.searchParams.set('territories', territoryNames.join(','));
            url.searchParams.set('combineMode', filterState.combineMode);
        } else {
            url.searchParams.delete('territories');
            url.searchParams.delete('combineMode');
        }

        if (filterState.scoreFilter) {
            url.searchParams.set('scoreFilter', filterState.scoreFilter);
        } else {
            url.searchParams.delete('scoreFilter');
        }

        if (filterState.durationFilter) {
            url.searchParams.set('gameDuration', filterState.durationFilter);
        } else {
            url.searchParams.delete('gameDuration');
        }

        if (filterState.scoreDifferenceFilter) {
            url.searchParams.set('scoreDifference', filterState.scoreDifferenceFilter);
        } else {
            url.searchParams.delete('scoreDifference');
        }

        if (filterState.dateRange.start) {
            url.searchParams.set('startDate', filterState.dateRange.start);
        } else {
            url.searchParams.delete('startDate');
        }

        if (filterState.dateRange.end) {
            url.searchParams.set('endDate', filterState.dateRange.end);
        } else {
            url.searchParams.delete('endDate');
        }

        if (filterState.gamesPlayed) {
            url.searchParams.set('gamesPlayed', filterState.gamesPlayed);
        } else {
            url.searchParams.delete('gamesPlayed');
        }

        window.location.href = url.toString();
    });
});

window.addEventListener('popstate', () => {
    window.location.reload();
    window.performPlayerSearch = performPlayerSearch;
});

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