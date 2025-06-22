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
import { setupCurrentSeason, checkSeasonEndingSoon, applySeasonFilter, toggleCustomDateInputs } from './seasons.js';
import { initializeFilterElements, initializePlaylistDropdown, applyUrlParameters, updateURL, resetFilters } from './filters.js';
import { fetchLeaderboard, fetchAllTimeMostActivePlayers, fetchPreviousSeasonWinners } from './api.js';

export function initializeLeaderboard() {
    setupCurrentSeason();
    initializeFilterElements();
    initializePlaylistDropdown();
    applyUrlParameters();
    setupEventListeners();
    checkSeasonEndingSoon();
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

            updateURL();
            fetchLeaderboard();
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

            updateURL();
            fetchLeaderboard();
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
            updateURL();
            fetchLeaderboard();
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

    const applyDateBtn = document.getElementById('apply-date-filter');
    if (applyDateBtn) {
        applyDateBtn.addEventListener('click', () => {
            updateURL();
            fetchLeaderboard();
        });
    }

    const resetFiltersBtn = document.getElementById('reset-filters');
    if (resetFiltersBtn) {
        resetFiltersBtn.addEventListener('click', resetFilters);
    }
}