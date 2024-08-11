// Global variables
let allDemos = []; 
let soundVolume = 0.1;
let isAdmin = false;

function setActiveNavItem() {
    const currentPath = window.location.pathname;
    const navItems = document.querySelectorAll('#sidebar .list-items li');
    
    navItems.forEach(item => {
        const link = item.querySelector('a');
        item.classList.remove('active');
        if (currentPath === link.getAttribute('href') || currentPath.startsWith(link.getAttribute('href') + '/')) {
            item.classList.add('active');
        }
    });
}

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
            // Display an error message to the user or provide more specific error details
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
        (demo.users && demo.users.some(user => user.toLowerCase().includes(searchTerm)))
    );
    showPage(1, filteredDemos);
}

function openModal(src, type, title) {
    console.log('Opening modal:', src, type, title);
    const modal = document.getElementById("myModal");
    const modalContent = document.getElementById("modalContent");
    if (!modal || !modalContent) {
        console.error('Modal elements not found');
        return;
    }

    modalContent.innerHTML = '';

    if (type === 'img') {
        const img = document.createElement('img');
        img.src = src;
        img.alt = title;
        img.className = 'modal-image';
        modalContent.appendChild(img);
    } else if (type === 'video') {
        const video = document.createElement('video');
        video.src = src;
        video.controls = true;
        video.className = 'plyr__video-embed';
        modalContent.appendChild(video);

        const player = new Plyr(video, {
            controls: ['play-large', 'play', 'progress', 'current-time', 'mute', 'volume', 'captions', 'settings', 'pip', 'airplay', 'fullscreen'],
            settings: ['captions', 'quality', 'speed', 'loop'],
            title: title
        });
    }

    modal.style.display = "flex";
}

function closeModal() {
    const modal = document.getElementById("myModal");
    if (!modal) {
        console.error('Modal element not found');
        return;
    }
    modal.style.display = "none";
    const modalContent = document.getElementById("modalContent");
    if (modalContent) {
        modalContent.innerHTML = '';
    }
}

function setupMobileMenu() {
    console.log('Setting up mobile menu');
    const sidebar = document.getElementById('sidebar');
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');

    console.log('Sidebar:', sidebar);
    console.log('Title:', title);
    console.log('List Items:', listItems);

    if (title && listItems) {
        title.addEventListener('click', function(e) {
            e.preventDefault();
            console.log('Title clicked');
            listItems.classList.toggle('show');
            console.log('Menu toggled, show class:', listItems.classList.contains('show'));
        });

        // Close menu when a link is clicked
        listItems.addEventListener('click', function(e) {
            if (e.target.tagName === 'A') {
                console.log('Link clicked, closing menu');
                listItems.classList.remove('show');
            }
        });
    } else {
        console.error('Could not find title or list items elements');
    }
}

// Authentication functions
function showLoginForm() {
    const sidebar = document.getElementById('sidebar');
    if (!sidebar) {
        console.error('Sidebar not found');
        return;
    }

    // Check if login form already exists
    if (document.getElementById('login-container')) {
        console.log('Login form already exists');
        return;
    }

    const loginContainer = document.createElement('div');
    loginContainer.id = 'login-container';
    loginContainer.innerHTML = `
        <form id="login-form">
            <input type="text" id="username" placeholder="Username" required>
            <input type="password" id="password" placeholder="Password" required>
            <button type="submit">Login</button>
        </form>
    `;
    
    // Find the list-items and icons elements
    const listItems = sidebar.querySelector('.list-items');
    const icons = sidebar.querySelector('.icons');
    
    // Insert the login form after the list-items and before the icons
    if (listItems && icons) {
        sidebar.insertBefore(loginContainer, icons);
    } else {
        console.error('Could not find proper position for login form');
        sidebar.appendChild(loginContainer);
    }

    document.getElementById('login-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        await login(username, password);
    });
}

