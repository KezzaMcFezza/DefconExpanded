let soundVolume = 0.1;
window.isAdmin = false; 

function isUserLoggedIn() {
  return !!localStorage.getItem('token');
}

function setActiveNavItem() {
    const currentPath = window.location.pathname;
    const navItems = document.querySelectorAll('#sidebar .list-items li');
    
    navItems.forEach(item => {
        const link = item.querySelector('a');
        item.classList.remove('active');
        if (currentPath === link.getAttribute('href') || currentPath.startsWith(link.getAttribute('href') + '/')) {
            item.classList.add('active');
            if (item.classList.contains('dropdown')) {
                item.querySelector('.dropdown-content').classList.add('show');
            }
        }
    });
}

async function likeMod(modId) {
  if (!isUserLoggedIn()) {
      alert('You need to be logged in to like mods.');
      return { ok: false };
  }
  return fetch(`/api/mods/${modId}/like`, { 
      method: 'POST',
      headers: {
          'Authorization': `Bearer ${localStorage.getItem('token')}`
      }
  });
}

async function favoriteMod(modId) {
  if (!isUserLoggedIn()) {
      alert('You need to be logged in to favorite mods.');
      return { ok: false };
  }
  return fetch(`/api/mods/${modId}/favorite`, { 
      method: 'POST',
      headers: {
          'Authorization': `Bearer ${localStorage.getItem('token')}`
      }
  });
}

function updateLikeButton(button) {
  button.classList.toggle('liked');
}

function updateFavoriteButton(button) {
  button.classList.toggle('favorited');
}

document.addEventListener('DOMContentLoaded', function() {
  const profileLinks = document.querySelectorAll('.profile-link');
  
  profileLinks.forEach(profileLink => {
    fetch('/api/current-user')
      .then(response => {
        if (!response.ok) {
          throw new Error('Not authenticated');
        }
        return response.json();
      })
      .then(data => {
        if (data.user && data.user.username) {
          profileLink.href = `/profile/${data.user.username}`;
        } else {
          profileLink.href = '#'; 
        }
      })
      .catch(error => {
        console.error(error);
      });

    profileLink.addEventListener('click', function(e) {
      if (!profileLink.href || profileLink.href === '#') {
        e.preventDefault();
      } else {
        window.location.href = profileLink.href;
      }
    });
  });
});

  const loggedInUsername = document.getElementById('logged-in-username');
  if (loggedInUsername) {
    fetch('/api/current-user')
      .then(response => response.json())
      .then(data => {
        if (data.user && data.user.username) {
          loggedInUsername.textContent = data.user.username;
        }
      })
  }


  document.addEventListener('DOMContentLoaded', async function() {
    const loggedOutActions = document.querySelectorAll('.logged-out-actions');
    const loggedInActions = document.querySelectorAll('.logged-in-actions');
    const loggedInUsernameSpan = document.querySelectorAll('#logged-in-username');
    const signoutButtons = document.querySelectorAll('.signout'); 
  
    function updateUIForLoginState(isLoggedIn, loggedInUsername) {
        if (isLoggedIn) {
            loggedOutActions.forEach(action => action.style.display = 'none');
            loggedInActions.forEach(action => action.style.display = 'flex');
            loggedInUsernameSpan.forEach(span => span.textContent = loggedInUsername);
        } else {
            loggedOutActions.forEach(action => action.style.display = 'flex');
            loggedInActions.forEach(action => action.style.display = 'none');
        }
    }
  
    fetch('/api/checkAuth')
        .then(response => response.json())
        .then(data => {
            if (data.isLoggedIn) {
                localStorage.setItem('token', data.token);
            }
            updateUIForLoginState(data.isLoggedIn, data.username);
        });
  
    if (signoutButtons.length > 0) {
        signoutButtons.forEach(signoutButton => {
            signoutButton.addEventListener('click', async function(event) {
                event.preventDefault();
                const confirmLogout = await confirm("Are you sure you want to log out?");
                if (confirmLogout) {
                    fetch('/api/logout', { method: 'POST' })
                        .then(response => response.json())
                        .then(async data => {
                            if (data.message === 'Logged out successfully') {
                                updateUIForLoginState(false);
                                await alert("You have been successfully logged out.");
                            }
                        });
                }
            });
        });
    }
  });
  


  function getNextSunday() {
    const today = new Date();
    const nextSunday = new Date(today);
    const daysUntilSunday = 7 - today.getDay();
    
    // If today is Sunday and it's before 3 PM EST, use today
    if (today.getDay() === 0) {
        const estHour = today.getUTCHours() - 5; // Convert UTC to EST
        if (estHour < 15) { // Before 3 PM EST
            return today;
        }
    }
    
    // Add days until next Sunday
    nextSunday.setDate(today.getDate() + (daysUntilSunday % 7));
    return nextSunday;
}

