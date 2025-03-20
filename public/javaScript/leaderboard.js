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
//Last Edited 03-03-2025


const leaderboardFilters = {
    serverName: '',
    sortBy: 'wins',
    startDate: '',
    endDate: '',
    minGames: '1',
    playlist: ''
};


const serverPlaylists = {
    all: { name: "All Servers", servers: [] },
    oneVsOne: {
        name: "1v1 Servers",
        servers: [
            "New Player Server",
            "New Player Server - Training Game",
            "DefconExpanded | 1v1 | Totally Random",
            "DefconExpanded | 1V1 | Best Setups Only!",
            "DefconExpanded | 1v1 | AF vs AS | Totally Random",
            "DefconExpanded | 1v1 | EU vs SA | Totally Random",
            "DefconExpanded | 1v1 | Default",
            "MURICON | 1v1 Default | 2.8.15"
        ]
    },
    twoVsTwo: {
        name: "2v2 Servers",
        servers: [
            "DefconExpanded | 2v2 | Totally Random",
            "DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random",
            "Mojo's 2v2 Arena - Quick Victory",
            "Sony and Hoov's Hideout"
        ]
    },
    Tournament: {
        name: "2v2 Tournament",
        servers: [
            "2v2 Tournament"
        ]
    },
    threeVsThree: {
        name: "3v3 Servers",
        servers: [
            "DefconExpanded | 3v3 | Totally Random"
        ]
    },
    fourVsFour: {
        name: "4v4 Servers",
        servers: [
            "DefconExpanded | 4V4 | Totally Random"
        ]
    },
    ffa: {
        name: "Free For All",
        servers: [
            "DefconExpanded | Free For All | Random Cities",
            "DefconExpanded | 8 Player | Diplomacy",
            "DefconExpanded | 10 Player | Diplomacy"
        ]
    }
};


const firstSeason = {
    name: 'The Beginning',
    displayName: 'The Beginning Y1S1',
    startDate: '2024-08-27',
    endDate: '2025-02-26',
    year: 1,
    season: 1
};



const seasonBoundaries = [
    { startMonth: 2, startDay: 27, endMonth: 4, endDay: 26 },
    { startMonth: 4, startDay: 27, endMonth: 8, endDay: 31 },
    { startMonth: 9, startDay: 1, endMonth: 12, endDay: 31 },
    { startMonth: 1, startDay: 1, endMonth: 2, endDay: 26 }
];


function getAllSeasons() {
    const today = new Date();
    const seasons = { beginning: firstSeason };


    const beginningEnd = new Date(firstSeason.endDate);
    if (today <= beginningEnd) {
        return seasons;
    }


    let year = 2;
    let season = 0;
    let seasonStart = new Date(beginningEnd);
    seasonStart.setDate(seasonStart.getDate() + 1);

    while (true) {

        season = (season % 4) + 1;


        const boundary = seasonBoundaries[season - 1];


        let seasonEnd;
        if (season === 4) {

            seasonEnd = new Date(seasonStart.getFullYear() + 1, boundary.endMonth - 1, boundary.endDay);
        } else {
            seasonEnd = new Date(seasonStart.getFullYear(), boundary.endMonth - 1, boundary.endDay);
        }


        if (seasonStart <= today) {
            const seasonKey = `y${year}s${season}`;
            seasons[seasonKey] = {
                name: `Y${year}S${season}`,
                displayName: `Y${year}S${season}`,
                startDate: seasonStart.toISOString().split('T')[0],
                endDate: seasonEnd.toISOString().split('T')[0],
                year,
                season
            };
        } else {

            break;
        }


        seasonStart = new Date(seasonEnd);
        seasonStart.setDate(seasonStart.getDate() + 1);


        if (season === 4) {
            year++;
        }


        if (Object.keys(seasons).length > 20) break;
    }

    return seasons;
}


const seasons = getAllSeasons();


const defaultNames = [
    'NewPlayer',
    'NewPlayer_1',
    'NewPlayer_2',
    'NewPlayer_3',
    'NewPlayer_4',
    'NewPlayer_5',
    'NewPlayer_6',
    'NewPlayer_7',
    'NewPlayer_8',
    'NewPlayer_9'
];

