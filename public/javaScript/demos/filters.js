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
//Last Edited 01-04-2025

import { displayDemos } from './democard.js';
import { updatePagination, updateURL } from '../main/main.js';
import { territoryMappingUI, filterState } from './constants.js';

let allDemos = [];
let currentPage = 1;
let totalPages = 1;
let currentSort = 'latest';
let currentServerFilter = '';

function initializeDemos() {
  const urlParams = new URLSearchParams(window.location.search);
  currentPage = parseInt(urlParams.get('page') || '1');
  currentSort = urlParams.get('sort') || 'latest';
  currentServerFilter = urlParams.get('server') || '';
 
  const serverFilter = document.getElementById('server-filter');
  if (serverFilter && currentServerFilter) {
    serverFilter.value = currentServerFilter;
  }

  const sortSelect = document.querySelector('.sort-container select');
  if (sortSelect && currentSort) {
    sortSelect.value = currentSort;
  }

  const playerSearchInput = document.getElementById('player-search-input');
  const playerName = urlParams.get('playerName');
  if (playerSearchInput && playerName) {
    playerSearchInput.value = playerName;
  }

  if (urlParams.get('territories')) {
    const savedTerritories = urlParams.get('territories').split(',');
    savedTerritories.forEach(territory => {
      const territoryId = Object.entries(territoryMappingUI)
        .find(([id, name]) => name === territory)?.[0];
      if (territoryId) {
        const checkbox = document.querySelector(`#${territoryId}`);
        if (checkbox) {
          checkbox.checked = true;
          filterState.territories.add(territoryId);
        }
      }
    });
  }

  if (urlParams.get('players')) {
    const savedPlayers = urlParams.get('players').split(',');
    const playerInputs = document.querySelectorAll('.player-input');
    savedPlayers.forEach((player, index) => {
      if (playerInputs[index]) {
        playerInputs[index].value = player;
        filterState.players[index] = player;
      }
    });
  }

  const combineMode = urlParams.get('combineMode') === 'true';
  const combineModeCheckbox = document.querySelector('.combine-territory-checkbox');
  if (combineModeCheckbox) {
    combineModeCheckbox.checked = combineMode;
    filterState.combineMode = combineMode;
  }

  const scoreFilter = urlParams.get('scoreFilter');
  if (scoreFilter) {
    const scoreFilterSelect = document.querySelector('.score-filter');
    if (scoreFilterSelect) {
      scoreFilterSelect.value = scoreFilter;
      filterState.scoreFilter = scoreFilter;
    }
  }

  const durationFilter = urlParams.get('gameDuration');
  if (durationFilter) {
    const durationSelect = document.querySelector('.duration-select');
    if (durationSelect) {
      durationSelect.value = durationFilter;
      filterState.durationFilter = durationFilter;
    }
  }

  const scoreDifferenceFilter = urlParams.get('scoreDifference');
  if (scoreDifferenceFilter) {
    const scoreDiffSelect = document.querySelector('.score-difference-select');
    if (scoreDiffSelect) {
      scoreDiffSelect.value = scoreDifferenceFilter;
      filterState.scoreDifferenceFilter = scoreDifferenceFilter;
    }
  }

  const dateInputs = document.querySelectorAll('.date-input');
  if (urlParams.get('startDate') && dateInputs[0]) {
    dateInputs[0].value = urlParams.get('startDate');
    filterState.dateRange.start = urlParams.get('startDate');
  }
  if (urlParams.get('endDate') && dateInputs[1]) {
    dateInputs[1].value = urlParams.get('endDate');
    filterState.dateRange.end = urlParams.get('endDate');
  }

  const gamesPlayed = urlParams.get('gamesPlayed');
  if (gamesPlayed) {
    const gamesPlayedSelect = document.querySelector('.games-played-filter');
    if (gamesPlayedSelect) {
      gamesPlayedSelect.value = gamesPlayed;
      filterState.gamesPlayed = gamesPlayed;
    }
  }
  
  const hasAdvancedFilters = [
    'territories', 'combineMode', 'players', 'scoreFilter',
    'gameDuration', 'scoreDifference', 'startDate', 'endDate',
    'gamesPlayed'
  ].some(param => urlParams.has(param));

  if (hasAdvancedFilters) {
    const advancedFilters = document.getElementById('advancedFilters');
    const toggleButton = document.getElementById('advancedFiltersToggle');
    if (advancedFilters && toggleButton) {
      advancedFilters.classList.add('show');
      toggleButton.classList.add('active');
    }
  }

  updateDemoList(playerName);
}

