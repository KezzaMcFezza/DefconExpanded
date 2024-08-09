// Global variables
let allDemos = []; 
let soundVolume = 0.1;

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
                ${getPlayerData(demo.name)}
            </table>
            <a href="https://defconexpanded.com/download/${demo.name}" class="btn-download foo"> Download
            </a>
            <a href="https://defconexpanded.com/homepage/matchroom" class="btn-matchroom foo"> Matchroom </a>
        </div>
    `;
    return demoCard;
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

function getDuration(size) {
    const minutes = Math.floor(Math.random() * (200 - 45 + 1)) + 45;
    
    if (minutes >= 60) {
        const hours = Math.floor(minutes / 60);
        const remainingMinutes = minutes % 60;
        return `${hours}h ${remainingMinutes}m`;
    } else {
        return `${minutes}m`;
    }
}

function getPlayerData(demoName, numPlayers = 8) {
    const territories = ['NA', 'EU', 'SA', 'RU', 'AF', 'EA', 'WA', 'AA'];
    const prefixes = [  'Nuclear', 'Atomic', 'Stealth', 'Cyber', 'Tactical', 'Strategic', 'Rogue', 'Shadow', 'Sieverts',
        'Covert', 'Phantom', 'Ghost', 'Recon', 'Assault', 'Havoc', 'Chaos', 'Mayhem', 'Vortex',
        'Laser', 'Plasma', 'Ion', 'Photon', 'Quantum', 'Fusion', 'Neutrino', 'Electron', 'Proton',
        'Gravity', 'Singularity', 'Nebula', 'Nova', 'Pulsar', 'Quasar', 'Supernova', 'Hypernova', 'Magnetar',
        'Void', 'Abyss', 'Oblivion', 'Nihil', 'Ether', 'Astral', 'Cosmic', 'Galactic', 'Celestial',
        'Titan', 'Colossus', 'Behemoth', 'Leviathan', 'Kraken', 'Hydra', 'Phoenix', 'Chimera', 'Sphinx',
        'Achtlos', 'Alien', 'an4rki', 'astrogal', 'andii', 'AndrewW', 'Annorax', 'Antzz', 'ash1',
        'ausjoel', 'AusMade', 'Ausroachman', 'AussiePenguin96', 'Ayz', 'barrymoves', 'Beefychops', 'Beno-ski',
        'BIG DWIFTER', 'bigknox', 'bkenh', 'BlackLotus', 'Boydio', 'Brothekid10', 'Buckets23', 'BURGER R1NGS',
        'CAD', 'CCAJ', 'CCsPack', 'chromium', 'chris_e_fresh', 'chris_e_fresh', 'colossus1509', 'Crazy CS',
        'Daelhoof', 'DUS SOULWOUND', 'deathschild', 'DES10', 'ZacB', 'D3lusion', 'Daniel Bedard', 'Darkrider',
        'darktranq', 'Dave Davids', 'davidhes', 'dazeld', 'DazzB66', 'Deneo123', 'DH Andrew', 'Didds',
        'DiggeR', 'diilpickle', 'Dingotech', 'DisasterArea', 'diux', 'doublewishbone', 'Dylsmangan', 'dkNigs',
        'Defy9', 'Deadly Dozeball', 'dqtl74', 'Economic', 'Epic', 'Egy Kangaroo', 'MattTheExpat', 'error-id10t',
        'F1 Viking', 'Fallz', 'Frost2468', 'fenryr', 'G4m3 0n', 'gameoverjack', 'garry75', 'ghostdog',
        'Gistane', 'GobiasIndustries', 'GokhanH', 'Guided Light', 'Grafik', 'Gristy', 'gothioso', 'Harry Callahan',
        'HBCJ', 'heyitskayxx', 'Hot turtle', 'Houda', 'hsvjez', 'The_Aus_Hulk', 'hyperactive', 'ike-sp',
        'interfreak', 'impatient', 'Jason87', 'Jonesy7707', 'JFin', 'Joshbartos', 'Joshaldo', 'jparton',
        'jazzyjeff', 'johno686', 'jooz', 'JRAW', 'JT_77', 'JLC215', 'kirbasin', 'Kire', 'krammis76',
        'Labmonkey', 'Lammiwinks', 'Lindsay', 'Lirrik', 'ljayljay', 'Lodan', 'Lord Byoss', 'Levinson-Durbin', 'mackaxx',
        'Mal68', 'Malibu2', 'Mangomadness', 'MadCheese', 'mazman', 'McCoyPauley78', 'McDethWivFries', 'melbounrefan',
        'micknq', 'MoabBoy', 'Moofius', 'mordinho', 'morph0', 'mozdesigns', 'mwblink', 'myzzz', 'Micdeez',
        'n3o-dystopia', 'nodii', 'Neckron', 'NinjaPizza', 'Nitro', 'Namasaki', 'Notsuree', 'nuxx', 'Noysy',
        'Never_Enough', 'ocat1979', 'OneTrackLover', 'OzDJ', 'OVRLOAD', 'parramaniac', 'Prelly99', 'Phalaxis',
        'pharmerdan', 'Philip J Farnsworth', 'Phreak09', 'plainfaced', 'Pottymouth', 'prenticem', 'Prang',
        'PrimeStormRider', 'PsychoDog', 'PsychoticOrc', 'pupmeister', 'Raima', 'razalom', 'RazCo', 'remaKe', 
        'Reveler', 'ricdam', 'rhinorizy', 'Riore', 'rjrgmc28', 'robbie y', 'RonnyBoi', 'Rumple', 'Russdg',
        'RynosaurusRex', 'Rysith', 'Rysup', 'sacka', 'salsicce77', 'SaminAus88', 'sammo', 'SandyRivers',
        'savior', 'Schikitar', 'Scubafinch', 'Shifty-au', 'ScorchedMirth', 'scottchicken', 'sesquipedalian',
        'ShayneRarma', 'Shmicks', 'Shrugal844', 'Sifheal', 'SJRWalker', 'slawsonftw', 'Solidus82', 'Sovi3t',
        'SPUD', 'steamtrain13583', 'sTeeL', 'Stormyt', 'Storm1981', 'Streetlights', 'swkotor', 'Syrup',
        'Syvergy', 'shazie', 'Trans_JimJam', 'TheImposter', 'thatsnazzydude', 'TheHighHorse', 'thinkbigbw',
        'Thommo77', 'Tiberius', 'tomee', 'Toasty Warm Hamster', 'trents', 'tubs105', 'Tuffers79', 'TwistedGecko',
        'UnciasDream', 'UnforgivnJudas', 'Vaderz', 'Valac', 'VenularSundew0', 'vinnie05', 'Vormund', 'walrus67',
        'Waughy', 'well_spoken', 'Westone', 'WhiskyAC', 'Wickzki', 'wildliquid', 'willy1818', 'wilsd004',
        'Windows1', 'Wraith', 'd_b', 'Xuak', 'Zarby', 'ZeeKor'];
    const suffixes = ['Xx'];
    const numbers = ['', '1', '2', '3', '4', '5', '42', '69', '99', '007'];

    function generateGamertag() {
        const prefix = prefixes[Math.floor(Math.random() * prefixes.length)];
        const suffix = suffixes[Math.floor(Math.random() * suffixes.length)];
        const number = numbers[Math.floor(Math.random() * numbers.length)];
        return `${prefix}${suffix}${number}`;
    }

    // Shuffle the territories array
    const shuffledTerritories = territories.slice().sort(() => Math.random() - 0.5);

    const players = Array.from({ length: numPlayers }, (_, index) => ({
        name: generateGamertag(),
        territory: shuffledTerritories[index % territories.length], // Assign territories in order from shuffled array
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

function updateDemoList() {
    fetch('https://defconexpanded.com/demos')
        .then(response => response.json())
        .then(demos => {
            allDemos = demos; 
            const gamesPlayed = document.getElementById('games-played');
            if (gamesPlayed) {
                gamesPlayed.textContent = `TOTAL GAMES PLAYED: ${demos.length}`;
            }
            showPage(1, allDemos);
        })
        .catch(error => console.error('Error fetching demos:', error));
}

function performGameSearch() {
    const searchInput = document.getElementById('search-input2');
    if (!searchInput) return;
    const searchTerm = searchInput.value.toLowerCase();
    const filteredDemos = allDemos.filter(demo => 
        demo.name.toLowerCase().includes(searchTerm) ||
        formatBytes(demo.size).toLowerCase().includes(searchTerm) ||
        new Date(demo.date).toLocaleDateString().toLowerCase().includes(searchTerm)
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

document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM fully loaded and parsed');

    // Set active navigation item
    setActiveNavItem();

    // Setup mobile menu
    setupMobileMenu();
    console.log('Mobile menu setup completed');

    // Existing functionality
    const searchButton = document.getElementById('search-button');
    const searchInput = document.getElementById('search-input');
    const searchButton2 = document.getElementById('search-button2');
    const searchInput2 = document.getElementById('search-input2');

    if (searchButton && searchInput) {
        searchButton.addEventListener('click', performMainSearch);
        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                performMainSearch();
            }
        });
    }

    if (searchButton2 && searchInput2) {
        searchButton2.addEventListener('click', performGameSearch);
        searchInput2.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                performGameSearch();
            }
        });
    }

    updateDemoList();

    const scrollLinks = document.getElementsByClassName('scroll-to-bottom');
    for (let i = 0; i < scrollLinks.length; i++) {
        scrollLinks[i].addEventListener('click', function(e) {
            e.preventDefault();
            const win = window.open(this.href, '_blank');
            if (win) {
                win.addEventListener('load', function() {
                    win.scrollTo(0, win.document.body.scrollHeight);
                });
            }
        });
    }
    
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
    const mediaItems = document.querySelectorAll('.featured-video, .video-item, .screenshot-item');
    mediaItems.forEach(item => {
        item.addEventListener('click', function(e) {
            e.preventDefault();
            const src = this.getAttribute('data-src');
            const type = this.getAttribute('data-type');
            const title = this.getAttribute('data-title') || '';
            console.log('Clicked item:', src, type, title); // Debug log
            if (src && type) {
                openModal(src, type, title);
            } else {
                console.error('Missing data attributes on clicked item');
            }
        });
    });

    // Perform search on page load if query parameter is present
    const urlParams = new URLSearchParams(window.location.search);
    const query = urlParams.get('q');
    if (query) {
        const searchInput = document.getElementById('search-input');
        if (searchInput) {
            searchInput.value = query;
        }
        performSearch(query);
    }
});

// Call setActiveNavItem immediately in case the DOM is already loaded
setActiveNavItem();

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

// Export functions and variables that might be used elsewhere
window.updateDemoList = updateDemoList;
window.performGameSearch = performGameSearch;
window.openModal = openModal;
window.closeModal = closeModal;
window.smoothScroll = smoothScroll;
window.lazyLoadImages = lazyLoadImages;
window.setupMobileMenu = setupMobileMenu;
window.setActiveNavItem = setActiveNavItem;

// Call setupMobileMenu when the window loads
window.addEventListener('load', function() {
    console.log('Window loaded, calling setupMobileMenu');
    setupMobileMenu();
    setActiveNavItem();
});

// Additional event listener for resize events
window.addEventListener('resize', function() {
    if (window.innerWidth <= 768) {
        console.log('Window resized to mobile view, calling setupMobileMenu');
        setupMobileMenu();
    }
});

// Ensure the mobile menu works even if the DOM is already loaded
if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('DOM already complete, calling setupMobileMenu immediately');
    setupMobileMenu();
    setActiveNavItem();
}