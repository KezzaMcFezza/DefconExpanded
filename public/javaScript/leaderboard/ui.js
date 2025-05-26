// DefconExpanded, Created by...
// KezzaMcFezza - Main Developer
// Nexustini - Server Management
//
// Notable Mentions...
// Rad - For helping with python scripts.
// Bert_the_turtle - Doing everything with c++
//
// Inspired by Sievert and Wan May
// 
// Last Edited 14-04-2025

import { leaderboardFilters, defaultNames, serverPlaylists } from './constants.js';

export function displayLeaderboard(data) {
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
            sixthColumnValue = `${Math.max(0, player.win_ratio).toFixed(2)}%`;
        } else if (leaderboardFilters.sortBy === 'avgScore') {
            sixthColumnValue = Math.max(0, Math.round(player.avg_score));
        } else if (leaderboardFilters.sortBy === 'highestScore') {
            sixthColumnValue = Math.max(0, player.highest_score);
        } else {
            sixthColumnValue = Math.max(0, player.total_score);
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

export function updateColumnHeaders(sortBy) {
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

export function updateLeaderboardMetadata(data) {
    const leaderboardHeader = document.querySelector('.leaderboard-header');
    let headerText = 'DEFCON EXPANDED LEADERBOARD';
    let shouldWaitForAsyncOperation = false;

    if (leaderboardFilters.serverName) {
        headerText = `${leaderboardFilters.serverName} LEADERBOARD`;
    } else if (leaderboardFilters.playlist && serverPlaylists[leaderboardFilters.playlist]) {
        headerText = `${serverPlaylists[leaderboardFilters.playlist].name.toUpperCase()} LEADERBOARD`;
    }

    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) {
        const selectedValue = seasonSelect.value;

        if (selectedValue === 'current') {
            const currentSeason = document.getElementById('current-season');
            if (currentSeason) {
                headerText += ` - ${currentSeason.textContent}`;
            }
        } else if (selectedValue === 'all') {
            headerText += ' - ALL TIME';
        } else if (selectedValue === 'custom') {
            headerText += ' - CUSTOM PERIOD';
        } else {
            shouldWaitForAsyncOperation = true;
            import('./seasons.js').then(module => {
                const seasons = module.getAllSeasons();
                if (seasons[selectedValue]) {
                    headerText += ` - ${seasons[selectedValue].displayName}`;
                    if (leaderboardHeader) {
                        leaderboardHeader.textContent = headerText;
                    }
                }
            });
        }
    }

    if (!shouldWaitForAsyncOperation && leaderboardHeader) {
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

export function displayMostActivePlayers(data) {
    const list = document.getElementById('active-players-list');
    if (!list) return;

    list.innerHTML = '';

    const filteredPlayers = data.filter(player => !defaultNames.includes(player.player_name));

    filteredPlayers.forEach((player, index) => {
        const listItem = document.createElement('li');

        const playerNameClass = index === 0 ? 'neon-flow' : '';
        let playerIcon = index === 0 ? '⭐' : '';

        listItem.innerHTML = `
        <span class="player-rank">${index + 1}</span>
        <span class="player-name ${playerNameClass}">${player.player_name}<span class="player-icon">${playerIcon}</span></span>
        <span class="player-games">${player.games_played}</span>
      `;

        list.appendChild(listItem);
    });
}

export function displayPreviousSeasonWinners(seasonWinners) {
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