function updateDemoList(playerName = '') {
  const timestamp = new Date().getTime();
  const serverFilter = currentServerFilter;
  const queryParams = new URLSearchParams();

  queryParams.set('page', currentPage);
  queryParams.set('sortBy', currentSort);

  if (playerName) {
    queryParams.set('playerName', playerName);
  }
  if (serverFilter) {
    queryParams.set('serverName', serverFilter);
  }

  const validPlayers = filterState.players.filter(player => player && player.trim());
  if (validPlayers.length > 0) {
    queryParams.set('players', validPlayers.join(','));
  }

  if (filterState.territories.size > 0) {
    const territoryNames = Array.from(filterState.territories)
      .map(id => territoryMappingUI[id])
      .filter(Boolean);
    queryParams.set('territories', territoryNames.join(','));
    queryParams.set('combineMode', filterState.combineMode);
  }

  if (filterState.scoreFilter) queryParams.set('scoreFilter', filterState.scoreFilter);
  if (filterState.durationFilter) queryParams.set('gameDuration', filterState.durationFilter);
  if (filterState.scoreDifferenceFilter) queryParams.set('scoreDifference', filterState.scoreDifferenceFilter);
  if (filterState.dateRange.start) queryParams.set('startDate', filterState.dateRange.start);
  if (filterState.dateRange.end) queryParams.set('endDate', filterState.dateRange.end);
  if (filterState.gamesPlayed) queryParams.set('gamesPlayed', filterState.gamesPlayed);

  queryParams.set('t', timestamp);
  queryParams.set('includeNewPlayers', filterState.includeNewPlayers);

  fetch(`/api/demos?${queryParams.toString()}`)
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.json();
    })
    .then(data => {
      allDemos = data.demos;
      totalPages = data.totalPages;
      currentPage = data.currentPage;

      const gamesPlayed = document.getElementById('games-played');
      if (gamesPlayed) {
        gamesPlayed.textContent = `Total Games Played: ${data.totalDemos}`;
      }

      displayDemos(allDemos);
      updatePagination(currentPage, totalPages);
      updateURL(currentPage, currentSort, currentServerFilter, filterState);
    })
    .catch(error => {
      console.error('Failed to fetch demo data:', error);
      alert('Failed to fetch demo data. Please try again later.');
    });
}

function performPlayerSearch() {
  const playerSearchInput = document.getElementById('player-search-input');
  if (!playerSearchInput) return;

  const playerName = playerSearchInput.value.trim();
  const url = new URL(window.location);

  if (playerName) {
    url.searchParams.set('playerName', playerName);
  } else {
    url.searchParams.delete('playerName');
  }

  url.searchParams.set('page', '1');
  window.location.href = url.toString();
}

