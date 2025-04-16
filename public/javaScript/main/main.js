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
//Last Edited 20-04-2025

import { initializeDiscordWidget } from './discord.js';
import { initializeReportHandlers } from './reporting.js';
import { initializeAuthentication } from './authentication.js';
import { initializeNavigation } from './mobilemenu.js';
import { initializeMods } from './mods.js';
import { initializePopupSystem } from './popup.js';
import { initializeLeaderboard } from '../leaderboard/leaderboard.js';
import { initializeGraphs } from '../graph/graph.js';

let soundVolume = 0.1;

function formatBytes(bytes, decimals = 2) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
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

function formatDuration(duration) {
  if (!duration) return 'Unknown';
  const [hours, minutes] = duration.split(':').map(Number);
  if (hours === 0) {
    return `${minutes} min`;
  } else {
    return `${hours} hr ${minutes} min`;
  }
}


function formatDurationProfile(duration) {
  if (!duration) return 'Unknown';

  if (typeof duration === 'string') {
    const [hours, minutes] = duration.split(':').map(Number);
    if (hours === 0) {
      return `${minutes} min`;
    } else {
      return `${hours} hr ${minutes} min`;
    }
  } else if (typeof duration === 'number') {
    if (duration < 60) {
      return `${Math.round(duration)} min`;
    } else {
      const hours = Math.floor(duration / 60);
      const remainingMinutes = Math.round(duration % 60);
      return `${hours} hr ${remainingMinutes} min`;
    }
  }

  return 'Unknown';
}

function updatePagination(currentPage, totalPages) {
  const paginationContainer = document.getElementById('pagination');
  if (!paginationContainer) return;
  paginationContainer.innerHTML = '';

  if (currentPage > 1) {
    const prevButton = createPaginationButton('Prev', currentPage - 1);
    paginationContainer.appendChild(prevButton);
  }

  let startPage = Math.max(1, currentPage - 4);
  let endPage = Math.min(totalPages, startPage + 8);

  if (totalPages > 9 && endPage - startPage < 8) {
    startPage = Math.max(1, endPage - 8);
  }

  if (startPage > 1) {
    const firstButton = createPaginationButton('1', 1);
    paginationContainer.appendChild(firstButton);
    if (startPage > 2) {
      const ellipsis = document.createElement('span');
      ellipsis.textContent = '...';
      ellipsis.className = 'pagination-ellipsis';
      paginationContainer.appendChild(ellipsis);
    }
  }

  for (let i = startPage; i <= endPage; i++) {
    const pageButton = createPaginationButton(i.toString(), i);
    if (i === currentPage) {
      pageButton.classList.add('active');
      pageButton.disabled = true;
    }
    paginationContainer.appendChild(pageButton);
  }

  if (endPage < totalPages) {
    if (endPage < totalPages - 1) {
      const ellipsis = document.createElement('span');
      ellipsis.textContent = '...';
      ellipsis.className = 'pagination-ellipsis';
      paginationContainer.appendChild(ellipsis);
    }
    const lastButton = createPaginationButton(totalPages.toString(), totalPages);
    paginationContainer.appendChild(lastButton);
  }

  if (currentPage < totalPages) {
    const nextButton = createPaginationButton('Next', currentPage + 1);
    paginationContainer.appendChild(nextButton);
  }

  if (totalPages > 9) {
    const customPageContainer = document.createElement('div');
    customPageContainer.className = 'custom-page-container';

    const customPageInput = document.createElement('input');
    customPageInput.type = 'number';
    customPageInput.min = 1;
    customPageInput.max = totalPages;
    customPageInput.placeholder = 'Go to page...';
    customPageInput.className = 'custom-page-input';

    const goButton = document.createElement('button');
    goButton.textContent = 'Go';
    goButton.className = 'custom-page-go';
    goButton.onclick = () => {
      const pageNum = parseInt(customPageInput.value);
      if (pageNum >= 1 && pageNum <= totalPages) {
        const url = new URL(window.location);
        url.searchParams.set('page', pageNum);
        window.location.href = url.toString();
      } else {
        alert(`Please enter a page number between 1 and ${totalPages}`);
      }
    };

    customPageInput.addEventListener('keypress', (e) => {
      if (e.key === 'Enter') {
        goButton.click();
      }
    });

    customPageContainer.appendChild(customPageInput);
    customPageContainer.appendChild(goButton);
    paginationContainer.appendChild(customPageContainer);
  }
}

function createPaginationButton(text, page) {
  const button = document.createElement('button');
  button.textContent = text;
  button.addEventListener('click', () => {
    const url = new URL(window.location);
    url.searchParams.set('page', page);
    window.location.href = url.toString();
  });
  return button;
}

function changePage(newPage, totalPages) {
  if (newPage >= 1 && newPage <= totalPages) {
    const url = new URL(window.location);
    url.searchParams.set('page', newPage);
    window.location.href = url.toString();
  }
}