function formatDateForEvent(date) {
    const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    const day = date.getDate();
    const month = months[date.getMonth()];
    
    // Function to add ordinal suffix
    function getOrdinalSuffix(n) {
        const s = ['th', 'st', 'nd', 'rd'];
        const v = n % 100;
        return n + (s[(v - 20) % 10] || s[v] || s[0]);
    }
    
    return `${month} ${getOrdinalSuffix(day)}`;
}

// Replace the static discordEvent object with a function
function getDiscordEvent() {
    const nextSunday = getNextSunday();
    return {
        title: "Community Game Night",
        date: `Sunday, ${formatDateForEvent(nextSunday)}`,
        time: "3PM EST"
    };
}

function updateDiscordWidget() {
    fetch('/api/discord-widget')
        .then(response => response.json())
        .then(data => {
            const widgetContainer = document.getElementById('discord-widget');
            if (!widgetContainer) return;

            // Get the current event details
            const discordEvent = getDiscordEvent();

            let html = `
                <div class="discord-widget">
                    <div class="discord-header">
                        <img src="/images/discord-logo.png" alt="Discord" class="discord-logo">
                        <span class="discord-title"></span>
                        <span class="discord-online-count">${data.presence_count} Online</span>
                    </div>
                    <div class="discord-event">
                        <h3 class="event-title">${discordEvent.title}</h3>
                        <p class="event-date">${discordEvent.date}</p>
                        <div class="event-time-div">
                            <p class="event-time">${discordEvent.time}</p>
                            <a href="${data.instant_invite}" target="_blank" rel="noopener noreferrer" class="discord-join-button">Join Discord</a>
                        </div>
                    </div>
                </div>
            `;

            widgetContainer.innerHTML = html;
        });
}

updateDiscordWidget();
setInterval(updateDiscordWidget, 5 * 60 * 1000);

function setupMobileMenu() {
    const sidebar = document.getElementById('sidebar');
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');
    const dropdowns = sidebar.querySelectorAll('.dropdown');

    if (title && listItems) {
        title.addEventListener('click', function(e) {
            e.preventDefault();
            listItems.classList.toggle('show');
            title.classList.toggle('menu-open'); 
        });

        listItems.addEventListener('click', function(e) {
            if (e.target.tagName === 'A' && !e.target.parentElement.classList.contains('dropdown')) {
                listItems.classList.remove('show');
                title.classList.remove('menu-open'); 
            }
        });

        dropdowns.forEach(dropdown => {
            const dropdownLink = dropdown.querySelector('a');
            const dropdownContent = dropdown.querySelector('.dropdown-content');
            dropdownLink.addEventListener('click', function(e) {
                e.preventDefault();
                dropdownContent.classList.toggle('show');
                dropdowns.forEach(otherDropdown => {
                    if (otherDropdown !== dropdown) {
                        otherDropdown.querySelector('.dropdown-content').classList.remove('show');
                    }
                });
            });
        });
    } else {
    }
}

function togglePatchNotes(button) {
    const details = button.closest('.patchnote-content').querySelector('.patchnote-details');
    if (details.style.display === 'none') {
      details.style.display = 'block';
      button.textContent = 'Hide patch notes';
    } else {
      details.style.display = 'none';
      button.textContent = 'View patch notes';
    }
  }