document.addEventListener('DOMContentLoaded', async () => {

    setupCurrentSeason();


    initializeFilterElements();


    initializePlaylistDropdown();


    applyUrlParameters();


    setupEventListeners();


    checkSeasonEndingSoon();


    fetchLeaderboard();


    fetchAllTimeMostActivePlayers();

    fetchPreviousSeasonWinners();
});

function getCurrentSeason() {
    const today = new Date();


    for (const [key, season] of Object.entries(seasons)) {
        const startDate = new Date(season.startDate);
        const endDate = new Date(season.endDate);

        if (today >= startDate && today <= endDate) {
            return { key, ...season };
        }
    }



    const sortedSeasons = Object.entries(seasons)
        .sort((a, b) => new Date(b[1].endDate) - new Date(a[1].endDate));

    if (sortedSeasons.length > 0) {
        const [lastKey, lastSeason] = sortedSeasons[0];
        const lastEndDate = new Date(lastSeason.endDate);

        if (today > lastEndDate) {


            return { key: lastKey, ...lastSeason };
        }
    }


    return { key: 'beginning', ...firstSeason };
}

function setupCurrentSeason() {
    const currentSeason = getCurrentSeason();


    const currentSeasonElement = document.getElementById('current-season');
    if (currentSeasonElement) {
        currentSeasonElement.textContent = currentSeason.displayName;
    }


    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;


    updateSeasonSelector(currentSeason.key);
}

function updateSeasonSelector(currentSeasonKey = 'current') {
    const seasonSelect = document.getElementById('season-select');
    if (!seasonSelect) return;


    while (seasonSelect.options.length > 2) {
        seasonSelect.remove(2);
    }


    const today = new Date();


    const availableSeasons = Object.entries(seasons)
        .filter(([key, season]) => new Date(season.startDate) <= today)
        .sort((a, b) => new Date(b[1].startDate) - new Date(a[1].startDate));


    for (const [key, season] of availableSeasons) {
        const option = document.createElement('option');
        option.value = key;
        option.textContent = season.displayName;
        seasonSelect.add(option);
    }


    seasonSelect.value = currentSeasonKey;
}
p
function checkSeasonEndingSoon() {
    const today = new Date();
    const currentSeason = getCurrentSeason();
    const seasonEndDate = new Date(currentSeason.endDate);


    const daysUntilSeasonEnd = Math.floor((seasonEndDate - today) / (1000 * 60 * 60 * 24));


    if (daysUntilSeasonEnd <= 7 && daysUntilSeasonEnd >= 0) {
        const seasonBanner = document.getElementById('season-end-banner');
        const seasonEndDateElement = document.getElementById('season-end-date');

        if (seasonBanner && seasonEndDateElement) {

            const options = { year: 'numeric', month: 'long', day: 'numeric' };
            seasonEndDateElement.textContent = seasonEndDate.toLocaleDateString(undefined, options);


            seasonBanner.style.display = 'flex';
        }
    }
}

function initializeFilterElements() {

    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) {
        serverFilter.value = leaderboardFilters.serverName;
    }


    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) {
        sortSelect.value = leaderboardFilters.sortBy;
    }


    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) {
        minGamesFilter.value = leaderboardFilters.minGames;
    }


    const startDateInput = document.getElementById('start-date');
    if (startDateInput) {
        startDateInput.value = leaderboardFilters.startDate;
    }

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) {
        endDateInput.value = leaderboardFilters.endDate;
    }
}


function initializePlaylistDropdown() {
    const playlistFilter = document.getElementById('playlist-filter');
    if (!playlistFilter) return;


    playlistFilter.innerHTML = '';


    Object.entries(serverPlaylists).forEach(([key, playlist]) => {
        const option = document.createElement('option');
        option.value = key;
        option.textContent = playlist.name;
        playlistFilter.appendChild(option);
    });
}

function applyUrlParameters() {
    const urlParams = new URLSearchParams(window.location.search);


    for (const [key, value] of urlParams.entries()) {
        if (key in leaderboardFilters) {
            leaderboardFilters[key] = value;
        }
    }


    if (urlParams.has('season')) {
        const seasonValue = urlParams.get('season');
        applySeasonFilter(seasonValue);


        const seasonSelect = document.getElementById('season-select');
        if (seasonSelect) {
            seasonSelect.value = seasonValue;
        }
    }


    if (urlParams.has('playlist')) {
        const playlistValue = urlParams.get('playlist');


        const playlistFilter = document.getElementById('playlist-filter');
        if (playlistFilter && serverPlaylists[playlistValue]) {
            playlistFilter.value = playlistValue;
        }
    }
}

