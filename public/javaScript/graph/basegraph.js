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

import * as api from './api.js';
import * as utils from './utils.js';

export class BaseGraph {
    constructor() {
        this.chartContainer = document.getElementById('gamesChart');
        this.width = this.chartContainer?.offsetWidth || 0;
        this.height = 550;

        this.initializeBaseControls();
        this.initializeEventListeners();
    }

    getPlayerName() {
        const urlParams = new URLSearchParams(window.location.search);
        const playerNameParam = urlParams.get('playerName');

        if (playerNameParam) {
            return playerNameParam;
        }

        return document.querySelector('.player-input')?.value || '';
    }

    initializeBaseControls() {
        const urlParams = new URLSearchParams(window.location.search);
        const startDateParam = urlParams.get('startDate');
        const endDateParam = urlParams.get('endDate');
        const yAxisMaxParam = urlParams.get('yAxisMax');

        const endDate = endDateParam ? new Date(endDateParam) : new Date();

        const startDate = startDateParam ?
            new Date(startDateParam) :
            new Date('2024-08-27T00:00:00');

        const startDateInput = document.getElementById('startDate');
        const endDateInput = document.getElementById('endDate');

        if (startDateInput) startDateInput.valueAsDate = startDate;
        if (endDateInput) endDateInput.valueAsDate = endDate;

        const yAxisMaxInput = document.getElementById('yAxisMax');
        if (yAxisMaxInput && yAxisMaxParam) {
            yAxisMaxInput.value = yAxisMaxParam;
        }

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
        }

        const pathname = window.location.pathname;

        if (pathname === '/about' && typeof window.fetchServerCounts === 'function') {
            window.fetchServerCounts();
        }

        if (pathname === '/about/hours-played' && typeof window.fetchServerHourCounts === 'function') {
            window.fetchServerHourCounts();
        }

        if (pathname === '/about/popular-territories' && typeof window.fetchTerritoryCounts === 'function') {
            window.fetchTerritoryCounts();
        }
    }

    initializeEventListeners() {
        window.addEventListener('resize', _.debounce(() => this.createChart(), 250));

        const exportButton = document.getElementById('exportData');
        if (exportButton) {
            exportButton.addEventListener('click', () => this.exportData());
        }
    }

    buildFilterURL() {
        return utils.buildFilterURL(utils.getCurrentPageType());
    }

    async fetchData(queryParams) {
        return api.fetchData(utils.getCurrentPageType(), queryParams);
    }
    
    exportData() {
    }
}