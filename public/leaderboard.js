document.addEventListener('DOMContentLoaded', () => {
    fetchLeaderboard();
    fetchMostActivePlayers();

    const sortSelect = document.getElementById('sort-select');
    sortSelect.addEventListener('change', () => {
        fetchLeaderboard(sortSelect.value);
    });
});

async function fetchLeaderboard(sortBy = 'wins') {
    try {
        const response = await fetch(`/api/leaderboard?sortBy=${sortBy}`);
        const leaderboardData = await response.json();
        displayLeaderboard(leaderboardData);
    } catch (error) {
        console.error('Error fetching leaderboard:', error);
    }
}

async function fetchMostActivePlayers() {
    try {
        const response = await fetch('/api/most-active-players');
        const activePlayers = await response.json();
        displayMostActivePlayers(activePlayers);
    } catch (error) {
        console.error('Error fetching most active players:', error);
    }
}

function displayLeaderboard(data) {
    const tableBody = document.querySelector('#leaderboard-table tbody');
    tableBody.innerHTML = '';

    if (data.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td colspan="7" style="text-align: center; color: #b8b8b8;">No players have achieved a positive score yet.</td>
        `;
        tableBody.appendChild(row);
    } else {
        data.forEach((player, index) => {
            const row = document.createElement('tr');
            let rankClass = '';
            let medalIcon = '';
            
            // Use absolute_rank to determine medals and colors
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
            
            // Add the profile URL icon before the player name if the player has a profile
            const profileLinkIcon = player.profileUrl 
                ? `<a href="${player.profileUrl}" target="_blank" class="player-link-icon"><i class="fas fa-square-up-right"></i></a> `
                : '';

            const currentRank = index + 1;
            
            row.innerHTML = `
                <td class="rank-cell">${currentRank}</td>
                <td class="player-cell ${rankClass}">${profileLinkIcon} ${player.player_name}<span class="medal-icon">${medalIcon}</span></td>
                <td class="stats-cell">${player.wins}</td>
                <td class="stats-cell">${player.losses}</td>
                <td class="stats-cell">${player.games_played}</td>
                <td class="total-score-cell">${player.total_score}</td>
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