function resetFilters() {
  const url = new URL(window.location);
  const hasAppliedFilters = [
    'territories', 'players', 'combineMode', 'scoreFilter',
    'gameDuration', 'scoreDifference', 'startDate', 'endDate',
    'gamesPlayed', 'playerName', 'includeNewPlayers'
  ].some(param => url.searchParams.has(param));

  document.querySelectorAll('.territory-checkbox').forEach(checkbox => {
    checkbox.checked = false;
    filterState.territories.clear();
  });

  const combineCheckbox = document.querySelector('.combine-territory-checkbox');
  if (combineCheckbox) {
    combineCheckbox.checked = false;
    filterState.combineMode = false;
  }

  const scoreFilter = document.querySelector('.score-filter');
  if (scoreFilter) {
    scoreFilter.value = '';
    filterState.scoreFilter = '';
  }

  const durationSelect = document.querySelector('.duration-select');
  if (durationSelect) {
    durationSelect.value = '';
    filterState.durationFilter = '';
  }

  const scoreDiffSelect = document.querySelector('.score-difference-select');
  if (scoreDiffSelect) {
    scoreDiffSelect.value = '';
    filterState.scoreDifferenceFilter = '';
  }

  document.querySelectorAll('.date-input').forEach((input, index) => {
    input.value = '';
    if (index === 0) {
      filterState.dateRange.start = null;
    } else {
      filterState.dateRange.end = null;
    }
  });

  document.querySelectorAll('.player-input').forEach((input, index) => {
    input.value = '';
    filterState.players[index] = '';
  });

  const gamesPlayedSelect = document.querySelector('.games-played-filter');
  if (gamesPlayedSelect) {
    gamesPlayedSelect.value = '';
    filterState.gamesPlayed = '';
  }

  const playerSearchInput = document.getElementById('player-search-input');
  if (playerSearchInput) {
    playerSearchInput.value = '';
  }

  const newPlayerCheckbox = document.querySelector('.newplayer-checkbox');
  if (newPlayerCheckbox instanceof HTMLInputElement) {
    newPlayerCheckbox.checked = true;
    filterState.includeNewPlayers = true;
  }

  if (hasAppliedFilters) {
    const newUrl = new URL(window.location);
    for (const param of newUrl.searchParams.keys()) {
      if (param !== 'sort' && param !== 'page' && param !== 'server') {
        newUrl.searchParams.delete(param);
      }
    }
    window.location.href = newUrl.toString();
  }
}

