// Global variables
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

function createDemoCard(demo) {
    const demoCard = document.createElement('div');
    demoCard.className = 'demo-card';
    demoCard.innerHTML = `
        <div class="demo-content">
            <h3 class="game-type">${demo.name}</h3>
            <p class="time-info">
                <span class="time-ago">${getTimeAgo(demo.date)}</span><br>
                <span class="game-time">${getGameTime(demo.date)}</span><br>
                <span class="game-date">${new Date(demo.date).toLocaleDateString()}</span>
                <span class="game-duration">(${getDuration()})</span>
            </p>
            <table class="game-results">
                <tr>
                    <th>Player</th>
                    <th>Territory</th>
                    <th>Score</th>
                </tr>
                ${getPlayerData(demo.name, demo.users)}
            </table>
            <a href="/api/download/${demo.name}" class="btn-download foo"> Download </a>
            <a href="/homepage/matchroom" class="btn-matchroom foo"> Matchroom </a>
            ${isAdmin ? `<button onclick="deleteDemo(${demo.id})" class="btn-delete foo">Delete Demo</button>` : ''}
        </div>
    `;
    return demoCard;
}

function getPlayerData(demoName, users) {
    if (users && users.length > 0) {
        return users.map(user => `
            <tr>
                <td>${user}</td>
                <td>Unknown</td>
                <td>0</td>
            </tr>
        `).join('');
    } else {
        return getRandomPlayerData(demoName);
    }
}

function getRandomPlayerData(demoName, numPlayers = 8) {
    const territories = ['NA', 'EU', 'SA', 'RU', 'AF', 'EA', 'WA', 'AA'];
    const prefixes = [  'Nuclear', 'Atomic', 'Stealth', 'Cyber', 'Tactical', 'Strategic', 'Rogue', 'Shadow', 'Sieverts',
        'Covert', 'Phantom', 'Ghost', 'Recon', 'Assault', 'Havoc', 'Chaos', 'Mayhem', 'Vortex',
        'Laser', 'Plasma', 'Ion', 'Photon', 'Quantum', 'Fusion', 'Neutrino', 'Electron', 'Proton',
        'Gravity', 'Singularity', 'Nebula', 'Nova', 'Pulsar', 'Quasar', 'Supernova', 'Hypernova', 'Magnetar',
        'Void', 'Abyss', 'Oblivion', 'Nihil', 'Ether', 'Astral', 'Cosmic', 'Galactic', 'Celestial',
        'Titan', 'Colossus', 'Behemoth', 'Leviathan', 'Kraken', 'Hydra', 'Phoenix', 'Chimera', 'Sphinx'];
    const suffixes = ['Xx'];
    const numbers = ['', '1', '2', '3', '4', '5', '42', '69', '99', '007'];

    function generateGamertag() {
        const prefix = prefixes[Math.floor(Math.random() * prefixes.length)];
        const suffix = suffixes[Math.floor(Math.random() * suffixes.length)];
        const number = numbers[Math.floor(Math.random() * numbers.length)];
        return `${prefix}${suffix}${number}`;
    }

    const shuffledTerritories = territories.slice().sort(() => Math.random() - 0.5);

    const players = Array.from({ length: numPlayers }, (_, index) => ({
        name: generateGamertag(),
        territory: shuffledTerritories[index % territories.length],
        score: Math.floor(Math.random() * 200) - 50
    }));

    let teamColors = [];
    if (demoName.toLowerCase().includes('4v4')) {
        teamColors = ['#c70000', '#c70000', '#c70000', '#c70000', '#40d340', '#40d340', '#40d340', '#40d340'];
    } else if (demoName.toLowerCase().includes('2v2v2v2')) {
        teamColors = ['#c70000', '#c70000', '#40d340', '#40d340', '#0084ff', '#0084ff', '#ff8000', '#ff8000'];
    } else if (demoName.toLowerCase().includes('diplomacy')) {
        teamColors = ['#40d340', '#40d340', '#40d340', '#40d340', '#40d340', '#40d340', '#40d340', '#40d340'];
    }

    return players.map((player, index) => `
        <tr>
            <td style="${teamColors[index] ? `color: ${teamColors[index]};` : ''}">${player.name}</td>
            <td>${player.territory}</td>
            <td>${player.score}</td>
        </tr>
    `).join('');
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

function getGameTime(date) {
    const gameDate = new Date(date);
    const startTime = gameDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true });
    gameDate.setMinutes(gameDate.getMinutes() + Math.floor(Math.random() * 60) + 15);
    const endTime = gameDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true });
    return `${startTime} — ${endTime}`;
}

function getDuration() {
    const minutes = Math.floor(Math.random() * (200 - 45 + 1)) + 45;
    
    if (minutes >= 60) {
        const hours = Math.floor(minutes / 60);
        const remainingMinutes = minutes % 60;
        return `${hours}h ${remainingMinutes}m`;
    } else {
        return `${minutes}m`;
    }
}

function updateDemoList() {
    console.log('Updating demo list, also why are you checking my website on inspect element 0__0');
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

updateDemoList();

function performGameSearch() {
    const searchInput = document.getElementById('search-input2');
    if (!searchInput) return;
    const searchTerm = searchInput.value.toLowerCase();
    const filteredDemos = allDemos.filter(demo => 
        demo.name.toLowerCase().includes(searchTerm) ||
        formatBytes(demo.size).toLowerCase().includes(searchTerm) ||
        new Date(demo.date).toLocaleDateString().toLowerCase().includes(searchTerm) ||
        (demo.users && demo.users.some(user => user.toLowerCase().includes(searchTerm)))
    );
    showPage(1, filteredDemos);
}

async function deleteDemo(demoId) {
    if (!isAdmin) {
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
                updateDemoList(); // Refresh the demo list
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

// Export functions that might be used elsewhere
window.updateDemoList = updateDemoList;
window.performGameSearch = performGameSearch;
window.deleteDemo = deleteDemo;