function applySeasonFilter(seasonValue) {

    switch (seasonValue) {
        case 'current':
            const currentSeason = getCurrentSeason();
            leaderboardFilters.startDate = currentSeason.startDate;
            leaderboardFilters.endDate = currentSeason.endDate;
            break;

        case 'all':
            leaderboardFilters.startDate = '';
            leaderboardFilters.endDate = '';
            break;

        case 'custom':

            toggleCustomDateInputs(true);
            break;

        default:

            if (seasons[seasonValue]) {
                leaderboardFilters.startDate = seasons[seasonValue].startDate;
                leaderboardFilters.endDate = seasons[seasonValue].endDate;
            }
            break;
    }


    const startDateInput = document.getElementById('start-date');
    const endDateInput = document.getElementById('end-date');

    if (startDateInput) startDateInput.value = leaderboardFilters.startDate;
    if (endDateInput) endDateInput.value = leaderboardFilters.endDate;
}


async function fetchPreviousSeasonWinners() {
    try {

        const pastSeasons = [];
        const currentSeason = getCurrentSeason();


        const today = new Date();

        for (const [key, season] of Object.entries(seasons)) {
            const endDate = new Date(season.endDate);
            if (endDate < today && key !== currentSeason.key) {
                pastSeasons.push({ key, ...season });
            }
        }

        if (pastSeasons.length === 0) {

            document.getElementById('previous-seasons-section').style.display = 'none';
            return;
        }


        const seasonWinners = [];

        for (const season of pastSeasons) {

            const queryParams = new URLSearchParams({
                startDate: season.startDate,
                endDate: season.endDate,
                sortBy: 'wins',
                limit: 1,
                excludeNames: defaultNames.join(','),
                includeDetailedStats: 'true'
            });

            const response = await fetch(`/api/leaderboard?${queryParams.toString()}`);
            const data = await response.json();

            if (data.leaderboard && data.leaderboard.length > 0) {
                const winner = data.leaderboard[0];


                const nemesisParams = new URLSearchParams({
                    playerName: winner.player_name,
                    startDate: season.startDate,
                    endDate: season.endDate,
                    getNemesisData: 'true'
                });

                const nemesisResponse = await fetch(`/api/player-nemesis?${nemesisParams.toString()}`);
                const nemesisData = await nemesisResponse.json();

                seasonWinners.push({
                    season: season.displayName,
                    playerName: winner.player_name,
                    wins: winner.wins,
                    losses: winner.losses,
                    gamesPlayed: winner.games_played,
                    avgScore: Math.round(winner.avg_score),
                    winRatio: (winner.win_ratio).toFixed(1),
                    archNemesis: nemesisData.nemesis || "None",
                    profileUrl: winner.profileUrl
                });
            }
        }


        displayPreviousSeasonWinners(seasonWinners);

    } catch (error) {
        console.error('Error fetching previous season winners:', error);

        document.getElementById('previous-seasons-section').style.display = 'none';
    }
}


function displayPreviousSeasonWinners(seasonWinners) {
    const winnersList = document.getElementById('previous-winners-list');
    if (!winnersList) return;

    winnersList.innerHTML = '';

    if (seasonWinners.length === 0) {
        document.getElementById('previous-seasons-section').style.display = 'none';
        return;
    }

    document.getElementById('previous-seasons-section').style.display = 'block';


    seasonWinners.sort((a, b) => {

        const aYearMatch = a.season.match(/Y(\d+)S(\d+)/);
        const bYearMatch = b.season.match(/Y(\d+)S(\d+)/);


        if (a.season.includes('The Beginning')) return 1;
        if (b.season.includes('The Beginning')) return -1;


        if (aYearMatch && bYearMatch) {
            const aYear = parseInt(aYearMatch[1]);
            const aSeason = parseInt(aYearMatch[2]);
            const bYear = parseInt(bYearMatch[1]);
            const bSeason = parseInt(bYearMatch[2]);


            if (aYear !== bYear) {
                return bYear - aYear;
            }
            return bSeason - aSeason;
        }


        return b.season.localeCompare(a.season);
    });


    const recentWinners = seasonWinners.slice(0, 5);


    recentWinners.forEach((winner, index) => {
        const listItem = document.createElement('li');
        const profileLink = winner.profileUrl
            ? `<a href="${winner.profileUrl}" target="_blank" class="player-link-icon"><i class="fas fa-square-up-right"></i></a> `
            : '';

        listItem.innerHTML = `
            <div class="season-name">${winner.season}</div>
            <div class="winner-info">
                ${profileLink}<span class="winner-name">${index === 0 ? '<i class="fas fa-crown previous-winner-crown"></i>' : ''}${winner.playerName}</span>
                <span class="winner-wins">${winner.wins} wins</span>
            </div>
            <div class="winner-stats">
                <div class="stat-item">
                    <span class="stat-label">Win Ratio</span>
                    <span class="stat-value">${winner.winRatio}%</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Avg Score</span>
                    <span class="stat-value">${winner.avgScore}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label nemesis-tip">Arch Nemesis</span>
                    <span class="stat-value nemesis-value">${winner.archNemesis}</span>
                </div>
            </div>
        `;

        winnersList.appendChild(listItem);
    });
}

