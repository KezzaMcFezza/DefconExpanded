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

import { PAGE_TO_GRAPH_TYPE } from './constants.js';

export function getCurrentPageType() {
    const pathname = window.location.pathname;
    return PAGE_TO_GRAPH_TYPE[pathname] || 'unknown';
}

export function updateCountDisplay(countElements, counts, options = {}) {
    const {
        suffix = 'games',
        thresholds = [
            { threshold: 70, color: '#5eff00' },
            { threshold: 20, color: '#ffd700' },
            { threshold: 0, color: '#ff6347' }
        ],
        precision = 'auto'
    } = options;

    Object.entries(counts).forEach(([key, count]) => {
        const countElement = countElements[key];
        if (!countElement) return;

        let formattedCount;
        if (precision === 'auto') {
            formattedCount = suffix === 'hours' ?
                count.toFixed(1) :
                Math.round(count).toString();
        } else if (precision === 'integer') {
            formattedCount = Math.round(count).toString();
        } else {
            formattedCount = count.toFixed(1);
        }

        countElement.textContent = `${formattedCount} ${suffix}`;

        const matchedThreshold = thresholds.find(t => count > t.threshold);
        countElement.style.color = matchedThreshold ? matchedThreshold.color : '#b8b8b8';
    });
}

export function getFiltersFromURL(serverTypes, territoryTypes) {
    const urlParams = new URLSearchParams(window.location.search);

    const basicFilters = {
        playerName: urlParams.get('playerName') || '',
        startDate: urlParams.get('startDate') || '',
        endDate: urlParams.get('endDate') || '',
        yAxisMax: urlParams.get('yAxisMax') || '',
        cumulative: urlParams.get('cumulative') === 'true'
    };

    const selectedItems = {
        servers: [],
        territories: [],
        setups: []
    };

    const serverParam = urlParams.get('servers');
    if (serverParam) {
        selectedItems.servers = serverParam.split(',');
    } else {
        selectedItems.servers = [...serverTypes];
    }

    const territoryParam = urlParams.get('territories');
    if (territoryParam) {
        selectedItems.territories = territoryParam.split(',');
    } else {
        selectedItems.territories = [...territoryTypes];
    }

    const setupParam = urlParams.get('setups');
    if (setupParam) {
        selectedItems.setups = setupParam.split(',');
    }

    return { ...basicFilters, ...selectedItems };
}

export function buildFilterURL(currentPage) {
    const url = new URL(window.location);

    const playerName = document.querySelector('.player-input')?.value || '';
    const startDate = document.getElementById('startDate')?.value;
    const endDate = document.getElementById('endDate')?.value;
    const yAxisMax = document.getElementById('yAxisMax')?.value;
    const isCumulativeView = window.isCumulativeView || false;

    ['playerName', 'startDate', 'endDate', 'yAxisMax', 'cumulative', 'servers', 'territories', 'setups'].forEach(param => {
        url.searchParams.delete(param);
    });

    if (playerName) url.searchParams.set('playerName', playerName);
    if (startDate) url.searchParams.set('startDate', startDate);
    if (endDate) url.searchParams.set('endDate', endDate);
    if (yAxisMax) url.searchParams.set('yAxisMax', yAxisMax);
    url.searchParams.set('cumulative', isCumulativeView);

    switch (currentPage) {
        case 'individualServers':
        case 'totalHoursPlayed':
            const selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
                .map(checkbox => checkbox.value);
            if (selectedServers.length > 0) {
                url.searchParams.set('servers', selectedServers.join(','));
            }
            break;

        case 'popularTerritories':
            const selectedTerritories = Array.from(document.querySelectorAll('input[name="territory"]:checked'))
                .map(checkbox => checkbox.value);
            if (selectedTerritories.length > 0) {
                url.searchParams.set('territories', selectedTerritories.join(','));
            }
            break;

        case '1v1setupStatistics':
            const selectedSetups = Array.from(document.querySelectorAll('input[name="setup"]:checked'))
                .map(checkbox => checkbox.value);
            if (selectedSetups.length > 0) {
                url.searchParams.set('setups', selectedSetups.join(','));
            }
            break;
    }

    return url.toString();
}

export function showChartLoading() {
    const chartContainer = document.getElementById('gamesChart');
    if (!chartContainer) return;

    let loadingOverlay = document.getElementById('chart-loading-overlay');
    if (!loadingOverlay) {
        loadingOverlay = document.createElement('div');
        loadingOverlay.id = 'chart-loading-overlay';
        loadingOverlay.style.cssText = `
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(20, 20, 30, 0.7);
            display: flex;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            color: #4da6ff;
            font-family: 'Orbitron', sans-serif;
        `;

        const spinner = document.createElement('div');
        spinner.className = 'loading-spinner';
        spinner.innerHTML = `
            <div style="text-align: center;">
                <div style="border: 4px solid rgba(77, 166, 255, 0.3); 
                            border-radius: 50%; 
                            border-top: 4px solid #4da6ff; 
                            width: 40px; 
                            height: 40px; 
                            margin: 0 auto 15px auto;
                            animation: spin 1s linear infinite;"></div>
                <div>Loading chart...</div>
            </div>
            <style>
                @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
            </style>
        `;

        loadingOverlay.appendChild(spinner);

        chartContainer.style.position = 'relative';
        chartContainer.appendChild(loadingOverlay);
    } else {
        loadingOverlay.style.display = 'flex';
    }
}

export function hideChartLoading() {
    const loadingOverlay = document.getElementById('chart-loading-overlay');
    if (loadingOverlay) {
        loadingOverlay.style.display = 'none';
    }
}