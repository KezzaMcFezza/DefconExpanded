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
import { setupCurrentSeason, checkSeasonEndingSoon, applySeasonFilter, toggleCustomDateInputs, getCurrentSeason } from './seasons.js';
import { initializeFilterElements, initializePlaylistDropdown, applyUrlParameters, updateURL, updateTournamentButtonState, updateTournamentButtonVisibility, isTournamentMode, getChristmasTournamentDates } from './filters.js';
import { fetchLeaderboard, fetchAllTimeMostActivePlayers, fetchPreviousSeasonWinners } from './api.js';

export async function initializeLeaderboard() {
    const urlParams = new URLSearchParams(window.location.search);
    const hasCustomDates = urlParams.has('startDate') || urlParams.has('endDate');
    
    const { updateSeasonSelector } = await import('./seasons.js');
    if (!hasCustomDates) {
        setupCurrentSeason();
    } else {
        updateSeasonSelector('custom');
    }
    
    initializeFilterElements();
    initializePlaylistDropdown();
    await applyUrlParameters();
    setupEventListeners();
    checkSeasonEndingSoon();
    updateTournamentButtonVisibility();
    updateTournamentButtonState();
    
    fetchLeaderboard();
    fetchAllTimeMostActivePlayers();
    fetchPreviousSeasonWinners();
}

function setupEventListeners() {
    const filtersToggle = document.getElementById('filters-toggle');
    if (filtersToggle) {
        filtersToggle.addEventListener('click', () => {
            const filtersSection = document.getElementById('leaderboard-filters');
            if (filtersSection) {
                filtersSection.classList.toggle('expanded');
                filtersToggle.classList.toggle('active');
            }

            const advancedFilters = document.getElementById('advanced-filters');
            if (advancedFilters) {
                advancedFilters.classList.toggle('expanded');
                filtersToggle.classList.toggle('active');
            }
        });
    }

    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) {
        serverFilter.addEventListener('change', () => {
            leaderboardFilters.serverName = serverFilter.value;

            const playlistFilter = document.getElementById('playlist-filter');
            if (playlistFilter) {
                playlistFilter.value = 'all';
            }
            delete leaderboardFilters.playlist;
        });
    }

    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) {
        playlistFilter.addEventListener('change', () => {
            const selectedPlaylist = playlistFilter.value;

            if (selectedPlaylist === 'all') {
                leaderboardFilters.serverName = '';
                delete leaderboardFilters.playlist;
            } else {
                leaderboardFilters.serverName = '';
                leaderboardFilters.playlist = selectedPlaylist;

                if (serverFilter) {
                    serverFilter.value = '';
                }
            }
        });
    }

    const sortSelect = document.getElementById('sort-select');
    if (sortSelect) {
        sortSelect.addEventListener('change', () => {
            leaderboardFilters.sortBy = sortSelect.value;
            updateURL();
            fetchLeaderboard();
        });
    }

    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) {
        minGamesFilter.addEventListener('change', () => {
            leaderboardFilters.minGames = minGamesFilter.value;
        });
    }

    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) {
        seasonSelect.addEventListener('change', () => {
            const selectedSeason = seasonSelect.value;
            applySeasonFilter(selectedSeason);

            if (selectedSeason !== 'custom') {
                updateURL();
                fetchLeaderboard();
            }
        });
    }

    const startDateInput = document.getElementById('start-date');
    if (startDateInput) {
        startDateInput.addEventListener('change', () => {
            leaderboardFilters.startDate = startDateInput.value;
        });
    }

    const endDateInput = document.getElementById('end-date');
    if (endDateInput) {
        endDateInput.addEventListener('change', () => {
            leaderboardFilters.endDate = endDateInput.value;
        });
    }

    const applyFiltersBtn = document.getElementById('apply-filters');
    if (applyFiltersBtn) {
        applyFiltersBtn.addEventListener('click', () => {
            if (leaderboardFilters.startDate || leaderboardFilters.endDate) {
                const seasonSelect = document.getElementById('season-select');
                if (seasonSelect) {
                    import('./seasons.js').then(module => {
                        module.updateSeasonSelector('custom');
                        module.toggleCustomDateInputs(true);
                    });
                }
            }
            
            updateURL();
            
            window.location.reload();
        });
    }

    const resetFiltersBtn = document.getElementById('reset-filters');
    if (resetFiltersBtn) {
        resetFiltersBtn.addEventListener('click', () => {
            window.location.href = '/leaderboard';
        });
    }

    const tournamentToggle = document.getElementById('tournament-toggle');
    if (tournamentToggle) {
        tournamentToggle.addEventListener('click', () => {
            toggleTournamentMode();
        });
    }
}

async function toggleTournamentMode() {
    if (isTournamentMode()) {
        exitTournamentMode();
    } else {
        enterTournamentMode();
    }
    
    updateTournamentButtonState();
    updateURL();
    fetchLeaderboard();
}

function exitTournamentMode() {
    const currentSeason = getCurrentSeason();
    
    leaderboardFilters.serverName = '';
    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;
    leaderboardFilters.minGames = '3';
    delete leaderboardFilters.playlist;
    
    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) seasonSelect.value = 'current';
    
    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) serverFilter.value = '';
    
    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) playlistFilter.value = 'all';
    
    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) minGamesFilter.value = '3';
    
    toggleCustomDateInputs(false);
    
    const seasonIndicator = document.getElementById('season-indicator');
    const currentSeasonSpan = document.getElementById('current-season');
    if (seasonIndicator && currentSeasonSpan) {
        seasonIndicator.innerHTML = `Current Season: <span id="current-season">${currentSeason.displayName}</span>`;
    }
}

function enterTournamentMode() {
    const dates = getChristmasTournamentDates();
    
    leaderboardFilters.serverName = 'Christmas Tournament 2025';
    leaderboardFilters.startDate = dates.startDate;
    leaderboardFilters.endDate = dates.endDate;
    leaderboardFilters.minGames = '1';
    delete leaderboardFilters.playlist;
    
    const seasonSelect = document.getElementById('season-select');
    if (seasonSelect) seasonSelect.value = 'custom';
    
    const serverFilter = document.getElementById('server-filter');
    if (serverFilter) serverFilter.value = '';
    
    const playlistFilter = document.getElementById('playlist-filter');
    if (playlistFilter) playlistFilter.value = 'all';
    
    const minGamesFilter = document.getElementById('min-games-filter');
    if (minGamesFilter) minGamesFilter.value = '1';
    
    const startDateInput = document.getElementById('start-date');
    const endDateInput = document.getElementById('end-date');
    if (startDateInput) startDateInput.value = dates.startDate;
    if (endDateInput) endDateInput.value = dates.endDate;
    
    toggleCustomDateInputs(true);
}