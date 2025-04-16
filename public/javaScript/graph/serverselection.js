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
import { getCurrentPage, getFiltersFromURL, isCumulativeView, setCumulativeView, buildFilterURL, isRedirecting, showChartLoading, hideChartLoading } from './utilities.js';
import { fetchServerCounts, fetchServerHourCounts, fetchTerritoryCounts } from './api.js';
import { 
    IndividualServersGraph, 
    CombinedServersGraph, 
    TotalHoursGraph, 
    PopularTerritoriesGraph, 
    SetupStatisticsGraph 
} from './graphtypes.js';

export function toggleCheckbox(element, serverValue, event) {
    if (event) {
        event.stopPropagation();
    }

    if (element.dataset.processing === 'true') {
        return;
    }

    element.dataset.processing = 'true';
    element.classList.toggle('selected');
    
    let valueAttribute = 'server';
    
    if (TERRITORY_TYPES.includes(serverValue)) {
        valueAttribute = 'territory';
    } else if (serverValue.includes('vs') || typeof serverValue === 'object') {
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

export function toggleCumulativeView() {
    setCumulativeView(!isCumulativeView());

    const toggleButton = document.getElementById('toggleView');
    if (toggleButton) {
        toggleButton.textContent = isCumulativeView() ?
            'Toggle to Daily View' :
            'Toggle to Cumulative View';
    }

    const url = new URL(window.location);
    url.searchParams.set('cumulative', isCumulativeView());

    window.history.replaceState({}, '', url);
    window.location.reload();
}

export function initializeToggleViewState() {
    const urlParams = new URLSearchParams(window.location.search);
    const cumulativeParam = urlParams.get('cumulative');

    if (cumulativeParam !== null) {
        setCumulativeView(cumulativeParam === 'true');
    } else {
        setCumulativeView(false);
    }

    const toggleButton = document.getElementById('toggleView');
    if (toggleButton) {
        toggleButton.textContent = isCumulativeView() ?
            'Toggle to Daily View' :
            'Toggle to Cumulative View';

        toggleButton.removeEventListener('click', toggleCumulativeView);
        toggleButton.addEventListener('click', toggleCumulativeView);
    }

    return isCumulativeView();
}

export function initializeInputsFromURL() {
    const filters = getFiltersFromURL();

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
    const currentPage = getCurrentPage();
    
    switch (currentPage) {
        case 'individualServers':
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

        case '1v1setupStatistics':
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

export function triggerChartCreation() {
    showChartLoading();

    if (window.currentChart) {
        try {
            window.currentChart.destroy();
            window.currentChart = null;
        } catch (error) {
            console.error("Error destroying existing chart:", error);
        }
    }

    let chartInstance = null;
    const currentPage = getCurrentPage();

    switch (currentPage) {
        case 'individualServers':
            chartInstance = new IndividualServersGraph();
            break;
        case 'combinedServers':
            chartInstance = new CombinedServersGraph();
            break;
        case 'totalHoursPlayed':
            chartInstance = new TotalHoursGraph();
            break;
        case 'popularTerritories':
            chartInstance = new PopularTerritoriesGraph();
            break;
        case '1v1setupStatistics':
            chartInstance = new SetupStatisticsGraph();
            window.setupStatsGraph = chartInstance;
            break;
    }

    if (chartInstance) {
        chartInstance.createChart().then(() => {
            hideChartLoading();
        }).catch(err => {
            console.error("Error creating chart:", err);
            hideChartLoading();
        });
    } else {
        hideChartLoading();
    }

    window.preventAutoChartCreation = false;
}

export function setupEventListeners() {
    window.isRedirecting = false;

    document.addEventListener('click', (event) => {
        if (event.target.hasAttribute('onclick') ||
            event.target.closest('[onclick]')) {
            return;
        }

        const serverCheckbox = event.target.closest('.server-checkbox');
        if (serverCheckbox && !event.target.matches('input[type="checkbox"]')) {
            const valueAttribute = serverCheckbox.querySelector('input[type="checkbox"]')?.getAttribute('name') || 'server';
            const value = serverCheckbox.querySelector('input[type="checkbox"]')?.value || '';
            toggleCheckbox(serverCheckbox, value);
        }
    });

    const updateGraphBtn = document.getElementById('updateGraph');
    if (updateGraphBtn) {
        const newBtn = updateGraphBtn.cloneNode(true);
        updateGraphBtn.parentNode.replaceChild(newBtn, updateGraphBtn);

        newBtn.addEventListener('click', () => {
            window.isRedirecting = true;

            if (getCurrentPage() === '1v1setupStatistics') {
                showChartLoading();
            }

            setTimeout(() => {
                window.location.href = buildFilterURL();
            }, 50);
        });
    }
    
    const resetGraphBtn = document.getElementById('resetGraphQueries');
    if (resetGraphBtn) {
        resetGraphBtn.addEventListener('click', () => {
            window.dataCache = {};
            
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

    const toggleViewBtn = document.getElementById('toggleView');
    if (toggleViewBtn) {
        toggleViewBtn.addEventListener('click', toggleCumulativeView);
    }

    const graphFilterDropdown = document.getElementById('graph-filter');
    if (graphFilterDropdown) {
        graphFilterDropdown.addEventListener('change', (e) => {
            const selectedGraphType = e.target.value;

            if (selectedGraphType) {
                const urlMap = {
                    'individualServers': '/about',
                    'combinedServers': '/about/combined-servers',
                    'totalHoursPlayed': '/about/hours-played',
                    'popularTerritories': '/about/popular-territories',
                    '1v1setupStatistics': '/about/1v1-setup-statistics'
                };

                const url = new URL(urlMap[selectedGraphType], window.location.origin);
                const currentFilters = getFiltersFromURL();

                if (currentFilters.playerName) {
                    url.searchParams.set('playerName', currentFilters.playerName);
                }

                if (currentFilters.startDate) {
                    url.searchParams.set('startDate', currentFilters.startDate);
                }

                if (currentFilters.endDate) {
                    url.searchParams.set('endDate', currentFilters.endDate);
                }

                if (currentFilters.cumulative) {
                    url.searchParams.set('cumulative', currentFilters.cumulative);
                }

                window.location.href = url.toString();
            }
        });
    }
}

export function initializePageData() {
    const currentPage = getCurrentPage();
    
    switch (currentPage) {
        case 'individualServers':
            initializeCheckboxControls();
            fetchServerCounts();
            break;
        case 'combinedServers':
            break;
        case 'totalHoursPlayed':
            initializeCheckboxControls({ suffix: 'hours' });
            fetchServerHourCounts();
            break;
        case 'popularTerritories':
            initializeCheckboxControls({
                selectAllSelector: '#selectAllTerritories',
                deselectAllSelector: '#deselectAllTerritories',
                checkboxSelector: '.server-checkbox',
                valueAttribute: 'territory'
            });
            fetchTerritoryCounts();
            break;
        case '1v1setupStatistics':
            initializeCheckboxControls({
                selectAllSelector: '#selectAllSetups',
                deselectAllSelector: '#deselectAllSetups',
                checkboxSelector: '.setup-checkbox',
                valueAttribute: 'setup'
            });
            break;
    }
}

export function initializePageControls() {
    initializeInputsFromURL();
    setupEventListeners();
    initializePageData();

    window.requestAnimationFrame(() => {
        triggerChartCreation();
    });
}