// DefconExpanded, Created by...
// KezzaMcFezza - Main Developer
// Nexustini - Server Management
//
// Notable Mentions...
// Rad - For helping with python scripts.
// Bert_the_turtle - Doing everything with c++
//
// Inspired by Sievert and Wan May
// 
// Last Edited 14-04-2025

import { leaderboardFilters, serverPlaylists } from './constants.js';
import { getCurrentSeason } from './seasons.js';
import { fetchLeaderboard } from './api.js';

export function initializeFilterElements() {
    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) {
        serverFilter.value = leaderboardFilters.serverName;
    }

    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) {
        sortSelect.value = leaderboardFilters.sortBy;
    }

    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) {
        minGamesFilter.value = leaderboardFilters.minGames;
    }

    const startDateInput = document.getElementById('start-date');
    if (startDateInput) {
        startDateInput.value = leaderboardFilters.startDate;
    }

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) {
        endDateInput.value = leaderboardFilters.endDate;
    }
}

export function initializePlaylistDropdown() {
    const playlistFilter = document.getElementById('playlist-filter');
    if (!playlistFilter) return;

    playlistFilter.innerHTML = '';

    Object.entries(serverPlaylists).forEach(([key, playlist]) => {
        const option = document.createElement('option');
        option.value = key;
        option.textContent = playlist.name;
        playlistFilter.appendChild(option);
    });
}

export function applyUrlParameters() {
    const urlParams = new URLSearchParams(window.location.search);

    for (const [key, value] of urlParams.entries()) {
        if (key in leaderboardFilters) {
            leaderboardFilters[key] = value;
        }
    }

    if (urlParams.has('season')) {
        const seasonValue = urlParams.get('season');
        
        import('./seasons.js').then(module => {
            module.applySeasonFilter(seasonValue);
        });

        const seasonSelect = document.getElementById('season-select');
        if (seasonSelect) {
            seasonSelect.value = seasonValue;
        }
    }

    if (urlParams.has('playlist')) {
        const playlistValue = urlParams.get('playlist');

        const playlistFilter = document.getElementById('playlist-filter');
        if (playlistFilter && serverPlaylists[playlistValue]) {
            playlistFilter.value = playlistValue;
        }
    }
}

export function updateURL() {
    const url = new URL(window.location);

    Object.entries(leaderboardFilters).forEach(([key, value]) => {
        if (value) {
            url.searchParams.set(key, value);
        } else {
            url.searchParams.delete(key);
        }
    });

    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) {
        url.searchParams.set('season', seasonSelect.value);
    }

    window.history.pushState({}, '', url);
}

export function resetFilters() {
    const currentSeason = getCurrentSeason();

    leaderboardFilters.serverName = '';
    leaderboardFilters.sortBy = 'wins';
    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;
    leaderboardFilters.minGames = '1';
    delete leaderboardFilters.playlist;

    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) serverFilter.value = '';

    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) playlistFilter.value = 'all';

    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) sortSelect.value = 'wins';

    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) minGamesFilter.value = '1';

    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) seasonSelect.value = 'current';

    const startDateInput = document.getElementById('start-date');
    if (startDateInput) startDateInput.value = currentSeason.startDate;

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) endDateInput.value = currentSeason.endDate;

    import('./seasons.js').then(module => {
        module.toggleCustomDateInputs(false);
    });

    updateURL();
    fetchLeaderboard();
}