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
//Last Edited 16-04-2025

import { SERVER_TYPES, TERRITORY_TYPES } from './constants.js';
import { getFiltersFromURL, buildFilterURL } from './utils.js';
import { clearDataCache } from './api.js';

export function toggleCheckbox(element, value, event) {
    if (event) {
        event.stopPropagation();
    }

    if (element.dataset.processing === 'true') {
        return;
    }

    element.dataset.processing = 'true';
    element.classList.toggle('selected');
    
    let valueAttribute = 'server';
    
    if (TERRITORY_TYPES.includes(value)) {
        valueAttribute = 'territory';
    } else if (value.includes('vs') || typeof value === 'object') {
        valueAttribute = 'setup';
    }

    const checkbox = element.querySelector(`input[type="checkbox"][name="${valueAttribute}"]`);
    if (checkbox) {
        checkbox.checked = !checkbox.checked;
    }

    setTimeout(() => {
        delete element.dataset.processing;
    }, 10);
}

export function toggleServer(element, serverKey) {
    toggleCheckbox(element, serverKey);
}

export function toggleCumulativeView(callback) {
    window.isCumulativeView = !window.isCumulativeView;

    const toggleButton = document.getElementById('toggleView');
    if (toggleButton) {
        toggleButton.textContent = window.isCumulativeView ?
            'Toggle to Daily View' :
            'Toggle to Cumulative View';
    }

    const url = new URL(window.location);
    url.searchParams.set('cumulative', window.isCumulativeView);
    window.history.replaceState({}, '', url);

    if (callback && typeof callback === 'function') {
        callback();
    } else {
        window.location.reload();
    }
}

export function initializeToggleViewState() {
    const urlParams = new URLSearchParams(window.location.search);
    const cumulativeParam = urlParams.get('cumulative');

    if (cumulativeParam !== null) {
        window.isCumulativeView = cumulativeParam === 'true';
    } else {
        window.isCumulativeView = false;
    }

    const toggleButton = document.getElementById('toggleView');
    if (toggleButton) {
        toggleButton.textContent = window.isCumulativeView ?
            'Toggle to Daily View' :
            'Toggle to Cumulative View';

        toggleButton.removeEventListener('click', toggleCumulativeView);
        toggleButton.addEventListener('click', toggleCumulativeView);
    }

    return window.isCumulativeView;
}

export function initializeInputsFromURL() {
    const filters = getFiltersFromURL(SERVER_TYPES, TERRITORY_TYPES);

    const playerInput = document.querySelector('.player-input');
    if (playerInput && filters.playerName) {
        playerInput.value = filters.playerName;
    }

    const startDateInput = document.getElementById('startDate');
    const endDateInput = document.getElementById('endDate');

    if (startDateInput && filters.startDate) {
        startDateInput.value = filters.startDate;
    }

    if (endDateInput && filters.endDate) {
        endDateInput.value = filters.endDate;
    }

    const yAxisInput = document.getElementById('yAxisMax');
    if (yAxisInput && filters.yAxisMax) {
        yAxisInput.value = filters.yAxisMax;
    }

    initializeCheckboxStates(filters);
    initializeToggleViewState();
}

export function initializeCheckboxStates(filters) {
    const currentPage = getCurrentPageFromPath();
    
    switch (currentPage) {
        case 'individualServers':
        case 'totalHoursPlayed':
            document.querySelectorAll('input[name="server"]').forEach(checkbox => {
                const isSelected = filters.servers.includes(checkbox.value);
                checkbox.checked = isSelected;
                const parent = checkbox.closest('.server-checkbox');
                if (parent) {
                    if (isSelected) {
                        parent.classList.add('selected');
                    } else {
                        parent.classList.remove('selected');
                    }
                }
            });
            break;

        case 'popularTerritories':
            document.querySelectorAll('input[name="territory"]').forEach(checkbox => {
                const isSelected = filters.territories.includes(checkbox.value);
                checkbox.checked = isSelected;
                const parent = checkbox.closest('.server-checkbox');
                if (parent) {
                    if (isSelected) {
                        parent.classList.add('selected');
                    } else {
                        parent.classList.remove('selected');
                    }
                }
            });
            break;
    }
}