async function login(username, password) {
    try {
        const response = await fetch('/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ username, password }),
        });
        const data = await response.json();
        if (response.ok) {
            isAdmin = true;
            alert('Logged in successfully');
            document.getElementById('login-container').remove();
            showLogoutButton();
            updateDemoList(); // Refresh the demo list to show admin controls
            makeContentEditable(); // Make content editable for admin
        } else {
            alert(data.error || 'Login failed');
        }
    } catch (error) {
        console.error('Login error:', error);
        alert('An error occurred during login');
    }
}

function showLogoutButton() {
    const sidebar = document.getElementById('sidebar');
    if (!sidebar) {
        console.error('Sidebar not found');
        return;
    }

    const logoutContainer = document.createElement('div');
    logoutContainer.id = 'logout-container';
    const logoutButton = document.createElement('button');
    logoutButton.textContent = 'Logout';
    logoutButton.onclick = logout;
    logoutContainer.appendChild(logoutButton);

    // Find the icons element
    const icons = sidebar.querySelector('.icons');
    
    // Insert the logout button before the icons
    if (icons) {
        sidebar.insertBefore(logoutContainer, icons);
    } else {
        console.error('Could not find proper position for logout button');
        sidebar.appendChild(logoutContainer);
    }
}

async function logout() {
    try {
        const response = await fetch('/api/logout', { method: 'POST' });
        if (response.ok) {
            isAdmin = false;
            alert('Logged out successfully');
            document.getElementById('logout-container').remove();
            showLoginForm();
            updateDemoList(); // Refresh the demo list to hide admin controls
            removeEditability(); // Remove edit buttons and editability when logging out
        } else {
            alert('Logout failed');
        }
    } catch (error) {
        console.error('Logout error:', error);
    }
}

// Demo management functions
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

// Content editing functions
function makeContentEditable() {
    if (!isAdmin) return;

    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach((card, index) => {
        const editButton = document.createElement('button');
        editButton.textContent = 'Edit';
        editButton.className = 'edit-button';
        editButton.onclick = () => toggleEditMode(card, index);
        card.insertBefore(editButton, card.firstChild);

        const saveButton = document.createElement('button');
        saveButton.textContent = 'Save';
        saveButton.className = 'save-button';
        saveButton.style.display = 'none';
        saveButton.onclick = () => saveChanges(card, index);
        card.insertBefore(saveButton, card.firstChild);

        // Ensure all elements are not editable by default
        const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
    });
}

function toggleEditMode(card, index) {
    if (!isAdmin) return;

    const editButton = card.querySelector('.edit-button');
    const saveButton = card.querySelector('.save-button');
    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');

    if (editButton.textContent === 'Edit') {
        editButton.textContent = 'Cancel';
        saveButton.style.display = 'inline-block';
        elements.forEach(el => {
            el.contentEditable = 'true';
            el.classList.add('editable');
        });
    } else {
        editButton.textContent = 'Edit';
        saveButton.style.display = 'none';
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
        location.reload(); // Revert changes without saving
    }
}

function removeEditability() {
    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach(card => {
        const editButton = card.querySelector('.edit-button');
        const saveButton = card.querySelector('.save-button');
        if (editButton) editButton.remove();
        if (saveButton) saveButton.remove();

        const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
    });
}

async function saveChanges(card, index) {
    if (!isAdmin) return;

    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
    const content = {};

    elements.forEach((el, i) => {
        content[`${el.tagName.toLowerCase()}_${i}`] = {
            outerHTML: el.outerHTML,
            text: el.innerText,
            href: el.tagName.toLowerCase() === 'a' ? el.getAttribute('href') : null
        };
    });

    try {
        const response = await fetch('/api/updateContent', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ cardIndex: index, content }),
        });

        if (response.ok) {
            alert('Content updated successfully');
            // Disable editing after saving
            elements.forEach(el => {
                el.contentEditable = 'false';
                el.classList.remove('editable');
            });
            const editButton = card.querySelector('.edit-button');
            const saveButton = card.querySelector('.save-button');
            editButton.textContent = 'Edit';
            saveButton.style.display = 'none';
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to update content');
        }
    } catch (error) {
        console.error('Error updating content:', error);
        alert('An error occurred while updating content');
    }
}