function setupFilterEventListeners() {
  
  const resetFiltersBtn = document.querySelector('.reset-filters-btn');
  if (resetFiltersBtn) {
    resetFiltersBtn.addEventListener('click', resetFilters);
  }

  const searchButton = document.getElementById('search-button2');
  if (searchButton) {
    searchButton.addEventListener('click', performPlayerSearch);
  }

  const playerSearchInput = document.getElementById('player-search-input');
  if (playerSearchInput) {
    playerSearchInput.addEventListener('keypress', (e) => {
      if (e.key === 'Enter') {
        performPlayerSearch();
      }
    });
  }

  const newPlayerSwitch = document.getElementById('newplayer');
  if (newPlayerSwitch) {
    const urlParams = new URLSearchParams(window.location.search);
    const includeNewPlayers = urlParams.get('includeNewPlayers') !== 'false';
    newPlayerSwitch.checked = includeNewPlayers;
    filterState.includeNewPlayers = includeNewPlayers;

    newPlayerSwitch.addEventListener('change', (e) => {
      filterState.includeNewPlayers = e.target.checked;
    });
  }

  const sortSelect = document.querySelector('.sort-container select');
  if (sortSelect) {
    sortSelect.addEventListener('change', (e) => {
      const url = new URL(window.location);
      url.searchParams.set('sort', e.target.value);
      url.searchParams.set('page', '1');
      window.location.href = url.toString();
    });
  }

  const serverFilter = document.getElementById('server-filter');
  if (serverFilter) {
    serverFilter.addEventListener('change', (e) => {
      const url = new URL(window.location);
      if (e.target.value) {
        url.searchParams.set('server', e.target.value);
      } else {
        url.searchParams.delete('server');
      }
      url.searchParams.set('page', '1');
      window.location.href = url.toString();
    });
  }

  document.querySelectorAll('.territory-checkbox').forEach(checkbox => {
    checkbox.addEventListener('change', (e) => {
      if (e.target.checked) {
        filterState.territories.add(e.target.id);
      } else {
        filterState.territories.delete(e.target.id);
      }
    });
  });

  document.querySelector('.combine-territory-checkbox')?.addEventListener('change', (e) => {
    filterState.combineMode = e.target.checked;
  });

  document.querySelector('.score-filter')?.addEventListener('change', (e) => {
    filterState.scoreFilter = e.target.value;
    if (e.target.value) {
      document.querySelector('.score-difference-select').value = '';
      document.querySelector('.duration-select').value = '';
      filterState.scoreDifferenceFilter = '';
      filterState.durationFilter = '';
    }
  });

  document.querySelector('.duration-select')?.addEventListener('change', (e) => {
    filterState.durationFilter = e.target.value;
    if (e.target.value) {
      document.querySelector('.score-filter').value = '';
      document.querySelector('.score-difference-select').value = '';
      filterState.scoreFilter = '';
      filterState.scoreDifferenceFilter = '';
    }
  });

  document.querySelector('.score-difference-select')?.addEventListener('change', (e) => {
    filterState.scoreDifferenceFilter = e.target.value;
    if (e.target.value) {
      document.querySelector('.score-filter').value = '';
      document.querySelector('.duration-select').value = '';
      filterState.scoreFilter = '';
      filterState.durationFilter = '';
    }
  });

  document.querySelectorAll('.player-input').forEach((input, index) => {
    input.addEventListener('input', (e) => {
      filterState.players[index] = e.target.value.trim();
    });
  });

  document.querySelectorAll('.date-input').forEach((input, index) => {
    input.addEventListener('change', (e) => {
      if (index === 0) {
        filterState.dateRange.start = e.target.value;
      } else {
        filterState.dateRange.end = e.target.value;
      }
    });
  });

  document.querySelector('.games-played-filter')?.addEventListener('change', (e) => {
    filterState.gamesPlayed = e.target.value;
  });

  document.querySelector('.apply-filters-btn')?.addEventListener('click', () => {
    const url = new URL(window.location);

    url.searchParams.set('page', '1');

    const validPlayers = filterState.players.filter(player => player && player.trim());
    if (validPlayers.length > 0) {
      url.searchParams.set('players', validPlayers.join(','));
    } else {
      url.searchParams.delete('players');
    }

    if (!filterState.includeNewPlayers) {
      url.searchParams.set('includeNewPlayers', 'false');
    } else {
      url.searchParams.delete('includeNewPlayers');
    }

    if (filterState.territories.size > 0) {
      const territoryNames = Array.from(filterState.territories)
        .map(id => territoryMappingUI[id])
        .filter(Boolean);
      url.searchParams.set('territories', territoryNames.join(','));
      url.searchParams.set('combineMode', filterState.combineMode);
    } else {
      url.searchParams.delete('territories');
      url.searchParams.delete('combineMode');
    }

    if (filterState.scoreFilter) {
      url.searchParams.set('scoreFilter', filterState.scoreFilter);
    } else {
      url.searchParams.delete('scoreFilter');
    }

    if (filterState.durationFilter) {
      url.searchParams.set('gameDuration', filterState.durationFilter);
    } else {
      url.searchParams.delete('gameDuration');
    }

    if (filterState.scoreDifferenceFilter) {
      url.searchParams.set('scoreDifference', filterState.scoreDifferenceFilter);
    } else {
      url.searchParams.delete('scoreDifference');
    }

    if (filterState.dateRange.start) {
      url.searchParams.set('startDate', filterState.dateRange.start);
    } else {
      url.searchParams.delete('startDate');
    }

    if (filterState.dateRange.end) {
      url.searchParams.set('endDate', filterState.dateRange.end);
    } else {
      url.searchParams.delete('endDate');
    }

    if (filterState.gamesPlayed) {
      url.searchParams.set('gamesPlayed', filterState.gamesPlayed);
    } else {
      url.searchParams.delete('gamesPlayed');
    }

    window.location.href = url.toString();
  });
}

document.addEventListener('DOMContentLoaded', () => {
  initializeDemos();
  setupFilterEventListeners();
});

window.addEventListener('popstate', () => {
  window.location.reload();
});

export {
  initializeDemos,
  updateDemoList,
  performPlayerSearch,
  resetFilters,
  allDemos,
  currentPage,
  totalPages,
  currentSort,
  currentServerFilter
};