function setupEventListeners() {

    const filtersToggle = document.getElementById('filters-toggle');
    if (filtersToggle) {
        filtersToggle.addEventListener('click', () => {

            const filtersSection = document.getElementById('leaderboard-filters');
            if (filtersSection) {
                filtersSection.classList.toggle('expanded');
                filtersToggle.classList.toggle('active');
            }


            const advancedFilters = document.getElementById('advanced-filters');
            if (advancedFilters) {
                advancedFilters.classList.toggle('expanded');
                filtersToggle.classList.toggle('active');
            }
        });
    }


    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) {
        serverFilter.addEventListener('change', () => {
            leaderboardFilters.serverName = serverFilter.value;


            const playlistFilter = document.getElementById('playlist-filter');
            if (playlistFilter) {
                playlistFilter.value = 'all';
            }
            delete leaderboardFilters.playlist;

            updateURL();
            fetchLeaderboard();

        });
    }


    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) {
        playlistFilter.addEventListener('change', () => {
            const selectedPlaylist = playlistFilter.value;

            if (selectedPlaylist === 'all') {
                leaderboardFilters.serverName = '';
                delete leaderboardFilters.playlist;
            } else {

                leaderboardFilters.serverName = '';
                leaderboardFilters.playlist = selectedPlaylist;

                if (serverFilter) {
                    serverFilter.value = '';
                }
            }

            updateURL();
            fetchLeaderboard();

        });
    }


    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) {
        sortSelect.addEventListener('change', () => {
            leaderboardFilters.sortBy = sortSelect.value;
            updateURL();
            fetchLeaderboard();
        });
    }


    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) {
        minGamesFilter.addEventListener('change', () => {
            leaderboardFilters.minGames = minGamesFilter.value;
            updateURL();
            fetchLeaderboard();
        });
    }


    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) {
        seasonSelect.addEventListener('change', () => {
            const selectedSeason = seasonSelect.value;
            applySeasonFilter(selectedSeason);


            if (selectedSeason !== 'custom') {
                updateURL();
                fetchLeaderboard();

            }
        });
    }


    const startDateInput = document.getElementById('start-date');
    if (startDateInput) {
        startDateInput.addEventListener('change', () => {
            leaderboardFilters.startDate = startDateInput.value;
        });
    }

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) {
        endDateInput.addEventListener('change', () => {
            leaderboardFilters.endDate = endDateInput.value;
        });
    }


    const applyDateBtn = document.getElementById('apply-date-filter');
    if (applyDateBtn) {
        applyDateBtn.addEventListener('click', () => {
            updateURL();
            fetchLeaderboard();

        });
    }


    const resetFiltersBtn = document.getElementById('reset-filters');
    if (resetFiltersBtn) {
        resetFiltersBtn.addEventListener('click', resetFilters);
    }
}


async function fetchAllTimeMostActivePlayers() {
    try {

        const queryParams = new URLSearchParams();


        queryParams.set('excludeNames', defaultNames.join(','));


        const response = await fetch(`/api/most-active-players?${queryParams.toString()}`);
        const activePlayers = await response.json();


        displayMostActivePlayers(activePlayers);
    } catch (error) {
        console.error('Error fetching all-time most active players:', error);
    }
}