document.addEventListener('DOMContentLoaded', async function() {
    console.log('DOM fully loaded and molested');

    // Check if user is already logged in
    try {
        const response = await fetch('/api/checkAuth');
        const data = await response.json();
        if (data.isAdmin) {
            isAdmin = true;
            showLogoutButton();
            makeContentEditable();
        } else {
            isAdmin = false;
            showLoginForm();
            removeEditability(); // Error occurs here
        }
    } catch (error) {
        console.error('Error checking authentication:', error);
        showLoginForm();
        removeEditability(); // Error occurs here
    }

    // Set active navigation item
    setActiveNavItem();

    // Setup mobile menu
    setupMobileMenu();
    console.log('Mobile menu setup completed');

    // Setup search functionality
    const searchButton2 = document.getElementById('search-button2');
    const searchInput2 = document.getElementById('search-input2');

    if (searchButton2 && searchInput2) {
        searchButton2.addEventListener('click', performGameSearch);
        searchInput2.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                performGameSearch();
            }
        });
    }

    updateDemoList();

    // Set up modal functionality
    const modal = document.getElementById("myModal");
    if (modal) {
        const closeBtn = modal.querySelector(".close");
        if (closeBtn) {
            closeBtn.onclick = closeModal;
        }
        window.onclick = function(event) {
            if (event.target === modal) {
                closeModal();
            }
        };
    }

// Set up media item click listeners
const mediaItems = document.querySelectorAll('.screenshot-item, .video-item');
mediaItems.forEach(item => {
    item.addEventListener('click', function(e) {
        e.preventDefault();
        const src = this.getAttribute('data-src');
        const type = this.getAttribute('data-type');
        const title = this.getAttribute('data-title') || '';
        console.log('Clicked item:', this, 'src:', src, 'type:', type, 'title:', title); // More detailed logging
        if (src && type) {
            openModal(src, type, title);
        } else {
            console.error('Missing data attributes on clicked item', this);
        }
    });
});
});

// Function to handle smooth scrolling
function smoothScroll(target) {
    const element = document.querySelector(target);
    if (element) {
        window.scrollTo({
            top: element.offsetTop,
            behavior: 'smooth'
        });
    }
}

// Set up smooth scrolling for anchor links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function(e) {
        e.preventDefault();
        smoothScroll(this.getAttribute('href'));
    });
});

// Function to handle lazy loading of images
function lazyLoadImages() {
    const imgObserver = new IntersectionObserver((entries, observer) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                const img = entry.target;
                img.src = img.dataset.src;
                img.classList.remove('lazy');
                observer.unobserve(img);
            }
        });
    });

    const imgs = document.querySelectorAll('img.lazy');
    imgs.forEach(img => imgObserver.observe(img));
}

// Call lazy loading function when the page is loaded
if ('IntersectionObserver' in window) {
    window.addEventListener('load', lazyLoadImages);
} else {
    // Fallback for browsers that don't support IntersectionObserver
    window.addEventListener('load', () => {
        document.querySelectorAll('img.lazy').forEach(img => {
            img.src = img.dataset.src;
            img.classList.remove('lazy');
        });
    });
}

// Export functions that might be used elsewhere
window.updateDemoList = updateDemoList;
window.performGameSearch = performGameSearch;
window.openModal = openModal;
window.closeModal = closeModal;
window.smoothScroll = smoothScroll;
window.lazyLoadImages = lazyLoadImages;
window.deleteDemo = deleteDemo;
window.setupMobileMenu = setupMobileMenu;
window.setActiveNavItem = setActiveNavItem;

// Additional event listeners
window.addEventListener('load', function() {
    console.log('Window loaded, calling setupMobileMenu');
    setupMobileMenu();
    setActiveNavItem();
});

window.addEventListener('resize', function() {
    if (window.innerWidth <= 768) {
        console.log('Window resized to mobile view, calling setupMobileMenu');
        setupMobileMenu();
    }
});

// Ensure the mobile menu works even if the DOM is already loaded
if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('DOM already complete, calling setupMobileMenu immediately and preparing to find the snake :)');
    setupMobileMenu();
    setActiveNavItem();
}