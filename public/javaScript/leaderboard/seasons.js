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

import { firstSeason, seasonBoundaries, leaderboardFilters } from './constants.js';

let seasons = {};

export function getAllSeasons() {
    const today = new Date();
    const seasons = { beginning: firstSeason };

    const beginningEnd = new Date(firstSeason.endDate);
    if (today <= beginningEnd) {
        return seasons;
    }

    let year = 2;
    let season = 0;
    let seasonStart = new Date(beginningEnd);
    seasonStart.setDate(seasonStart.getDate() + 1);

    while (true) {
        season = (season % 4) + 1;

        const boundary = seasonBoundaries[season - 1];

        let seasonEnd;
        if (season === 4) {
            seasonEnd = new Date(seasonStart.getFullYear() + 1, boundary.endMonth - 1, boundary.endDay);
        } else {
            seasonEnd = new Date(seasonStart.getFullYear(), boundary.endMonth - 1, boundary.endDay);
        }

        if (seasonStart <= today) {
            const seasonKey = `y${year}s${season}`;
            seasons[seasonKey] = {
                name: `Y${year}S${season}`,
                displayName: `Y${year}S${season}`,
                startDate: seasonStart.toISOString().split('T')[0],
                endDate: seasonEnd.toISOString().split('T')[0],
                year,
                season
            };
        } else {
            break;
        }

        seasonStart = new Date(seasonEnd);
        seasonStart.setDate(seasonStart.getDate() + 1);

        if (season === 4) {
            year++;
        }

        if (Object.keys(seasons).length > 20) break;
    }

    return seasons;
}

export function getCurrentSeason() {
    const today = new Date();
    seasons = getAllSeasons();

    for (const [key, season] of Object.entries(seasons)) {
        const startDate = new Date(season.startDate);
        const endDate = new Date(season.endDate);

        if (today >= startDate && today <= endDate) {
            return { key, ...season };
        }
    }

    // If no current season is found, return the most recent one
    const sortedSeasons = Object.entries(seasons)
        .sort((a, b) => new Date(b[1].endDate) - new Date(a[1].endDate));

    if (sortedSeasons.length > 0) {
        const [lastKey, lastSeason] = sortedSeasons[0];
        const lastEndDate = new Date(lastSeason.endDate);

        if (today > lastEndDate) {
            return { key: lastKey, ...lastSeason };
        }
    }

    return { key: 'beginning', ...firstSeason };
}

export function setupCurrentSeason() {
    const currentSeason = getCurrentSeason();

    const currentSeasonElement = document.getElementById('current-season');
    if (currentSeasonElement) {
        currentSeasonElement.textContent = currentSeason.displayName;
    }

    leaderboardFilters.startDate = currentSeason.startDate;
    leaderboardFilters.endDate = currentSeason.endDate;

    updateSeasonSelector(currentSeason.key);
}

export function updateSeasonSelector(currentSeasonKey = 'current') {
    const seasonSelect = document.getElementById('season-select');
    if (!seasonSelect) return;

    while (seasonSelect.options.length > 2) {
        seasonSelect.remove(2);
    }

    const today = new Date();
    
    seasons = getAllSeasons();
    const availableSeasons = Object.entries(seasons)
        .filter(([key, season]) => new Date(season.startDate) <= today)
        .sort((a, b) => new Date(b[1].startDate) - new Date(a[1].startDate));

    for (const [key, season] of availableSeasons) {
        const option = document.createElement('option');
        option.value = key;
        option.textContent = season.displayName;
        seasonSelect.add(option);
    }

    seasonSelect.value = currentSeasonKey;
}

export function checkSeasonEndingSoon() {
    const today = new Date();
    const currentSeason = getCurrentSeason();
    const seasonEndDate = new Date(currentSeason.endDate);

    const daysUntilSeasonEnd = Math.floor((seasonEndDate - today) / (1000 * 60 * 60 * 24));

    if (daysUntilSeasonEnd <= 7 && daysUntilSeasonEnd >= 0) {
        const seasonBanner = document.getElementById('season-end-banner');
        const seasonEndDateElement = document.getElementById('season-end-date');

        if (seasonBanner && seasonEndDateElement) {
            const options = { year: 'numeric', month: 'long', day: 'numeric' };
            seasonEndDateElement.textContent = seasonEndDate.toLocaleDateString(undefined, options);

            seasonBanner.style.display = 'flex';
        }
    }
}

export function applySeasonFilter(seasonValue) {
    switch (seasonValue) {
        case 'current':
            const currentSeason = getCurrentSeason();
            leaderboardFilters.startDate = currentSeason.startDate;
            leaderboardFilters.endDate = currentSeason.endDate;
            break;

        case 'all':
            leaderboardFilters.startDate = '';
            leaderboardFilters.endDate = '';
            break;

        case 'custom':
            toggleCustomDateInputs(true);
            break;

        default:
            if (seasons[seasonValue]) {
                leaderboardFilters.startDate = seasons[seasonValue].startDate;
                leaderboardFilters.endDate = seasons[seasonValue].endDate;
            }
            break;
    }

    const startDateInput = document.getElementById('start-date');
    const endDateInput = document.getElementById('end-date');

    if (startDateInput) startDateInput.value = leaderboardFilters.startDate;
    if (endDateInput) endDateInput.value = leaderboardFilters.endDate;
}

export function toggleCustomDateInputs(show) {
    const customDateRange = document.getElementById('custom-date-range');
    if (customDateRange) {
        customDateRange.style.display = show ? 'flex' : 'none';

        if (show) {
            const filtersSection = document.getElementById('leaderboard-filters');
            const filtersToggle = document.getElementById('filters-toggle');

            if (filtersSection && !filtersSection.classList.contains('expanded')) {
                filtersSection.classList.add('expanded');
                if (filtersToggle) filtersToggle.classList.add('active');
            }

            const advancedFilters = document.getElementById('advanced-filters');
            if (advancedFilters && !advancedFilters.classList.contains('expanded')) {
                advancedFilters.classList.add('expanded');
                if (filtersToggle) filtersToggle.classList.add('active');
            }
        }
    }

    const dateRangeContainer = document.querySelector('.date-range-container');
    if (dateRangeContainer) {
        dateRangeContainer.style.display = show ? 'flex' : 'none';
    }
}