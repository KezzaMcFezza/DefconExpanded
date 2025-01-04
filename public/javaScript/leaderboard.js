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
//Last Edited 18-12-2024 

document.addEventListener('DOMContentLoaded', () => {
    const urlParams = new URLSearchParams(window.location.search);
    const leaderboardType = urlParams.get('type') || 'regular';

    const buttons = document.querySelectorAll('.leaderboard-type-selector button');
    buttons.forEach(btn => {
        btn.classList.remove('active');
        if ((btn.id === 'regularLeaderboard' && leaderboardType === 'regular') ||
            (btn.id === 'tournamentLeaderboard' && leaderboardType === 'tournament')) {
            btn.classList.add('active');
        }
    });

    const leaderboardHeader = document.querySelector('.leaderboard-header');
    const infoLabel = document.querySelector('.active-players ul label');
    const qualifyLabel = document.querySelector('.sort-container label')

    if (leaderboardType === 'tournament') {
        leaderboardHeader.textContent = 'DEFCON EXPANDED 2V2 TOURNAMENT';
        infoLabel.innerHTML = `
            The way the tournament ranking system works is based on a formula that aims to rank people higher if they retain a high win rate with more games played
            <br>
            <br>
            So this prevents people playing 5 games and getting 100 percent win ratio and calling it a day.
            <br>
            <br>
        `;
        qualifyLabel.textContent = `To qualify for the finals you need 5 games played`;
    } else {
        leaderboardHeader.textContent = 'DEFCON EXPANDED 1V1 LEADERBOARD';
        infoLabel.innerHTML = `
            The way the leaderboard ranking system works is based on who has the most wins
            <br>
            <br>
            However if two of the same people have the same wins it will display the person with the most total score as a higher rank
            <br>
            <br>
        `;
        qualifyLabel.textContent = `To qualify you need to play 1 game`;
    }

    const sortSelect = document.getElementById('sort-select');
    if (leaderboardType === 'tournament') {
        sortSelect.innerHTML = `
            <option value="weightedScore" selected>Tournament</option>
            <option value="wins">Wins</option>
            <option value="gamesPlayed">Games Played</option>
        `;
    } else {
        sortSelect.innerHTML = `
            <option value="wins" selected>Wins</option>
            <option value="gamesPlayed">Games Played</option>
            <option value="totalScore">Total Score</option>
        `;
    }

    fetchLeaderboard(leaderboardType, leaderboardType === 'tournament' ? 'winRatio' : 'wins');
    fetchMostActivePlayers(leaderboardType);

    document.getElementById('regularLeaderboard').addEventListener('click', () => {
        window.location.href = '/leaderboard?type=regular';
    });

    document.getElementById('tournamentLeaderboard').addEventListener('click', () => {
        window.location.href = '/leaderboard?type=tournament';
    });

    sortSelect.addEventListener('change', () => {
        fetchLeaderboard(leaderboardType, sortSelect.value);
    });
});

async function fetchLeaderboard(type = 'regular', sortBy = 'wins') {
    try {
        const endpoint = type === 'tournament' ? '/api/tournament-leaderboard' : '/api/leaderboard';
        const response = await fetch(`${endpoint}?sortBy=${sortBy}`);
        const leaderboardData = await response.json();
        displayLeaderboard(leaderboardData, type);
    } catch (error) {
        console.error('Error fetching leaderboard:', error);
    }
}

async function fetchMostActivePlayers(type = 'regular') {
    try {
        const endpoint = type === 'tournament' ? '/api/most-active-tournament-players' : '/api/most-active-players';
        const response = await fetch(endpoint);
        const activePlayers = await response.json();
        displayMostActivePlayers(activePlayers);
    } catch (error) {
        console.error('Error fetching most active players:', error);
    }
}

function displayLeaderboard(data, type) {
    const tableBody = document.querySelector('#leaderboard-table tbody');
    const headers = document.querySelectorAll('#leaderboard-table th');
    tableBody.innerHTML = '';

    const lastHeader = headers[headers.length - 1];
    if (type === 'tournament') {
        lastHeader.textContent = 'WIN RATIO';
        lastHeader.style.display = '';
    } else {
        lastHeader.textContent = 'TOTAL SCORE';
        lastHeader.style.display = '';
    }

    if (data.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td colspan="6" style="text-align: center; color: #b8b8b8;">No players have played a game, yet :)</td>
        `;
        tableBody.appendChild(row);
    } else {
        data.forEach((player, index) => {
            const row = document.createElement('tr');
            let rankClass = '';
            let medalIcon = '';

            if (player.absolute_rank === 1) {
                rankClass = 'gold-rank';
                medalIcon = '🥇';
            } else if (player.absolute_rank === 2) {
                rankClass = 'silver-rank';
                medalIcon = '🥈';
            } else if (player.absolute_rank === 3) {
                rankClass = 'bronze-rank';
                medalIcon = '🥉';
            }

            const winRatio = ((player.wins / player.games_played) * 100 || 0).toFixed(2);
            const profileLinkIcon = player.profileUrl
                ? `<a href="${player.profileUrl}" target="_blank" class="player-link-icon"><i class="fas fa-square-up-right"></i></a> `
                : '';

            row.innerHTML = `
                <td class="rank-cell">${index + 1}</td>
                <td class="player-cell ${rankClass}">${profileLinkIcon}${player.player_name}<span class="medal-icon">${medalIcon}</span></td>
                <td class="stats-cell">${player.wins}</td>
                <td class="stats-cell">${player.losses}</td>
                <td class="stats-cell">${player.games_played}</td>
                ${type === 'tournament'
                    ? `<td class="win-ratio-cell">${winRatio}%</td>`
                    : `<td class="total-score-cell">${player.total_score}</td>`
                }
            `;
            tableBody.appendChild(row);
        });
    }
}

function displayMostActivePlayers(data) {
    const list = document.getElementById('active-players-list');
    list.innerHTML = '';

    data.forEach((player, index) => {
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