function getPageName() {
    const path = window.location.pathname;
    const pages = ['index.html', 'about.html', 'resources.html', 'news.html', 'laikasdefcon.html', '404.html', 'media.html', 'matchroom.html'];
    let pageName = pages.find(page => path.endsWith(page));
    
    if (!pageName) {
        pageName = path === '/' ? 'index.html' : path.split('/').pop() + '.html';
    }
    
    return pageName;
}

document.addEventListener('DOMContentLoaded', async function() {

    setActiveNavItem();
    setupMobileMenu();

    const modListContainer = document.getElementById('mod-list');
    if (modListContainer) {
        const modType = modListContainer.dataset.modType;
        if (modType) {
            setupModFilters(modType);
        }
    }

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

    const bugReportForm = document.getElementById('bug-report-form');
    if (bugReportForm) {
        bugReportForm.addEventListener('submit', submitBugReport);
    }
});

function smoothScroll(target) {
    const element = document.querySelector(target);
    if (element) {
        window.scrollTo({
            top: element.offsetTop,
            behavior: 'smooth'
        });
    }
}

document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function(e) {
        e.preventDefault();
        smoothScroll(this.getAttribute('href'));
    });
});

window.smoothScroll = smoothScroll;
window.setupMobileMenu = setupMobileMenu;
window.setActiveNavItem = setActiveNavItem;

window.addEventListener('load', function() {
    setupMobileMenu();
    setActiveNavItem();
});

window.addEventListener('resize', function() {
    if (window.innerWidth <= 768) {
        setupMobileMenu();
    }
});

if (document.readyState === 'complete' || document.readyState === 'interactive') {
    setupMobileMenu();
    setActiveNavItem();
}

async function submitBugReport(event) {
    event.preventDefault();

    const bugTitle = document.querySelector('input[name="bug-title"]').value;
    const bugDescription = document.querySelector('textarea[name="bug-description"]').value;

    try {
        const response = await fetch('/api/report-bug', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ bugTitle, bugDescription }),
        });

        if (response.ok) {
            const data = await response.json();
            alert(data.message);
            event.target.reset(); 
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to submit bug report');
        }
    } catch (error) {
        alert('An error occurred while submitting the bug report');
    }
}

let allResources = [];

function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

async function loadResources() {
    try {
      const response = await fetch('/api/resources');
      const resources = await response.json();
  
      if (window.location.pathname.includes('/resourcemanage')) {
        displayResourcesManagement(resources);  
      } else {
        displayResourcesMain(resources);  
      }
    } catch (error) {
    }
  }

  function displayResourcesMain(resources) {
    const windowsSection = document.querySelector('.download-section-windows');
    const linuxSection = document.querySelector('.download-section-linux');

    if (!windowsSection || !linuxSection) {
        return;
    }

    windowsSection.innerHTML = '<div class="resource-label-div" style="display: flex; justify-content: center;"><label style="font-size: 35px;">Windows Builds<label></div>';
    linuxSection.innerHTML = '<div class="resource-label-div" style="display: flex; justify-content: center;"><label style="font-size: 35px;">Linux Builds</label></div>';

    const groupedResources = groupResourcesByPlayerCount(resources);

    for (const [playerCount, builds] of Object.entries(groupedResources)) {
        const windowsBuildHtml = createBuildTable(playerCount, builds.filter(b => b.platform === 'windows'), 'windows');
        windowsSection.innerHTML += windowsBuildHtml;

        const linuxBuildHtml = createBuildTable(playerCount, builds.filter(b => b.platform === 'linux'), 'linux');
        linuxSection.innerHTML += linuxBuildHtml;
    }
}

function groupResourcesByPlayerCount(resources) {
    const grouped = {
        '8 Player Build': [],
        '10 Player Build': [],
        '16 Player Build': []
    };

    resources.forEach(resource => {
        if (resource.name.includes('8 Players')) {
            grouped['8 Player Build'].push(resource);
        } else if (resource.name.includes('10 Players')) {
            grouped['10 Player Build'].push(resource);
        } else if (resource.name.includes('16 Players')) {
            grouped['16 Player Build'].push(resource);
        }
    });

    return grouped;
}

