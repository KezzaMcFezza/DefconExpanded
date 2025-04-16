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

import * as constants from './constants.js';
import * as api from './api.js';
import * as ui from './ui.js';
import * as utils from './utils.js';
import { BaseGraph } from './basegraph.js';


let graphTypes = null;
const importGraphTypes = async () => {
    if (!graphTypes) {
        graphTypes = await import('./graphtypes.js');
    }
    return graphTypes;
};

let currentChart = null;
let preventAutoChartCreation = true;

export { BaseGraph };

function setupEventListeners() {
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
            ui.toggleCheckbox(serverCheckbox, value);
        }
    });

    const updateGraphBtn = document.getElementById('updateGraph');
    if (updateGraphBtn) {
        const newBtn = updateGraphBtn.cloneNode(true);
        updateGraphBtn.parentNode.replaceChild(newBtn, updateGraphBtn);

        newBtn.addEventListener('click', () => {
            window.isRedirecting = true;

            if (utils.getCurrentPageType() === constants.GRAPH_TYPES.SETUP_STATISTICS) {
                utils.showChartLoading();
            }

            setTimeout(() => {
                window.location.href = utils.buildFilterURL(utils.getCurrentPageType());
            }, 50);
        });
    }
    
    ui.setupResetButton();

    const toggleViewBtn = document.getElementById('toggleView');
    if (toggleViewBtn) {
        toggleViewBtn.addEventListener('click', ui.toggleCumulativeView);
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
                const currentFilters = utils.getFiltersFromURL(constants.SERVER_TYPES, constants.TERRITORY_TYPES);

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

function initializePageData() {
    const currentPageType = utils.getCurrentPageType();
    
    switch (currentPageType) {
        case constants.GRAPH_TYPES.INDIVIDUAL_SERVERS:
            ui.initializeCheckboxControls();
            fetchServerCounts();
            break;
        case constants.GRAPH_TYPES.COMBINED_SERVERS:
            break;
        case constants.GRAPH_TYPES.TOTAL_HOURS:
            ui.initializeCheckboxControls({ suffix: 'hours' });
            fetchServerHourCounts();
            break;
        case constants.GRAPH_TYPES.POPULAR_TERRITORIES:
            ui.initializeCheckboxControls({
                selectAllSelector: '#selectAllTerritories',
                deselectAllSelector: '#deselectAllTerritories',
                checkboxSelector: '.server-checkbox',
                valueAttribute: 'territory'
            });
            fetchTerritoryCounts();
            break;
        case constants.GRAPH_TYPES.SETUP_STATISTICS:
            ui.initializeCheckboxControls({
                selectAllSelector: '#selectAllSetups',
                deselectAllSelector: '#deselectAllSetups',
                checkboxSelector: '.setup-checkbox',
                valueAttribute: 'setup'
            });
            break;
    }
}

async function fetchServerCounts() {
    if (utils.getCurrentPageType() !== constants.GRAPH_TYPES.INDIVIDUAL_SERVERS) return;

    const filters = utils.getFiltersFromURL(constants.SERVER_TYPES, constants.TERRITORY_TYPES);
    const serverCounts = await api.fetchServerCounts('individualServers', constants.SERVER_TYPES, filters);
    
    if (!serverCounts) return;

    const countElements = Object.fromEntries(
        constants.SERVER_TYPES.map(serverType => [
            serverType,
            document.getElementById(`count-${serverType}`)
        ])
    );

    utils.updateCountDisplay(countElements, serverCounts, { precision: 'integer' });
}

async function fetchServerHourCounts() {
    if (utils.getCurrentPageType() !== constants.GRAPH_TYPES.TOTAL_HOURS) return;

    const filters = utils.getFiltersFromURL(constants.SERVER_TYPES, constants.TERRITORY_TYPES);
    const data = await api.fetchData('totalHoursPlayed', {
        byServer: true,
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return;

    const serverHours = Object.fromEntries(
        constants.SERVER_TYPES.map(serverType => [serverType, 0])
    );

    data.forEach(day => {
        constants.SERVER_TYPES.forEach(serverType => {
            serverHours[serverType] += day[serverType] || 0;
        });
    });

    const countElements = Object.fromEntries(
        constants.SERVER_TYPES.map(serverType => [
            serverType,
            document.getElementById(`hours-count-${serverType}`) || document.getElementById(`count-${serverType}`)
        ])
    );

    utils.updateCountDisplay(countElements, serverHours, {
        suffix: 'hours',
        thresholds: [
            { threshold: 50, color: '#5eff00' },
            { threshold: 10, color: '#ffd700' },
            { threshold: 0, color: '#ff6347' }
        ]
    });
}

async function fetchTerritoryCounts() {
    if (utils.getCurrentPageType() !== constants.GRAPH_TYPES.POPULAR_TERRITORIES) return;

    const filters = utils.getFiltersFromURL(constants.SERVER_TYPES, constants.TERRITORY_TYPES);
    const data = await api.fetchData('popularTerritories', {
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return;

    const territoryCounts = Object.fromEntries(
        constants.TERRITORY_TYPES.map(territoryKey => [territoryKey, 0])
    );

    data.forEach(day => {
        constants.TERRITORY_TYPES.forEach(territoryKey => {
            territoryCounts[territoryKey] += day[territoryKey] || 0;
        });
    });

    const countElements = Object.fromEntries(
        constants.TERRITORY_TYPES.map(territoryKey => [
            territoryKey,
            document.getElementById(`territory-count-${territoryKey}`) || document.getElementById(`count-${territoryKey}`)
        ])
    );

    utils.updateCountDisplay(countElements, territoryCounts, {
        suffix: 'times',
        precision: 'integer',
        thresholds: [
            { threshold: 250, color: '#5eff00' },
            { threshold: 100, color: '#ffd700' },
            { threshold: 0, color: '#ff6347' }
        ]
    });
}

async function triggerChartCreation() {
    utils.showChartLoading();

    if (window.currentChart) {
        try {
            window.currentChart.destroy();
            window.currentChart = null;
        } catch (error) {
            console.error("Error destroying existing chart:", error);
        }
    }

    let chartInstance = null;
    const currentPageType = utils.getCurrentPageType();
    const graphModules = await importGraphTypes();

    switch (currentPageType) {
        case constants.GRAPH_TYPES.INDIVIDUAL_SERVERS:
            chartInstance = new graphModules.IndividualServersGraph();
            break;
        case constants.GRAPH_TYPES.COMBINED_SERVERS:
            chartInstance = new graphModules.CombinedServersGraph();
            break;
        case constants.GRAPH_TYPES.TOTAL_HOURS:
            chartInstance = new graphModules.TotalHoursGraph();
            break;
        case constants.GRAPH_TYPES.POPULAR_TERRITORIES:
            chartInstance = new graphModules.PopularTerritoriesGraph();
            break;
        case constants.GRAPH_TYPES.SETUP_STATISTICS:
            chartInstance = new graphModules.SetupStatisticsGraph();
            window.setupStatsGraph = chartInstance;
            break;
    }

    if (chartInstance) {
        chartInstance.createChart().then(() => {
            utils.hideChartLoading();
        }).catch(err => {
            console.error("Error creating chart:", err);
            utils.hideChartLoading();
        });
    } else {
        utils.hideChartLoading();
    }

    preventAutoChartCreation = false;
}

export async function initializeGraphs() {
    
    window.isCumulativeView = false;
    window.currentChart = null;
    window.preventAutoChartCreation = true;
    window.toggleCheckbox = ui.toggleCheckbox;
    window.toggleServer = ui.toggleServer;
    window.toggleCumulativeView = ui.toggleCumulativeView;
    window.populateSetupCheckboxes = ui.populateSetupCheckboxes;
    window.fetchServerCounts = fetchServerCounts;
    window.fetchServerHourCounts = fetchServerHourCounts;
    window.fetchTerritoryCounts = fetchTerritoryCounts;
    
    ui.initializeInputsFromURL();
    setupEventListeners();
    initializePageData();
    
    window.requestAnimationFrame(() => {
        triggerChartCreation();
    });
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeGraphs);
} else {
    initializeGraphs();
}