// Main application file - connects all modules and provides utility functions
import { initializeDiscordWidget } from './discord.js';
import { initializeReportHandlers } from './reporting.js';
import { initializeAuthentication } from './authentication.js';
import { initializeNavigation } from './mobilemenu.js';
import { initializeMods } from './mods.js';
import { initializePopupSystem } from './popup.js';

// Global variables
let soundVolume = 0.1;

// Utility Functions
// Format bytes to human-readable form
function formatBytes(bytes, decimals = 2) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

// Perform game search
function performGameSearch() {
  const searchInput = document.getElementById('search-input2');
  if (searchInput) {
    const searchTerm = searchInput.value.trim();
    if (searchTerm) {
      window.location.href = `/search?term=${encodeURIComponent(searchTerm)}`;
    }
  }
}

// Load resources from server
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

// Display resources in main view
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

// Display DEDCON builds
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

// Main initialization function
function initializeApplication() {
  // Set up core functionality
  initializePopupSystem();
  initializeAuthentication();
  initializeNavigation();
  initializeDiscordWidget();
  initializeReportHandlers();
  initializeMods();
  
  // Set up search functionality
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
  
  // Load resources if on the resources page
  const resourcesContainer = document.querySelector('.resources-container');
  if (resourcesContainer) {
    loadResources();
  }
  
  // Load DEDCON builds if on the DEDCON page
  const dedconContainer = document.querySelector('.dedcon-build-container');
  if (dedconContainer) {
    displayDedconBuilds();
  }
  
  // Check for demos initialization function
  if (typeof window.initializeDemos === 'function') {
    window.initializeDemos();
  }
  
  // Handle window resize for mobile menu
  window.addEventListener('resize', function () {
    if (window.innerWidth <= 768) {
      initializeNavigation();
    }
  });
}

// Initialize on DOM content loaded
document.addEventListener('DOMContentLoaded', initializeApplication);

// Export utility functions and initialization
export {
  formatBytes,
  performGameSearch,
  loadResources,
  displayResourcesMain,
  displayDedconBuilds,
  initializeApplication
};

// Make some functions available globally
window.formatBytes = formatBytes;
window.performGameSearch = performGameSearch;
window.soundVolume = soundVolume;