function createBuildTable(playerCount, builds, platform) {
    if (builds.length === 0) return '';

    let html = `
        <h3 class="card-title-download">${playerCount}</h3>
        <div class="tutorial-download-container">
            <table class="version-history-table">
                <thead>
                    <tr>
                        <th>Version</th>
                        <th>Release Date</th>
                        <th>Download</th>
                    </tr>
                </thead>
            <tbody>
    `;

    builds.forEach((build, index) => {
        const isLatest = index === 0;
        html += `
            <tr>
                <td class="td2">${build.version}</td>
                <td>${isLatest ? `Latest Version - ${new Date(build.date).toLocaleDateString()}` : new Date(build.date).toLocaleDateString()}</td>
                <td><a href="/api/download-resource/${encodeURIComponent(build.name)}" class="btn-download foo">Download</a></td>
            </tr>
        `;
    });

    html += `
                </tbody>
            </table>
        </div>
    `;

    return html;
}

function formatBytes(bytes, decimals = 2) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function groupDedconBuildsByPlayerCount(builds) {
  const grouped = {
      'DEDCON': [],
      'DEDCON 8P': [],
      'DEDCON 10P': []
  };

  builds.forEach(build => {
      if (build.player_count === '8') {
          grouped['DEDCON 8P'].push(build);
      } else if (build.player_count === '10') {
          grouped['DEDCON 10P'].push(build);
      } else {
          grouped['DEDCON'].push(build);
      }
  });

  return grouped;
}

async function displayDedconBuilds() {
  try {
      const response = await fetch('/api/dedcon-builds');
      const builds = await response.json();

      const buildContainer = document.querySelector('.dedcon-build-container');
      if (!buildContainer) {
          console.error('Build container not found');
          return;
      }

      // Clear existing content
      buildContainer.innerHTML = `
          <div class="tutorial-download-container">
              <table class="version-history-table">
                  <thead>
                      <tr>
                          <th>Platform</th>
                          <th>Version</th>
                          <th>Build Type</th>
                          <th>Release Date</th>
                          <th>Download</th>
                      </tr>
                  </thead>
                  <tbody>
                  </tbody>
              </table>
          </div>
      `;

      // Sort all builds by release date (newest first)
      const sortedBuilds = builds.sort((a, b) => new Date(b.release_date) - new Date(a.release_date));

      // Get table body
      const tableBody = buildContainer.querySelector('tbody');

      // Map platform names to more user-friendly display names
      const platformDisplayNames = {
          'windows': 'Windows',
          'linux': 'Linux',
          'macos-intel': 'MacOS Intel',
          'macos-arm64': 'MacOS ARM64'
      };

      // Map player counts to display names
      const playerCountDisplayNames = {
          '': 'Vanilla',
          '8': '8 Player',
          '10': '10 Player'
      };

      // Create rows for each build
      sortedBuilds.forEach((build, index) => {
          const platformName = platformDisplayNames[build.platform] || build.platform;
          const buildType = playerCountDisplayNames[build.player_count] || 'Vanilla';
          const isLatest = index === 0 || (build.platform !== sortedBuilds[index - 1].platform);

          const row = document.createElement('tr');
          row.className = isLatest ? 'latest-version' : '';
          
          row.innerHTML = `
              <td>
                  <div class="platform-cell">
                      <img src="/images/${build.platform}-icon.png" alt="${platformName}" class="steam-logo" style="width: 16px;" />
                      ${platformName}
                  </div>
              </td>
              <td class="td2">${build.version}</td>
              <td>${buildType}</td>
              <td>${isLatest ? 
                  `<span class="latest-tag"></span> ${new Date(build.release_date).toLocaleDateString()}` : 
                  new Date(build.release_date).toLocaleDateString()
              }</td>
              <td>
                  <a href="/api/download-dedcon-build/${encodeURIComponent(build.name)}" 
                     class="btn-download foo">
                     Download
                  </a>
              </td>
          `;

          tableBody.appendChild(row);
      });

  } catch (error) {
      console.error('Error loading dedcon builds:', error);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  const dedconContainer = document.querySelector('.dedcon-build-container');
  if (dedconContainer) {
      displayDedconBuilds();
  }
});

