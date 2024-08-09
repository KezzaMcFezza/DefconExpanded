// Global variables
let allDemos = []; 
let soundVolume = 0.1;

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
        teamColors = ['#d13b3b', '#d13b3b', '#d13b3b', '#d13b3b', '#40d340', '#40d340', '#40d340', '#40d340'];
    } else if (demoName.toLowerCase().includes('2v2v2v2')) {
        teamColors = ['#d13b3b', '#d13b3b', '#40d340', '#40d340', '#3488d6', '#3488d6', '#bb7b3b', '#bb7b3b'];
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

function performMainSearch() {
    const searchInput = document.getElementById('search-input');
    if (!searchInput) return;
    const searchTerm = searchInput.value;
    if (searchTerm.trim() === '') return;
    window.location.href = `https://defconexpanded.com/searchresults?q=${encodeURIComponent(searchTerm)}`;
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

function performSearch(query) {
    query = query.toLowerCase();
    const searchResults = document.getElementById('searchresults');
    if (!searchResults) return;
    searchResults.innerHTML = '<p>Searching...</p>';

    fetch('/search-index.json')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(searchIndex => {
            console.log('Fetched search index:', searchIndex);
            searchResults.innerHTML = '';
            
            const filteredResults = searchIndex.flatMap(page => {
                let pageMatches = [];

                const checkAndAddMatch = (condition, snippet) => {
                    if (condition) {
                        pageMatches.push({
                            url: page.url,
                            title: page.title,
                            snippet: snippet || page.metaDescription
                        });
                    }
                };

                checkAndAddMatch(
                    page.title.toLowerCase().includes(query) ||
                    page.h1.toLowerCase().includes(query) ||
                    page.metaDescription.toLowerCase().includes(query)
                );

                page.snippets.forEach(snippet => {
                    checkAndAddMatch(snippet.toLowerCase().includes(query), snippet);
                });

                if (page.sections) {
                    page.sections.forEach(section => {
                        if (section.title.toLowerCase().includes(query) || 
                            section.content.toLowerCase().includes(query)) {
                            pageMatches.push({
                                url: `${page.url}#${section.id}`,
                                title: `${page.title} - ${section.title}`,
                                snippet: section.content
                            });
                        }
                    });
                }

                checkAndAddMatch(
                    page.keywords.some(keyword => keyword.toLowerCase().includes(query)),
                    `This page contains relevant keywords: ${page.keywords.join(", ")}`
                );

                return pageMatches;
            });

            if (filteredResults.length === 0) {
                searchResults.innerHTML = '<p>No results found.</p>';
            } else {
                const resultList = document.createElement('ul');
                filteredResults.forEach(result => {
                    const listItem = document.createElement('li');
                    
                    const link = document.createElement('a');
                    link.href = result.url.startsWith('/') ? result.url : '/' + result.url;
                    link.textContent = result.title;
                    listItem.appendChild(link);
                    
                    const snippet = document.createElement('p');
                    const highlightedSnippet = result.snippet.replace(
                        new RegExp(query, 'gi'),
                        match => `<span class="search-highlight">${match}</span>`
                    );
                    snippet.innerHTML = highlightedSnippet;
                    listItem.appendChild(snippet);
                    
                    resultList.appendChild(listItem);
                });
                searchResults.innerHTML = '';
                searchResults.appendChild(resultList);
            }
        })
        .catch(error => {
            console.error('Error performing search:', error);
            searchResults.innerHTML = `<p>An error occurred while searching: ${error.message}. Please try again.</p>`;
        });
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

    addJSONLD();

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

const terminal = document.getElementById('terminal');
const output = document.getElementById('output');
const input = document.getElementById('cmd-input');
const inputLine = document.getElementById('input-line');
const cursor = document.getElementById('cursor');

const files = {
    '8playerv2.8.7': 'files/DEFCON 8 Players Alpha v2.8.7.zip',
    '8playerv2.8.6': 'files/DEFCON 8 Players Alpha v2.8.6.zip',
    '8playerv2.8.3': 'files/DEFCON 8 Players Prerelease v2.8.3.zip',
    '8playerv2.8.2': 'files/DEFCON 8 Players Prerelease v2.8.2.zip',
    '8playerv2.8.1': 'files/DEFCON 8 Players Prerelease v2.8.1.zip',
    '8playerv2.8.0': 'files/DEFCON 8 Players Prerelease v2.8.0.zip',
    '10playerv2.10.5': 'files/DEFCON 10 Players Alpha v2.10.5.zip',
    '10playerv2.10.4': 'files/DEFCON 10 Players Alpha v2.10.4.zip',
    '10playerv2.10.1': 'files/DEFCON 10 Players Prerelease v2.10.1.zip',
    '10playerv2.10.0': 'files/DEFCON 10 Players Prerelease v2.10.0.zip',
    '16playerv2.16.4': 'files/DEFCON 16 Players Alpha v2.16.4.zip',
    '16playerv2.16.3': 'files/DEFCON 16 Players Alpha v2.16.3.zip',
    '16playerv2.16.0': 'files/DEFCON 16 Players Prerelease v2.16.0.zip',
    // ... (other files)
};

function appendOutput(text, className = '') {
    const line = document.createElement('div');
    line.className = `output-line ${className}`;
    line.innerHTML = text;
    output.appendChild(line);
    terminal.scrollTop = terminal.scrollHeight;
}

function downloadFile(file) {
    const link = document.createElement('a');
    link.href = `https://defconexpanded.com/${files[file]}`;
    link.download = file;
    link.target = '_blank';
    link.style.display = 'none';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

function playSound(sound, volume = 0.1) {
    const audio = new Audio(sound);
    audio.volume = volume;
    audio.play();
}

function handleCommand(cmd) {
    const parts = cmd.split(' ');
    const command = parts[0].toLowerCase();

    switch (command) {
        case 'help':
            appendOutput(`
                <div class="command-help">
                    <p><span class="command">list</span> - Show all available files</p>
                    <p><span class="command">download [filename]</span> - Download the specified file</p>
                    <p><span class="command">clear</span> - Clear the terminal screen</p>
                    <p><span class="command">exit</span> - Exit the command interface</p>
                    <p><span class="command">credits</span> - Show the credits for the website</p>
                </div>
            `);
            break;
        case 'list':
            const filesArray = Object.keys(files);
            const columns = Math.ceil(filesArray.length / 15);
            const fileList = document.createElement('div');
            fileList.className = 'file-list';

            let currentColumn = 0;
            let currentIndex = 0;

            function listNextFile() {
                if (currentIndex < filesArray.length) {
                    const file = filesArray[currentIndex];
                    const fileItem = document.createElement('div');
                    fileItem.className = 'file-item';
                    fileItem.textContent = file;
                    fileItem.addEventListener('click', () => {
                        input.value = `download ${file}`;
                        input.focus();
                        updateCursorPosition();
                    });

                    if (currentColumn === 0) {
                        const column = document.createElement('div');
                        column.className = 'file-column';
                        fileList.appendChild(column);
                    }

                    fileList.children[currentColumn].appendChild(fileItem);

                    currentIndex++;
                    if (currentIndex % 15 === 0) {
                        currentColumn++;
                    }

                    playSound('https://defconexpanded.com/sounds/chat.ogg', soundVolume);
                    setTimeout(listNextFile, 100);
                } else {
                    appendOutput(fileList.outerHTML);
                }
            }

            listNextFile();
            break;
        case 'download':
            if (parts.length < 2) {
                appendOutput('Error: Please specify a file to download', 'error');
            } else {
                const file = parts[1];
                if (files[file]) {
                    appendOutput(`Downloading ${file}...`);
                    downloadFile(file);
                    playSound('https://defconexpanded.com/sounds/chat.ogg', soundVolume);
                } else {
                    appendOutput(`Error: File '${file}' not found`, 'error');
                }
            }
            break;
        case 'clear':
            output.innerHTML = '';
            break;
        case 'exit':
            window.location.href = 'https://defconexpanded.com';
            break;
        case 'credits':
            appendOutput(`
                <div class="credits">
                    <p>DEFCON Expanded Credits</p>
                    <ul>
                        <li>Website Development: KezzaMcFezza, Devil, Nexustini</li>
                        <li>Mod Development: Sievert, FasterThanRaito, Wan May</li>
                        <li>Game Testers: KezzaMcFezza, Sievert, FasterThanRaito, Demo, Nicole, Ben_Herr, zak, Meme Warrior, Erronous, Gyarados, Idealist, Jackolite, Laika, Very Fine Art, Lævateinn </li>
                        <li>Special Thanks: The DEFCON Community</li>
                    </ul>
                </div>
            `);
            break;
        default:
            appendOutput(`Error: Unknown command '${command}'`, 'error');
            appendOutput("Type 'help' for a list of available commands.");
    }
}

if (input) {
    input.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
            const cmd = input.value;
            appendOutput(`> ${cmd}`);
            handleCommand(cmd);
            input.value = '';
            updateCursorPosition();
            playSound('https://defconexpanded.com/sounds/chat.ogg', soundVolume);
        }
    });

    input.addEventListener('input', updateCursorPosition);
    input.addEventListener('keydown', updateCursorPosition);
}

document.addEventListener('click', function() {
    if (input) input.focus();
});

function updateCursorPosition() {
    if (input && cursor) {
        const inputRect = input.getBoundingClientRect();
        const inputStyle = window.getComputedStyle(input);
        const letterWidth = parseFloat(inputStyle.fontSize) * 0.6;
        const textWidth = input.value.length * letterWidth;
        cursor.style.left = `${textWidth + inputRect.left}px`;
    }
}

// Updated boot sequence with varying delays
function bootSequence() {
    const bootMessages = [
        "Loading file system...",
        "Establishing secure connection...",
        "Access granted. You now have access to all the website's files!"
    ];

    const delays = [2000, 4000, 200];

    let i = 0;
    function displayNextMessage() {
        if (i < bootMessages.length) {
            appendOutput(bootMessages[i]);
            playSound('https://defconexpanded.com/sounds/chat.ogg', soundVolume);
            setTimeout(displayNextMessage, delays[i] || 3000);
            i++;
        } else {
            if (inputLine) inputLine.style.display = 'flex';
            if (input) {
                input.focus();
                updateCursorPosition();
            }
            appendOutput("Type 'help' for a list of available commands.");
        }
    }

    displayNextMessage();
}

if (terminal) {
    window.onload = bootSequence;
}

// Export functions and variables that might be used elsewhere
window.updateDemoList = updateDemoList;
window.performMainSearch = performMainSearch;
window.performGameSearch = performGameSearch;
window.addJSONLD = addJSONLD;
window.openModal = openModal;
window.closeModal = closeModal;
window.performSearch = performSearch;
window.smoothScroll = smoothScroll;
window.lazyLoadImages = lazyLoadImages;
window.handleCommand = handleCommand;
window.playSound = playSound;
window.updateCursorPosition = updateCursorPosition;
window.bootSequence = bootSequence;
window.setupMobileMenu = setupMobileMenu;

// Call setupMobileMenu when the window loads
window.addEventListener('load', function() {
    console.log('Window loaded, calling setupMobileMenu');
    setupMobileMenu();
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
}