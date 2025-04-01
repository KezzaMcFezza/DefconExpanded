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
//Last Edited 20-03-2025

const SERVER_TYPES = [
    'new_player', 'training', 'defcon_random', 'defcon_best', 'defcon_afas',
    'defcon_eusa', 'defcon_default', 'defcon_2v2', 'tournament_2v2',
    'defcon_2v2_special', 'mojo_2v2', 'sony_hoov', 'defcon_3v3', 'defcon_4v4',
    'muricon', 'cg_2v2_2815', 'cg_2v2_28141', 'cg_1v1_2815', 'cg_1v1_28141',
    'defcon_8p_diplo', 'defcon_10p_diplo'
];

const TERRITORY_TYPES = [
    'na', 'sa', 'eu', 'ru', 'af', 'as',
    'au', 'we', 'ea', 'ant', 'naf', 'saf'
];

const CURRENT_PAGE = getCurrentPage();

const dataCache = {};

function getCurrentPage() {
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

async function fetchData(graphType, options = {}) {
    const { byServer = false, playerName, startDate, endDate, skipCache = false } = options;

    const cacheKey = `${graphType}_${playerName || ''}_${startDate || ''}_${endDate || ''}`;

    if (!skipCache && dataCache[cacheKey]) {
        return dataCache[cacheKey];
    }

    try {
        const queryParams = new URLSearchParams({
            graphType
        });

        if (byServer) {
            queryParams.append('byServer', 'true');
        }

        if (playerName) {
            queryParams.append('playerName', playerName);
        }

        if (startDate) {
            queryParams.append('startDate', startDate);
        }
        if (endDate) {
            queryParams.append('endDate', endDate);
        }

        const response = await fetch(`/api/games-timeline?${queryParams.toString()}`);
        const data = await response.json();

        if (!data || data.length === 0) {
            console.warn(`No data returned for query: ${queryParams.toString()}`);
            return null;
        }

        dataCache[cacheKey] = data;
        return data;
    } catch (error) {
        console.error(`Error fetching ${graphType} data:`, error);
        return null;
    }
}

function updateCountDisplay(countElements, counts, options = {}) {
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

function toggleCheckbox(element, serverValue, event) {
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


function toggleServer(element, serverKey) {
    toggleCheckbox(element, serverKey);
}

function toggleCumulativeView() {
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

    window.location.reload();
}

function initializeToggleViewState() {
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

function getFiltersFromURL() {
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
        selectedItems.servers = [...SERVER_TYPES];
    }

    const territoryParam = urlParams.get('territories');
    if (territoryParam) {
        selectedItems.territories = territoryParam.split(',');
    } else {
        selectedItems.territories = [...TERRITORY_TYPES];
    }

    const setupParam = urlParams.get('setups');
    if (setupParam) {
        selectedItems.setups = setupParam.split(',');
    }

    return { ...basicFilters, ...selectedItems };
}

function initializeInputsFromURL() {
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

function initializeCheckboxStates(filters) {
    switch (CURRENT_PAGE) {
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

async function fetchServerCounts() {
    if (CURRENT_PAGE !== 'individualServers') return;

    const filters = getFiltersFromURL();

    const data = await fetchData('individualServers', {
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return;

    const serverCounts = Object.fromEntries(
        SERVER_TYPES.map(serverType => [serverType, 0])
    );

    data.forEach(day => {
        SERVER_TYPES.forEach(serverType => {
            serverCounts[serverType] += day[serverType] || 0;
        });
    });

    const countElements = Object.fromEntries(
        SERVER_TYPES.map(serverType => [
            serverType,
            document.getElementById(`count-${serverType}`)
        ])
    );

    updateCountDisplay(countElements, serverCounts, { precision: 'integer' });
}

async function fetchServerHourCounts() {
    if (CURRENT_PAGE !== 'totalHoursPlayed') return;

    const filters = getFiltersFromURL();

    const data = await fetchData('totalHoursPlayed', {
        byServer: true,
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return;

    const serverHours = Object.fromEntries(
        SERVER_TYPES.map(serverType => [serverType, 0])
    );

    data.forEach(day => {
        SERVER_TYPES.forEach(serverType => {
            serverHours[serverType] += day[serverType] || 0;
        });
    });

    
    const countElements = Object.fromEntries(
        SERVER_TYPES.map(serverType => [
            serverType,
            document.getElementById(`hours-count-${serverType}`) || document.getElementById(`count-${serverType}`)
        ])
    );

    updateCountDisplay(countElements, serverHours, {
        suffix: 'hours',
        thresholds: [
            { threshold: 50, color: '#5eff00' },
            { threshold: 10, color: '#ffd700' },
            { threshold: 0, color: '#ff6347' }
        ]
    });
}

async function fetchTerritoryCounts() {
    if (CURRENT_PAGE !== 'popularTerritories') return;

    const filters = getFiltersFromURL();

    const data = await fetchData('popularTerritories', {
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return;

    const territoryCounts = Object.fromEntries(
        TERRITORY_TYPES.map(territoryKey => [territoryKey, 0])
    );

    data.forEach(day => {
        TERRITORY_TYPES.forEach(territoryKey => {
            territoryCounts[territoryKey] += day[territoryKey] || 0;
        });
    });

    
    const countElements = Object.fromEntries(
        TERRITORY_TYPES.map(territoryKey => [
            territoryKey,
            document.getElementById(`territory-count-${territoryKey}`) || document.getElementById(`count-${territoryKey}`)
        ])
    );

    updateCountDisplay(countElements, territoryCounts, {
        suffix: 'times',
        precision: 'integer',
        thresholds: [
            { threshold: 250, color: '#5eff00' },
            { threshold: 100, color: '#ffd700' },
            { threshold: 0, color: '#ff6347' }
        ]
    });
}

function buildFilterURL() {
    const url = new URL(window.location);

    const playerName = document.querySelector('.player-input')?.value || '';
    const startDate = document.getElementById('startDate')?.value;
    const endDate = document.getElementById('endDate')?.value;
    const yAxisMax = document.getElementById('yAxisMax')?.value;

    ['playerName', 'startDate', 'endDate', 'yAxisMax', 'cumulative', 'servers', 'territories', 'setups'].forEach(param => {
        url.searchParams.delete(param);
    });

    if (playerName) {
        url.searchParams.set('playerName', playerName);
    }

    if (startDate) {
        url.searchParams.set('startDate', startDate);
    }

    if (endDate) {
        url.searchParams.set('endDate', endDate);
    }

    if (yAxisMax) {
        url.searchParams.set('yAxisMax', yAxisMax);
    }

    switch (CURRENT_PAGE) {
        case 'individualServers':
            const selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
                .map(checkbox => checkbox.value);

            if (selectedServers.length > 0 && selectedServers.length < SERVER_TYPES.length) {
                url.searchParams.set('servers', selectedServers.join(','));
            }
            break;

        case 'popularTerritories':
            const selectedTerritories = Array.from(document.querySelectorAll('input[name="territory"]:checked'))
                .map(checkbox => checkbox.value);

            if (selectedTerritories.length > 0 && selectedTerritories.length < TERRITORY_TYPES.length) {
                url.searchParams.set('territories', selectedTerritories.join(','));
            }
            break;

        case 'totalHoursPlayed':
            const selectedHoursServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
                .map(checkbox => checkbox.value);

            if (selectedHoursServers.length > 0 && selectedHoursServers.length < SERVER_TYPES.length) {
                url.searchParams.set('servers', selectedHoursServers.join(','));
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

    url.searchParams.set('cumulative', window.isCumulativeView || false);

    return url.toString();
}

function initializeCheckboxControls(options = {}) {
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

function populateSetupCheckboxes(data) {
    const checkboxContainer = document.querySelector('.setup-grid');
    if (!checkboxContainer) return;

    const filters = getFiltersFromURL();

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

function showChartLoading() {
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

function hideChartLoading() {
    const loadingOverlay = document.getElementById('chart-loading-overlay');
    if (loadingOverlay) {
        loadingOverlay.style.display = 'none';
    }
}

function triggerChartCreation() {

    showChartLoading();

    if (window.currentChart) {
        try {
            window.currentChart.destroy();
            window.currentChart = null;
        } catch (error) {

        }
    }

    let chartInstance = null;

    switch (CURRENT_PAGE) {
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
            toggleCheckbox(serverCheckbox, value);
        }
    });

    const updateGraphBtn = document.getElementById('updateGraph');
    if (updateGraphBtn) {
        const newBtn = updateGraphBtn.cloneNode(true);
        updateGraphBtn.parentNode.replaceChild(newBtn, updateGraphBtn);

        newBtn.addEventListener('click', () => {

            window.isRedirecting = true;


            if (CURRENT_PAGE === '1v1setupStatistics') {
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

function initializePageData() {
    switch (CURRENT_PAGE) {
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

function initializePageControls() {

    initializeInputsFromURL();

    setupEventListeners();

    initializePageData();

    window.requestAnimationFrame(() => {
        triggerChartCreation();
    });
}

document.addEventListener('DOMContentLoaded', () => {
    initializePageControls();
});

window.toggleCheckbox = toggleCheckbox;
window.toggleServer = toggleServer;
window.toggleCumulativeView = toggleCumulativeView;
window.populateSetupCheckboxes = populateSetupCheckboxes;
window.fetchServerCounts = fetchServerCounts;
window.fetchServerHourCounts = fetchServerHourCounts;
window.fetchTerritoryCounts = fetchTerritoryCounts;