document.addEventListener('DOMContentLoaded', function() {
  const sections = document.querySelectorAll('.installation-section');
  
  sections.forEach(section => {
    const header = section.querySelector('.dropdown-header');
    const content = section.querySelector('.dropdown-content');
    const arrow = section.querySelector('.arrow');
    
    header.addEventListener('click', () => {
      arrow.classList.toggle('up');
      
      content.classList.toggle('show');
      
      sections.forEach(otherSection => {
        if (otherSection !== section) {
          otherSection.querySelector('.arrow').classList.remove('up');
          otherSection.querySelector('.dropdown-content').classList.remove('show');
        }
      });
    });
  });
});

async function loadModsByType(type, searchTerm = '', sortBy = 'latest') {
  try {
      let url = `/api/mods?type=${encodeURIComponent(type)}`;
      if (searchTerm) {
          url += `&search=${encodeURIComponent(searchTerm)}`;
      }
      url += `&sort=${encodeURIComponent(sortBy)}`;
      
      const response = await fetch(url);
      const mods = await response.json();
      displayMods(mods);
  } catch (error) {
  }
}

function displayMods(mods) {
    const modList = document.getElementById('mod-list');
    modList.innerHTML = '';

    const uniqueMods = Array.from(new Set(mods.map(mod => mod.id)))
        .map(id => mods.find(mod => mod.id === id));

    uniqueMods.forEach(mod => {
        const modItem = document.createElement('div');
        modItem.className = 'mod-item';
        modItem.dataset.modId = mod.id;

        const headerImagePath = mod.preview_image_path
            ? '/' + mod.preview_image_path.split('/').slice(-2).join('/')
            : '/modpreviews/icon3.png';

        modItem.innerHTML = `
            <div class="mod-header">
                <div class="mod-header-background" style="background-image: url('${headerImagePath}')"></div>
                <div class="mod-header-overlay"></div>
                <div class="mod-title-container">
                    <h3 class="mod-title">${mod.name}</h3>
                    <h3 class="mod-author">By (${mod.creator})</h3>
                    <span class="file-size"><i class="fas fa-file-archive"></i> ${formatBytes(mod.size)}</span>
                    <p class="mod-subtitle">${mod.description}</p>
                    <div class="mod-interaction">
                        <button class="like-button ${mod.isLiked ? 'liked' : ''}">
                            <i class="fas fa-thumbs-up"></i>
                            <span class="like-count">${mod.likes_count || 0}</span>
                        </button>
                        <button class="favorite-button ${mod.isFavorited ? 'favorited' : ''}">
                            <i class="fas fa-star"></i>
                            <span class="favorite-count">${mod.favorites_count || 0}</span>
                        </button>
                    </div>
                </div>
            </div>
            <div class="mod-info">
                <div class="mod-meta">
                    <span class="downloads"><i class="fas fa-download"></i> ${mod.download_count || 0}</span>
                    <span class="release-date"><i class="fas fa-calendar-alt"></i> ${new Date(mod.release_date).toLocaleDateString()}</span>
                    <span class="compatibility">${mod.compatibility || 'Unknown'}</span>
                    <button class="btn-report">Report</button>
                    <a href="/api/download-mod/${mod.id}" class="download-btn"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                </div>
            </div>
        `;

        modList.appendChild(modItem);

        const likeButton = modItem.querySelector('.like-button');
        const favoriteButton = modItem.querySelector('.favorite-button');

        likeButton.addEventListener('click', async function () {
          const response = await likeMod(mod.id);
          if (response.ok) {
              this.classList.toggle('liked');
              mod.isLiked = !mod.isLiked;
              const likeCount = this.querySelector('.like-count');
              let count = parseInt(likeCount.textContent);
              likeCount.textContent = mod.isLiked ? count + 1 : Math.max(0, count - 1);
          }
      });
      
      favoriteButton.addEventListener('click', async function () {
          const response = await favoriteMod(mod.id);
          if (response.ok) {
              this.classList.toggle('favorited');
              mod.isFavorited = !mod.isFavorited;
              const favoriteCount = this.querySelector('.favorite-count');
              let count = parseInt(favoriteCount.textContent);
              favoriteCount.textContent = mod.isFavorited ? count + 1 : Math.max(0, count - 1);
          }
      });

      modList.appendChild(modItem);

      const reportButton = modItem.querySelector('.btn-report');
      if (reportButton) {
          reportButton.addEventListener('click', function(event) {
              showModReportOptions(mod.id, event);
          });
      }
    });
}