function updateURL(currentPage, currentSort, currentServerFilter, filterState) {
  const url = new URL(window.location);
  url.searchParams.set('page', currentPage);
  url.searchParams.set('sort', currentSort);

  if (currentServerFilter) {
    url.searchParams.set('server', currentServerFilter);
  } else {
    url.searchParams.delete('server');
  }

  if (filterState && filterState.territories && filterState.territories.size > 0) {
    const territoryNames = Array.from(filterState.territories)
      .map(id => filterState.territoryMapping ? filterState.territoryMapping[id] : null)
      .filter(Boolean);
    url.searchParams.set('territories', territoryNames.join(','));
    url.searchParams.set('combineMode', filterState.combineMode);
  }

  if (filterState) {
    if (filterState.scoreFilter) url.searchParams.set('scoreFilter', filterState.scoreFilter);
    if (filterState.durationFilter) url.searchParams.set('gameDuration', filterState.durationFilter);
    if (filterState.scoreDifferenceFilter) url.searchParams.set('scoreDifference', filterState.scoreDifferenceFilter);
    if (filterState.dateRange && filterState.dateRange.start) url.searchParams.set('startDate', filterState.dateRange.start);
    if (filterState.dateRange && filterState.dateRange.end) url.searchParams.set('endDate', filterState.dateRange.end);
    if (filterState.gamesPlayed) url.searchParams.set('gamesPlayed', filterState.gamesPlayed);
  }

  window.history.pushState({}, '', url);
}

function performGameSearch() {
  const searchInput = document.getElementById('search-input2');
  if (searchInput) {
    const searchTerm = searchInput.value.trim();
    if (searchTerm) {
      window.location.href = `/search?term=${encodeURIComponent(searchTerm)}`;
    }
  }
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
    console.error('Error loading resources:', error);
  }
}

function displayResourcesMain(resources) {
  const buildContainer = document.querySelector('.resources-container');
  if (!buildContainer) return;

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

  const tableBody = buildContainer.querySelector('tbody');
  const platformDisplayNames = {
    'windows': 'Windows',
    'linux': 'Linux',
    'macos-intel': 'MacOS Intel',
    'macos-arm64': 'MacOS ARM64',
    'macos': 'MacOS'
  };

  const sortedResources = resources.sort((a, b) => new Date(b.date) - new Date(a.date));

  const completeResources = sortedResources.filter(resource =>
    resource.platform &&
    resource.platform !== 'NULL' &&
    resource.player_count &&
    resource.player_count !== 'NULL'
  );

  completeResources.forEach((resource, index) => {
    const platformName = platformDisplayNames[resource.platform] || 'Unknown';

    const isLatest = index === 0 ||
      (resource.platform !== completeResources[index - 1].platform) ||
      (resource.player_count !== completeResources[index - 1].player_count);

    const row = document.createElement('tr');
    row.className = isLatest ? 'latest-version' : '';

    row.innerHTML = `
      <td>
        <div class="platform-cell">
          <img src="/images/${resource.platform}-icon.png" alt="${platformName}" class="steam-logo" style="width: 16px;" />
          ${platformName}
        </div>
      </td>
      <td class="td2">${resource.version}</td>
      <td>${resource.player_count} Player</td>
      <td>${isLatest ?
        `<span class="latest-tag"></span> ${new Date(resource.date).toLocaleDateString()}` :
        new Date(resource.date).toLocaleDateString()
      }</td>
      <td>
        <a href="/api/download-resource/${encodeURIComponent(resource.name)}" 
           class="btn-download foo">
           Download
        </a>
      </td>
    `;

    tableBody.appendChild(row);
  });
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

    const sortedBuilds = builds.sort((a, b) => new Date(b.release_date) - new Date(a.release_date));
    const tableBody = buildContainer.querySelector('tbody');
    const platformDisplayNames = {
      'windows': 'Windows',
      'linux': 'Linux',
      'macos-intel': 'MacOS Intel',
      'macos-arm64': 'MacOS ARM64'
    };

    const playerCountDisplayNames = {
      '': 'Vanilla',
      '8': '8 Player',
      '10': '10 Player'
    };

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

/**
* Initialize the application with all required components
*/
function initializeApplication() {
  // Initialize core systems
  initializePopupSystem();
  initializeAuthentication();
  initializeNavigation();
  initializeDiscordWidget();
  initializeReportHandlers();
  initializeMods();

  // Initialize graphs if we're on a graph page
  const isGraphPage = window.location.pathname.startsWith('/about');
  if (isGraphPage) {
    initializeGraphs();
  }

  // Initialize leaderboard if on leaderboard page
  if (window.location.pathname.includes('/leaderboard')) {
    initializeLeaderboard();
  }

  // Initialize search functionality
  const searchButton2 = document.getElementById('search-button2');
  const searchInput2 = document.getElementById('search-input2');

  if (searchButton2 && searchInput2) {
    searchButton2.addEventListener('click', performGameSearch);
    searchInput2.addEventListener('keypress', function (e) {
      if (e.key === 'Enter') {
        performGameSearch();
      }
    });
  }

  // Initialize resources if on resources page
  const resourcesContainer = document.querySelector('.resources-container');
  if (resourcesContainer) {
    loadResources();
  }

  // Initialize dedcon builds if on dedcon builds page
  const dedconContainer = document.querySelector('.dedcon-build-container');
  if (dedconContainer) {
    displayDedconBuilds();
  }

  // Handle window resizing
  window.addEventListener('resize', function () {
    if (window.innerWidth <= 768) {
      initializeNavigation();
    }
  });
}

// Initialize the application when the DOM is loaded
document.addEventListener('DOMContentLoaded', initializeApplication);

// Export utilities and functions for other modules
export {
  formatBytes,
  getTimeAgo,
  formatDuration,
  formatDurationProfile,
  updatePagination,
  createPaginationButton,
  changePage,
  updateURL,
  performGameSearch,
  loadResources,
  displayResourcesMain,
  displayDedconBuilds,
  initializeApplication
};

// Make core functions available globally
window.formatBytes = formatBytes;
window.getTimeAgo = getTimeAgo;
window.formatDuration = formatDuration;
window.formatDurationProfile = formatDurationProfile;
window.performGameSearch = performGameSearch;
window.soundVolume = soundVolume;