function toggleCustomDateInputs(show) {

    const customDateRange = document.getElementById('custom-date-range');
    if (customDateRange) {
        customDateRange.style.display = show ? 'flex' : 'none';


        if (show) {
            const filtersSection = document.getElementById('leaderboard-filters');
            const filtersToggle = document.getElementById('filters-toggle');

            if (filtersSection && !filtersSection.classList.contains('expanded')) {
                filtersSection.classList.add('expanded');
                if (filtersToggle) filtersToggle.classList.add('active');
            }


            const advancedFilters = document.getElementById('advanced-filters');
            if (advancedFilters && !advancedFilters.classList.contains('expanded')) {
                advancedFilters.classList.add('expanded');
                if (filtersToggle) filtersToggle.classList.add('active');
            }
        }
    }


    const dateRangeContainer = document.querySelector('.date-range-container');
    if (dateRangeContainer) {
        dateRangeContainer.style.display = show ? 'flex' : 'none';
    }
}

function updateURL() {
    const url = new URL(window.location);


    Object.entries(leaderboardFilters).forEach(([key, value]) => {
        if (value) {
            url.searchParams.set(key, value);
        } else {
            url.searchParams.delete(key);
        }
    });


    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) {
        url.searchParams.set('season', seasonSelect.value);
    }

    window.history.pushState({}, '', url);
}

function resetFilters() {

    const currentSeason = getCurrentSeason();


    leaderboardFilters.serverName = '';
    leaderboardFilters.sortBy = 'wins';
    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;
    leaderboardFilters.minGames = '1';
    delete leaderboardFilters.playlist;


    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) serverFilter.value = '';

    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) playlistFilter.value = 'all';

    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) sortSelect.value = 'wins';

    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) minGamesFilter.value = '1';

    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) seasonSelect.value = 'current';

    const startDateInput = document.getElementById('start-date');
    if (startDateInput) startDateInput.value = currentSeason.startDate;

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) endDateInput.value = currentSeason.endDate;


    toggleCustomDateInputs(false);


    updateURL();
    fetchLeaderboard();

}


async function fetchLeaderboard() {
    try {

        const queryParams = new URLSearchParams();


        Object.entries(leaderboardFilters).forEach(([key, value]) => {
            if (value) queryParams.set(key, value);
        });


        if (leaderboardFilters.playlist && serverPlaylists[leaderboardFilters.playlist]) {
            queryParams.delete('playlist');
            queryParams.set('serverList', serverPlaylists[leaderboardFilters.playlist].servers.join(','));
        }


        queryParams.set('excludeNames', defaultNames.join(','));


        queryParams.set('_t', Date.now());


        const tableBody = document.querySelector('#leaderboard-table tbody');
        if (tableBody) {
            tableBody.innerHTML = '<tr><td colspan="6" class="loading-row"><i class="fas fa-circle-notch fa-spin"></i> Loading leaderboard data...</td></tr>';
        }


        const response = await fetch(`/api/leaderboard?${queryParams.toString()}`);
        const data = await response.json();


        displayLeaderboard(data.leaderboard);


        updateLeaderboardMetadata(data);
    } catch (error) {
        console.error('Error fetching leaderboard:', error);


        const tableBody = document.querySelector('#leaderboard-table tbody');
        if (tableBody) {
            tableBody.innerHTML = '<tr><td colspan="6" class="error-row">Error loading leaderboard data. Please try again later.</td></tr>';
        }
    }
}

function updateLeaderboardMetadata(data) {

    const leaderboardHeader = document.querySelector('.leaderboard-header');
    if (leaderboardHeader) {
        let headerText = 'DEFCON EXPANDED LEADERBOARD';

        if (leaderboardFilters.serverName) {
            headerText = `${leaderboardFilters.serverName} LEADERBOARD`;
        } else if (leaderboardFilters.playlist && serverPlaylists[leaderboardFilters.playlist]) {
            headerText = `${serverPlaylists[leaderboardFilters.playlist].name.toUpperCase()} LEADERBOARD`;
        }

        const seasonSelect = document.getElementById('season-select');
        if (seasonSelect) {
            const selectedValue = seasonSelect.value;

            if (selectedValue === 'current') {
                const currentSeason = getCurrentSeason();
                headerText += ` - ${currentSeason.displayName}`;
            } else if (selectedValue === 'all') {
                headerText += ' - ALL TIME';
            } else if (selectedValue === 'custom') {
                headerText += ' - CUSTOM PERIOD';
            } else if (seasons[selectedValue]) {
                headerText += ` - ${seasons[selectedValue].displayName}`;
            }
        }

        leaderboardHeader.textContent = headerText;
    }


    const totalPlayers = document.getElementById('unique-players');
    if (totalPlayers && data.totalPlayers !== undefined) {
        totalPlayers.textContent = `Unique Players: ${data.totalPlayers}`;
    }


    const qualifyLabel = document.querySelector('.qualifylabel');
    if (qualifyLabel) {
        qualifyLabel.textContent = `To qualify you need ${leaderboardFilters.minGames} game${leaderboardFilters.minGames > 1 ? 's' : ''} played`;
    }
}