async function getUserLikes() {
  try {
      const response = await fetch('/api/user/likes');
      const data = await response.json();
      return data.likes || [];  
  } catch (error) {
      return [];  
  }
}

async function getUserFavorites() {
  try {
      const response = await fetch('/api/user/favorites');
      const data = await response.json();
      return data.favorites || [];  
  } catch (error) {
      return [];  
  }
}

async function likeMod(modId) {
  if (!isUserLoggedIn()) {
      await alert('You need to be logged in to like mods.');
      return { ok: false };
  }
  try {
      const response = await fetch(`/api/mods/${modId}/like`, { 
          method: 'POST',
          headers: {
              'Authorization': `Bearer ${localStorage.getItem('token')}`
          }
      });
      if (!response.ok) {
          if (response.status === 401) {
              await alert('You need to be logged in to like mods.');
          } else {
              const errorData = await response.json();
              await alert(errorData.error || 'An error occurred while liking the mod.');
          }
          return { ok: false };
      }
      return { ok: true };
  } catch (error) {
      await alert('An error occurred while liking the mod.');
      return { ok: false };
  }
}

async function favoriteMod(modId) {
  if (!isUserLoggedIn()) {
      await alert('You need to be logged in to favorite mods.');
      return { ok: false };
  }
  try {
      const response = await fetch(`/api/mods/${modId}/favorite`, { 
          method: 'POST',
          headers: {
              'Authorization': `Bearer ${localStorage.getItem('token')}`
          }
      });
      if (!response.ok) {
          if (response.status === 401) {
              await alert('You need to be logged in to favorite mods.');
          } else {
              const errorData = await response.json();
              await alert(errorData.error || 'An error occurred while favoriting the mod.');
          }
          return { ok: false };
      }
      return { ok: true };
  } catch (error) {
      await alert('An error occurred while favoriting the mod.');
      return { ok: false };
  }
}
  

function showModReportOptions(modId, event) {
  event.preventDefault();
  event.stopPropagation();

  const options = [
    'Broken download',
    'Incorrect information',
    'Malicious content',
    'Hard install process',
    'Does not work as expected'
  ];

  const dropdown = document.createElement('div');
  dropdown.className = 'report-dropdown';
  
  options.forEach(option => {
    const button = document.createElement('button');
    button.textContent = option;
    button.onclick = () => confirmModReport(modId, option);
    dropdown.appendChild(button);
  });

  const existingDropdown = document.querySelector('.report-dropdown');
  if (existingDropdown) {
    existingDropdown.remove();
  }

  const reportButton = event.target;
  const modItem = reportButton.closest('.mod-info');
  modItem.style.position = 'relative';
  modItem.appendChild(dropdown);

  document.addEventListener('click', closeModDropdown);
}

function closeModDropdown(event) {
  if (!event.target.matches('.btn-report')) {
    const dropdowns = document.getElementsByClassName('report-dropdown');
    for (let i = 0; i < dropdowns.length; i++) {
      dropdowns[i].remove();
    }
    document.removeEventListener('click', closeModDropdown);
  }
}

async function confirmModReport(modId, reportType) {
  const confirmed = await confirm(`Are you sure you want to report this mod for ${reportType}?`);
  if (confirmed) {
    submitModReport(modId, reportType);
  }
}

async function submitModReport(modId, reportType) {
  try {
    const response = await fetch('/api/report-mod', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ modId, reportType }),
    });

    if (response.ok) {
      alert('Mod report successfully sent to the team!');
    } else {
      throw new Error('You need to be logged in to report mods.');
    }
  } catch (error) {
    alert('You need to be logged in to report mods.');
  }
}

