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
    const demosPerPage = 6;
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
                <span class="game-duration">(${getDuration(demo.size)})</span>
            </p>
            <table class="game-results">
                <tr>
                    <th>Player</th>
                    <th>Territory</th>
                    <th>Score</th>
                </tr>
                ${getPlayerData(demo.name)}
            </table>
            <a href="https://defconexpanded.com/download/${demo.name}" class="btn-download foo">
                <img src="https://defconexpanded.com/images/dedcon.ico" alt="" class="download-icon"> Download
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
    gameDate.setMinutes(gameDate.getMinutes() + Math.floor(Math.random() * 60) + 15); // Random duration between 15-75 minutes
    const endTime = gameDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true });
    return `${startTime} — ${endTime}`;
}

function getDuration(size) {
    // Estimate duration based on file size (this is a rough approximation)
    const minutes = Math.max(15, Math.min(75, Math.floor(size / 1024 / 1024))); // 1 MB ≈ 1 minute, min 15, max 75
    return `${minutes} min`;
}

function getPlayerData(demoName) {
    // This is a placeholder. In a real scenario, you'd fetch this data from the server
    const players = [
        { name: "Player1", territory: "NA", score: Math.floor(Math.random() * 200) - 50 },
        { name: "Player2", territory: "EU", score: Math.floor(Math.random() * 200) - 50 },
        { name: "Player3", territory: "SA", score: Math.floor(Math.random() * 200) - 50 },
        { name: "Player4", territory: "RU", score: Math.floor(Math.random() * 200) - 50 },

    ];
    return players.map(player => `
        <tr>
            <td>${player.name}</td>
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
                gamesPlayed.textContent = `Total GamEs PlayEd: ${demos.length}`;
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

function addJSONLD() {
    const script = document.createElement('script');
    script.type = 'application/ld+json';
    const currentPath = window.location.pathname;
    let jsonLDContent;

    switch(currentPath) {
        case '/':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebSite",
                "url": "https://defconexpanded.com/",
                "name": "DEFCON Expanded: A DEFCON Modding Community",
                "description": "The Home for DEFCON Mods and Demos"
            };
            break;
        case '/about':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebPage",
                "url": "https://defconexpanded.com/about",
                "name": "About DEFCON Expanded",
                "description": "Learn about DEFCON Expanded, a modding community for DEFCON"
            };
            break;
        case '/experimental':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebPage",
                "url": "https://defconexpanded.com/experimental",
                "name": "DEFCON Expanded Experimental Builds",
                "description": "Find out the new builds we are working on."
            };
            break;
        case '/resources':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebPage",
                "url": "https://defconexpanded.com/resources",
                "name": "DEFCON Expanded Downloads",
                "description": "Download DEFCON resources for 8, 10, and 16 player games."
            };
            break;
        case '/news':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebPage",
                "url": "https://defconexpanded.com/news",
                "name": "DEFCON Expanded News",
                "description": "Latest news and updates for DEFCON Expanded, including information on new mods and features."
            };
            break;
        case '/media':
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebPage",
                "url": "https://defconexpanded.com/media",
                "name": "DEFCON Expanded Media Gallery",
                "description": "View screenshots and videos of DEFCON Expanded mods and Gameplay."
            };
            break;
        default:
            jsonLDContent = {
                "@context": "https://schema.org",
                "@type": "WebSite",
                "url": "https://defconexpanded.com/",
                "name": "DEFCON Expanded: A DEFCON Modding Community",
                "description": "Experimental DEFCON Mods"
            };
    }

    script.text = JSON.stringify(jsonLDContent);
    document.head.appendChild(script);
}

function openModal(src, type, title) {
    const modal = document.getElementById("myModal");
    const modalImg = document.getElementById("modalImg");
    const modalVideo = document.getElementById("modalVideo");
    if (!modal || !modalImg || !modalVideo) return;

    modal.style.display = "flex";
    if (type === 'img') {
        modalImg.style.display = "block";
        modalVideo.style.display = "none";
        modalImg.src = src;
        modalImg.alt = title;
    } else {
        modalImg.style.display = "none";
        modalVideo.style.display = "block";
        modalVideo.src = src;
        modalVideo.title = title;
    }
}

function closeModal() {
    const modal = document.getElementById("myModal");
    const modalVideo = document.getElementById("modalVideo");
    if (!modal || !modalVideo) return;
    modal.style.display = "none";
    modalVideo.pause();
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

document.addEventListener('DOMContentLoaded', function() {
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
    setInterval(updateDemoList, 30000);

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
        const mediaItems = document.querySelectorAll('.media-item, .screenshot-item, .video-item');
        mediaItems.forEach(item => {
            item.addEventListener('click', function() {
                const src = this.getAttribute('data-src');
                const type = this.getAttribute('data-type');
                const title = this.getAttribute('data-title') || '';
                openModal(src, type, title);
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