function displayLeaderboard(data) {
    const tableBody = document.querySelector('#leaderboard-table tbody');
    if (!tableBody) return;

    tableBody.innerHTML = '';


    if (!data || data.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = `
        <td colspan="6" style="text-align: center; color: #b8b8b8;">No players found matching your criteria</td>
      `;
        tableBody.appendChild(row);
        return;
    }


    updateColumnHeaders(leaderboardFilters.sortBy);


    const filteredData = data.filter(player => !defaultNames.includes(player.player_name));


    filteredData.forEach((player, index) => {
        const displayRank = index + 1;

        const row = document.createElement('tr');
        let rankClass = '';


        if (displayRank === 1) {
            rankClass = 'gold-rank';
        } else if (displayRank === 2) {
            rankClass = 'silver-rank';
        } else if (displayRank === 3) {
            rankClass = 'bronze-rank';
        }


        const profileLinkIcon = player.profileUrl
            ? `<a href="${player.profileUrl}" target="_blank" class="player-link-icon"><i class="fas fa-square-up-right"></i></a> `
            : '';


        let sixthColumnValue = '';
        if (leaderboardFilters.sortBy === 'winRatio' || leaderboardFilters.sortBy === 'weightedScore') {
            sixthColumnValue = `${player.win_ratio.toFixed(2)}%`;
        } else if (leaderboardFilters.sortBy === 'avgScore') {
            sixthColumnValue = Math.round(player.avg_score);
        } else if (leaderboardFilters.sortBy === 'highestScore') {
            sixthColumnValue = player.highest_score;
        } else {
            sixthColumnValue = player.total_score;
        }


        row.innerHTML = `
        <td class="rank-cell">${displayRank}</td>
        <td class="player-cell ${rankClass}">${profileLinkIcon}${player.player_name}</td>
        <td class="stats-cell">${player.wins}</td>
        <td class="stats-cell">${player.losses}</td>
        <td class="stats-cell">${player.games_played}</td>
        <td class="stats-cell">${sixthColumnValue}</td>
      `;


        if (player.best_territory) {
            row.setAttribute('title', `Best Territory: ${player.best_territory} | Worst Territory: ${player.worst_territory || 'N/A'}`);
        }

        tableBody.appendChild(row);
    });
}

function updateColumnHeaders(sortBy) {
    const headers = document.querySelectorAll('#leaderboard-table th');
    if (headers.length < 6) return;


    const lastHeader = headers[5];

    switch (sortBy) {
        case 'winRatio':
        case 'weightedScore':
            lastHeader.textContent = 'WIN RATIO';
            break;
        case 'avgScore':
            lastHeader.textContent = 'AVG SCORE';
            break;
        case 'highestScore':
            lastHeader.textContent = 'HIGHEST SCORE';
            break;
        default:
            lastHeader.textContent = 'TOTAL SCORE';
    }
}

function displayMostActivePlayers(data) {
    const list = document.getElementById('active-players-list');
    if (!list) return;

    list.innerHTML = '';


    const filteredPlayers = data.filter(player => !defaultNames.includes(player.player_name));

    filteredPlayers.forEach((player, index) => {
        const listItem = document.createElement('li');
        const playerNameClass = player.player_name.toLowerCase() === 'ladenbinkezza' ? 'neon-flow' : '';
        let playerIcon = index === 0 ? '⭐' : '';

        listItem.innerHTML = `
        <span class="player-rank">${index + 1}</span>
        <span class="player-name ${playerNameClass}">${player.player_name}<span class="player-icon">${playerIcon}</span></span>
        <span class="player-games">${player.games_played}</span>
      `;

        list.appendChild(listItem);
    });
}