const dialogHTML = `
<div id="custom-dialog" class="custom-dialog">
  <div class="dialog-content">
    <label id="dialog-message"></label>
    <div class="dialog-buttons">
      <button id="dialog-cancel" class="dialog-button">Cancel</button>
      <button id="dialog-confirm" class="dialog-button">OK</button>
    </div>
  </div>
</div>
`;

document.body.insertAdjacentHTML('beforeend', dialogHTML);

const styles = `
<style>
  .custom-dialog {
    display: none;
    position: fixed;
    z-index: 1000;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0,0,0,0.4);
  }
  .dialog-content {
    background-color: #101010;
    margin: 15% auto;
    padding: 20px;
    width: 350px;
    border-radius: 10px;
    text-align: center;
  }
  .dialog-message {
    color: #1a1a1a;
  }
  #dialog-cancel {
    background-color: #830000;
    color: #ffffff;
  }
  #dialog-confirm {
    background-color: #0067b8;
    color: #ffffff;
  }
  .dialog-button {
    font-size: 0.9rem;
    font-weight: bold;
    padding: 5px 20px;
    margin: 20px 10px 0px 10px;
    border-radius: 10px;
    cursor: pointer;
    border: none;
  }
</style>
`;

document.head.insertAdjacentHTML('beforeend', styles);

window.confirm = function(message) {
  return new Promise((resolve) => {
    const dialog = document.getElementById('custom-dialog');
    const messageElement = document.getElementById('dialog-message');
    const confirmButton = document.getElementById('dialog-confirm');
    const cancelButton = document.getElementById('dialog-cancel');

    messageElement.textContent = message;
    dialog.style.display = 'block';
    cancelButton.style.display = 'inline-block';

    const closeDialog = (result) => {
      dialog.style.display = 'none';
      resolve(result);
    };

    confirmButton.onclick = () => closeDialog(true);
    cancelButton.onclick = () => closeDialog(false);
  });
};

window.alert = function(message) {
  return new Promise((resolve) => {
    const dialog = document.getElementById('custom-dialog');
    const messageElement = document.getElementById('dialog-message');
    const confirmButton = document.getElementById('dialog-confirm');
    const cancelButton = document.getElementById('dialog-cancel');

    messageElement.textContent = message;
    dialog.style.display = 'block';
    cancelButton.style.display = 'none';

    const closeDialog = () => {
      dialog.style.display = 'none';
      resolve();
    };

    confirmButton.onclick = closeDialog;
  });
};
  
  document.addEventListener('DOMContentLoaded', function() {
    const searchInput = document.getElementById('mod-search');
    const searchButton = document.getElementById('search-button');
    const sortSelect = document.getElementById('sort-select');
    const modListContainer = document.getElementById('mod-list');
  
    if (modListContainer) {
      const modType = modListContainer.dataset.modType;
      
      if (searchButton) {
        searchButton.addEventListener('click', () => {
          const searchTerm = searchInput.value.trim();
          const sortBy = sortSelect ? sortSelect.value : '';
          loadModsByType(modType, searchTerm, sortBy);
        });
      }
  
      if (sortSelect) {
        sortSelect.addEventListener('change', () => {
          const searchTerm = searchInput ? searchInput.value.trim() : '';
          loadModsByType(modType, searchTerm, sortSelect.value);
        });
      }
  
      loadModsByType(modType);
    }
  });

  function setupModFilters(modType) {
    const searchInput = document.getElementById('mod-search');
    const searchButton = document.getElementById('search-button');
    const sortSelect = document.getElementById('sort-select');

    searchButton.addEventListener('click', () => {
        const searchTerm = searchInput.value.trim();
        loadModsByType(modType, searchTerm, sortSelect.value);
    });

    sortSelect.addEventListener('change', () => {
        loadModsByType(modType, searchInput.value.trim(), sortSelect.value);
    });

    sortSelect.value = 'latest';
    loadModsByType(modType, '', 'latest');
}
  
document.addEventListener('DOMContentLoaded', loadResources);