export function initializeCheckboxControls(options = {}) {
    const {
        selectAllSelector = '#selectAllServers',
        deselectAllSelector = '#deselectAllServers',
        checkboxSelector = '.server-checkbox',
        valueAttribute = 'server'
    } = options;

    const selectAllBtn = document.querySelector(selectAllSelector);
    const deselectAllBtn = document.querySelector(deselectAllSelector);

    if (selectAllBtn) {
        selectAllBtn.addEventListener('click', () => {
            document.querySelectorAll(checkboxSelector).forEach(box => {
                box.classList.add('selected');
                const checkbox = box.querySelector(`input[type="checkbox"][name="${valueAttribute}"]`);
                if (checkbox) checkbox.checked = true;
            });
        });
    }

    if (deselectAllBtn) {
        deselectAllBtn.addEventListener('click', () => {
            document.querySelectorAll(checkboxSelector).forEach(box => {
                box.classList.remove('selected');
                const checkbox = box.querySelector(`input[type="checkbox"][name="${valueAttribute}"]`);
                if (checkbox) checkbox.checked = false;
            });
        });
    }
}

export function populateSetupCheckboxes(data) {
    const checkboxContainer = document.querySelector('.setup-grid');
    if (!checkboxContainer) return;

    const filters = getFiltersFromURL(SERVER_TYPES, TERRITORY_TYPES);

    checkboxContainer.innerHTML = '';
    const columns = Array.from({ length: 4 }, () => {
        const col = document.createElement('div');
        col.className = 'checkbox-column';
        checkboxContainer.appendChild(col);
        return col;
    });

    const sortedSetups = [...data]
        .sort((a, b) => b.total_games - a.total_games);

    sortedSetups.forEach((setup, index) => {
        const isSelected = filters.setups.length === 0 || filters.setups.includes(setup.setup);

        const setupBox = document.createElement('div');
        setupBox.className = `server-checkbox setup-checkbox ${isSelected ? 'selected' : ''}`;
        setupBox.setAttribute('onclick', `toggleCheckbox(this, '${setup.setup}')`);
        setupBox.dataset.territory1 = setup.territories[0];
        setupBox.dataset.territory2 = setup.territories[1];

        setupBox.innerHTML = `
            <input type="checkbox" name="setup" value="${setup.setup}" ${isSelected ? 'checked' : ''} style="display: none;">
            <span class="server-name">${setup.setup.replace('_vs_', ' vs ')}</span>
            <span class="game-count">${setup.total_games} games</span>
        `;

        columns[index % 4].appendChild(setupBox);
    });
}

export function setupResetButton() {
    const resetGraphBtn = document.getElementById('resetGraphQueries');
    if (resetGraphBtn) {
        resetGraphBtn.addEventListener('click', () => {
            clearDataCache();
            
            const keysToRemove = [];
            for (let i = 0; i < localStorage.length; i++) {
                const key = localStorage.key(i);
                if (key && (
                    key.includes('graph') || 
                    key.includes('chart') || 
                    key.includes('server') || 
                    key.includes('territory') ||
                    key.includes('hours')
                )) {
                    keysToRemove.push(key);
                }
            }
            keysToRemove.forEach(key => {
                localStorage.removeItem(key);
            });
            
            sessionStorage.clear();
            
            const baseUrl = window.location.pathname;
            window.history.replaceState({}, document.title, baseUrl);
            window.location.reload(true);
        });
    }
}

function getCurrentPageFromPath() {
    const pathname = window.location.pathname;
    
    if (pathname === '/about') {
        return 'individualServers';
    } else if (pathname === '/about/combined-servers') {
        return 'combinedServers';
    } else if (pathname === '/about/hours-played') {
        return 'totalHoursPlayed';
    } else if (pathname === '/about/popular-territories') {
        return 'popularTerritories';
    } else if (pathname === '/about/1v1-setup-statistics') {
        return '1v1setupStatistics';
    } else {
        return 'unknown';
    }
}