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
// Last Edited 19-09-2025

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

export async function applyUrlParameters() {
    const urlParams = new URLSearchParams(window.location.search);

    for (const [key, value] of urlParams.entries()) {
        if (key in leaderboardFilters) {
            leaderboardFilters[key] = value;
        }
    }

    const hasCustomDates = urlParams.has('startDate') || urlParams.has('endDate');
    
    if (urlParams.has('season')) {
        const seasonValue = urlParams.get('season');
        
        const seasonsModule = await import('./seasons.js');
        seasonsModule.applySeasonFilter(seasonValue);

        const seasonSelect = document.getElementById('season-select');
        if (seasonSelect) {
            seasonSelect.value = seasonValue;
        }
    } else if (hasCustomDates) {
        const seasonsModule = await import('./seasons.js');
        seasonsModule.applySeasonFilter('custom');
        seasonsModule.toggleCustomDateInputs(true);

        const seasonSelect = document.getElementById('season-select');
        if (seasonSelect) {
            seasonSelect.value = 'custom';
        }
    }

    if (urlParams.has('playlist')) {
        const playlistValue = urlParams.get('playlist');

        const playlistFilter = document.getElementById('playlist-filter');
        if (playlistFilter && serverPlaylists[playlistValue]) {
            playlistFilter.value = playlistValue;
        }
    }
    
    initializeFilterElements();
    updateTournamentButtonVisibility();
    updateTournamentButtonState();
    
    if (isTournamentMode()) {
        leaderboardFilters.minGames = '1';
        const minGamesFilter = document.getElementById('min-games-filter');
        if (minGamesFilter) {
            minGamesFilter.value = '1';
        }
    }
}

export function getChristmasTournamentDates() {
    const today = new Date();
    const currentYear = today.getFullYear();
    const currentMonth = today.getMonth();
    const currentDay = today.getDate();
    
    let startYear = currentYear;
    let endYear = currentYear + 1;
    
    if (currentMonth < 10 || (currentMonth === 10 && currentDay < 28)) {
        startYear = currentYear - 1;
        endYear = currentYear;
    }
    
    const startDate = new Date(startYear, 10, 28);
    const endDate = new Date(endYear, 0, 12);
    
    return {
        startDate: startDate.toISOString().split('T')[0],
        endDate: endDate.toISOString().split('T')[0]
    };
}

export function isInTournamentPeriod() {
    const today = new Date();
    const currentMonth = today.getMonth();
    const currentDay = today.getDate();
    
    if (currentMonth === 10 && currentDay >= 28) {
        return true;
    }
    
    if (currentMonth === 11) {
        return true;
    }
    
    if (currentMonth === 0 && currentDay <= 12) {
        return true;
    }
    
    return false;
}

export function isTournamentMode() {
    return leaderboardFilters.serverName === 'Christmas Tournament 2025' &&
           leaderboardFilters.startDate && leaderboardFilters.endDate &&
           leaderboardFilters.startDate.includes('-11-28') &&
           leaderboardFilters.endDate.includes('-01-12');
}

export function updateTournamentButtonVisibility() {
    const tournamentToggle = document.getElementById('tournament-toggle');
    if (!tournamentToggle) return;
    
    if (isInTournamentPeriod()) {
        tournamentToggle.style.display = 'flex';
    } else {
        tournamentToggle.style.display = 'none';
    }
}

export function updateTournamentButtonState() {
    const tournamentToggle = document.getElementById('tournament-toggle');
    if (!tournamentToggle) return;
    
    if (isTournamentMode()) {
        tournamentToggle.classList.add('active');
        tournamentToggle.innerHTML = '<i class="fas fa-trophy"></i> Leaderboard';
    } else {
        tournamentToggle.classList.remove('active');
        tournamentToggle.innerHTML = '<i class="fas fa-trophy"></i> Tournament';
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
        if (leaderboardFilters.startDate || leaderboardFilters.endDate) {
            url.searchParams.set('season', 'custom');
        } else {
            url.searchParams.set('season', seasonSelect.value);
        }
    }

    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter && playlistFilter.value !== 'all') {
        url.searchParams.set('playlist', playlistFilter.value);
    } else {
        url.searchParams.delete('playlist');
    }

    window.history.pushState({}, '', url);
}

export function resetFilters() {
    const currentSeason = getCurrentSeason();

    leaderboardFilters.serverName = '';
    leaderboardFilters.sortBy = 'winRatio';
    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;
    leaderboardFilters.minGames = '3';
    delete leaderboardFilters.playlist;

    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) serverFilter.value = '';

    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) playlistFilter.value = 'all';

    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) sortSelect.value = 'winRatio';

    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) minGamesFilter.value = '3';

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