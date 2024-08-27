let allDemos = [];

function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function updatePagination(totalPages, currentPage) {
    const paginationContainer = document.getElementById('pagination');
    if (!paginationContainer) return;
    paginationContainer.innerHTML = '';

    for (let i = 1; i <= totalPages; i++) {
        const pageButton = document.createElement('button');
        pageButton.textContent = i;
        if (i === currentPage) {
            pageButton.disabled = true;
        }
        pageButton.addEventListener('click', () => showPage(i, allDemos));
        paginationContainer.appendChild(pageButton);
    }
}

function showPage(pageNumber, demos) {
    const demosPerPage = 8;
    const start = (pageNumber - 1) * demosPerPage;
    const end = start + demosPerPage;
    const demoContainer = document.getElementById('demo-container');
    if (!demoContainer) return;
    demoContainer.innerHTML = ''; 

    demos.slice(start, end).forEach(demo => {
        const demoCard = createDemoCard(demo);
        demoContainer.appendChild(demoCard);
    });

    updatePagination(Math.ceil(demos.length / demosPerPage), pageNumber);
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

function createDemoCard(demo) {
    console.log('Creating demo card for:', demo);
    const demoDate = new Date(demo.date);
    const demoCard = document.createElement('div');
    demoCard.className = 'demo-card';
    demoCard.innerHTML = `
        <div class="demo-content">
            <h3 class="game-type">${demo.game_type || 'Unknown'}</h3>
            <p class="time-info">
                <span class="time-ago">${getTimeAgo(demo.date)}</span><br>
                <span class="game-added">Game Added - ${demoDate.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'})}</span><br>
                <span class="game-date">Date - ${demoDate.toLocaleDateString()}</span><br>
                <span class="game-duration">Game Duration - ${formatDuration(demo.duration)}</span>
            </p>
            <table class="game-results">
                <tr>
                    <th>Player</th>
                    <th>Territory</th>
                    <th>Score</th>
                </tr>
                ${getPlayerDataFromDemo(demo)}
            </table>
            <a href="/api/download/${demo.name}" class="btn-download foo">Download</a>
            <a href="/homepage/matchroom" class="btn-matchroom foo">Matchroom</a>
            ${window.isAdmin ? `<button onclick="deleteDemo(${demo.id})" class="btn-delete foo">Delete Demo</button>` : ''}
        </div>
    `;
    return demoCard;
}

function getPlayerDataFromDemo(demo) {
  console.log('Processing demo:', demo);
  let playerData = [];
  
  let parsedPlayers = [];
  if (demo.players) {
    try {
      parsedPlayers = JSON.parse(demo.players);
    } catch (e) {
      console.error('Error parsing players JSON:', e);
    }
  }
  
  let alliance = null;
  if (demo.last_alliance) {
    try {
      alliance = JSON.parse(demo.last_alliance);
      console.log('Alliance data:', alliance);
    } catch (e) {
      console.error('Error parsing last_alliance JSON:', e);
    }
  }

  const colors = ['#ff4949', '#2da6ff', '#00bf00', '#ffa700', 'purple', 'orange', '#00e5ff', 'magenta'];
  
  if (parsedPlayers.length > 0) {
    // Calculate total score for each team
    const teamScores = {};
    parsedPlayers.forEach(player => {
      if (player.team !== undefined && player.score !== undefined) {
        if (teamScores[player.team] === undefined) {
          teamScores[player.team] = 0;
        }
        teamScores[player.team] += player.score;
      }
    });

    // Find the winning team
    const winningTeam = Object.keys(teamScores).reduce((a, b) => teamScores[a] > teamScores[b] ? a : b);

    // Sort players by team score in descending order
    parsedPlayers.sort((a, b) => teamScores[b.team] - teamScores[a.team]);

    return parsedPlayers.map((player) => {
      console.log(`Player ${player.name} - Team: ${player.team}, Game Type: ${demo.game_type}`);
      let color;
      if (alliance && alliance.includes(player.team.toString())) {
        color = colors[alliance[0] % colors.length];
      } else {
        color = colors[player.team % colors.length];
      }
      const isTrophyWinner = player.team.toString() === winningTeam;
      return `
        <tr>
          <td style="color: ${color}">${player.name}${isTrophyWinner ? ' 🏆' : ''}</td>
          <td>${player.territory}</td>
          <td>${player.score}</td>
        </tr>
      `;
    }).join('');
  }
  
  return '<tr><td colspan="3">No player data available</td></tr>';
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

function updateDemoList() {
    console.log('Updating demo list');
    fetch('/api/demos')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(demos => {
            allDemos = demos; 
            const gamesPlayed = document.getElementById('games-played');
            if (gamesPlayed) {
                gamesPlayed.textContent = `TOTAL GAMES PLAYED: ${demos.length}`;
            }
            showPage(1, allDemos);
        })
        .catch(error => {
            console.error('Error fetching demos:', error);
            alert('Failed to fetch demo data. Please try again later.');
        });
}

function performGameSearch() {
    const searchInput = document.getElementById('search-input2');
    if (!searchInput) return;
    const searchTerm = searchInput.value.toLowerCase();
    const filteredDemos = allDemos.filter(demo => 
        demo.name.toLowerCase().includes(searchTerm) ||
        formatBytes(demo.size).toLowerCase().includes(searchTerm) ||
        new Date(demo.date).toLocaleDateString().toLowerCase().includes(searchTerm) ||
        (demo.player_data && demo.player_data.toLowerCase().includes(searchTerm))
    );
    showPage(1, filteredDemos);
}

async function deleteDemo(demoId) {
    if (!window.isAdmin) {
        alert('You must be an admin to perform this action');
        return;
    }
    if (confirm('Are you sure you want to delete this demo?')) {
        try {
            const response = await fetch(`/api/demo/${demoId}`, {
                method: 'DELETE',
            });
            if (response.ok) {
                alert('Demo deleted successfully');
                updateDemoList(); 
            } else {
                const data = await response.json();
                alert(data.error || 'Failed to delete demo');
            }
        } catch (error) {
            console.error('Error deleting demo:', error);
            alert('An error occurred while deleting demo');
        }
    }
}

// Initialize the demo list when the page loads
document.addEventListener('DOMContentLoaded', () => {
    updateDemoList();

    const searchButton = document.getElementById('search-button2');
    if (searchButton) {
        searchButton.addEventListener('click', performGameSearch);
    }

    const searchInput = document.getElementById('search-input2');
    if (searchInput) {
        searchInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                performGameSearch();
            }
        });
    }
});

// Make functions globally available
window.updateDemoList = updateDemoList;
window.performGameSearch = performGameSearch;
window.deleteDemo = deleteDemo;