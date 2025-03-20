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

window.preventAutoChartCreation = true;

window.isCumulativeView = window.isCumulativeView || false;
let currentChart = null;

const graphConfigs = {
    individualServers: {
        serverTypes: [
            { key: "new_player", name: "New Player Server", color: "#8884d8" },
            { key: "training", name: "Training Game", color: "#82ca9d" },
            { key: "defcon_random", name: "1v1 Random", color: "#ffc658" },
            { key: "defcon_best", name: "1v1 Best Setups", color: "#ff8042" },
            { key: "defcon_afas", name: "AF vs AS", color: "#0088fe" },
            { key: "defcon_eusa", name: "EU vs SA", color: "#00C49F" },
            { key: "defcon_default", name: "1v1 Default", color: "#FFBB28" },
            { key: "defcon_2v2", name: "2v2 Random", color: "#FF8042" },
            { key: "tournament_2v2", name: "2v2 Tournament", color: "#c71585" },
            { key: "defcon_2v2_special", name: "2v2 NA-SA-EU-AF", color: "#ba55d3" },
            { key: "mojo_2v2", name: "Mojo's 2v2", color: "#4b0082" },
            { key: "sony_hoov", name: "Sony & Hoov's", color: "#00ced1" },
            { key: "defcon_3v3", name: "3v3 Random", color: "#98fb98" },
            { key: "muricon", name: "MURICON", color: "#f0e68c" },
            { key: "cg_2v2_2815", name: "CG | 2v2 2.8.15", color: "#dda0dd" },
            { key: "cg_2v2_28141", name: "CG | 2v2 2.8.14.1", color: "#ff69b4" },
            { key: "cg_1v1_2815", name: "CG | 1v1 2.8.15", color: "#cd853f" },
            { key: "cg_1v1_28141", name: "CG | 1v1 2.8.14.1", color: "#8b4513" },
            { key: "defcon_ffa", name: "FFA Random", color: "#556b2f" },
            { key: "defcon_8p_diplo", name: "8P Diplomacy", color: "#483d8b" },
            { key: "defcon_4v4", name: "4v4 Random", color: "#008b8b" },
            { key: "defcon_10p_diplo", name: "10P Diplomacy", color: "#8b008b" }
        ],
        yAxisLabel: "Number of Games"
    },
    combinedServers: {
        serverTypes: [
            { key: "allServers", name: "All Servers", color: "#4a90e2" }
        ],
        yAxisLabel: "Total Number of Games"
    },
    totalHoursPlayed: {
        serverTypes: [
            { key: "totalHours", name: "Total Hours", color: "#82ca9d" }
        ],
        yAxisLabel: "Hours Played"
    },
    popularTerritories: {
        serverTypes: [
            { key: "na", name: "North America", color: "#FF4136" },
            { key: "sa", name: "South America", color: "#FF851B" },
            { key: "eu", name: "Europe", color: "#0074D9" },
            { key: "ru", name: "Russia", color: "#7FDBFF" },
            { key: "af", name: "Africa", color: "#B10DC9" },
            { key: "as", name: "Asia", color: "#01FF70" },
            { key: "au", name: "Australasia", color: "#FFDC00" },
            { key: "we", name: "West Asia", color: "#39CCCC" },
            { key: "ea", name: "East Asia", color: "#F012BE" },
            { key: "ant", name: "Antarctica", color: "#85144b" },
            { key: "naf", name: "North Africa", color: "#3D9970" },
            { key: "saf", name: "South Africa", color: "#2ECC40" }
        ],
        yAxisLabel: "Times Played"
    },
    "1v1setupStatistics": {
        setupTypes: {
            'Europe_vs_Russia': { name: 'Europe vs Russia', colors: ['#0074D9', '#7FDBFF'] },
            'Russia_vs_Asia': { name: 'Russia vs Asia', colors: ['#7FDBFF', '#01FF70'] },
            'Europe_vs_Africa': { name: 'Europe vs Africa', colors: ['#0074D9', '#B10DC9'] },
            'Africa_vs_Asia': { name: 'Africa vs Asia', colors: ['#B10DC9', '#01FF70'] },
            'Europe_vs_Asia': { name: 'Europe vs Asia', colors: ['#0074D9', '#01FF70'] },
            'Russia_vs_Africa': { name: 'Russia vs Africa', colors: ['#7FDBFF', '#B10DC9'] },
        },
        yAxisLabel: "Number of Wins",
        maxDisplayedSetups: 999
    }
};

class BaseGraph {
    constructor() {
        this.chartContainer = document.getElementById('gamesChart');
        this.width = this.chartContainer.offsetWidth;
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
        const url = new URL(window.location);

        const playerName = document.querySelector('.player-input')?.value || '';
        const startDate = document.getElementById('startDate')?.value;
        const endDate = document.getElementById('endDate')?.value;
        const yAxisMax = document.getElementById('yAxisMax')?.value;

        if (playerName) {
            url.searchParams.set('playerName', playerName);
        } else {
            url.searchParams.delete('playerName');
        }

        if (startDate) {
            url.searchParams.set('startDate', startDate);
        } else {
            url.searchParams.delete('startDate');
        }

        if (endDate) {
            url.searchParams.set('endDate', endDate);
        } else {
            url.searchParams.delete('endDate');
        }

        if (yAxisMax) {
            url.searchParams.set('yAxisMax', yAxisMax);
        } else {
            url.searchParams.delete('yAxisMax');
        }

        url.searchParams.set('cumulative', window.isCumulativeView);

        return url;
    }

    async fetchData(queryParams) {
        try {
            const response = await fetch(`/api/games-timeline?${queryParams}`);
            return await response.json();
        } catch (error) {
            console.error('Error fetching data:', error);
            return null;
        }
    }
}

async function fetchServerCounts() {
    try {
        const response = await fetch('/api/games-timeline?graphType=individualServers');
        const data = await response.json();

        if (!data || data.length === 0) return;

        const latestData = data[data.length - 1];

        const totalCounts = {};

        graphConfigs.individualServers.serverTypes.forEach(serverType => {
            totalCounts[serverType.key] = 0;
        });

        data.forEach(day => {
            graphConfigs.individualServers.serverTypes.forEach(serverType => {
                totalCounts[serverType.key] += day[serverType.key] || 0;
            });
        });

        for (const [serverKey, count] of Object.entries(totalCounts)) {
            const countElement = document.getElementById(`count-${serverKey}`);
            if (countElement) {
                countElement.textContent = `${count} games`;

                if (count > 500) {
                    countElement.style.color = '#5eff00';
                } else if (count > 100) {
                    countElement.style.color = '#ffd700';
                } else if (count === 0) {
                    countElement.style.color = '#ff6347';
                }
            }
        }
    } catch (error) {
        console.error('Error fetching server counts:', error);
    }
}

class IndividualServersGraph extends BaseGraph {
    constructor() {
        super();
        this.initializeServerControls();
        if (!window.preventAutoChartCreation) {
            this.createChart();
        }
    }

    initializeServerControls() {
        document.getElementById('selectAllServers')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = true);
            this.createChart();
        });

        document.getElementById('deselectAllServers')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = false);
            this.createChart();
        });

        document.getElementById('updateGraph').addEventListener('click', () => {
            fetchServerCounts();
        });
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedServers.length === 0) return Promise.resolve();

        const queryParams = new URLSearchParams({
            graphType: 'individualServers',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return Promise.resolve();

        data = data.filter(d => {
            const date = new Date(d.date);
            return date >= startDate && date <= endDate;
        });

        if (isCumulativeView) {
            const serverTotals = {};
            data = data.map(dayData => {
                const newDayData = { date: dayData.date };
                selectedServers.forEach(server => {
                    serverTotals[server] = (serverTotals[server] || 0) + (dayData[server] || 0);
                    newDayData[server] = serverTotals[server];
                });
                return newDayData;
            });
        }

        const series = selectedServers.map(server => {
            const serverConfig = graphConfigs.individualServers.serverTypes.find(type => type.key === server);
            return {
                name: serverConfig.name,
                data: data.map(item => ({
                    x: new Date(item.date).getTime(),
                    y: item[server] || 0
                })),
                color: serverConfig.color
            };
        });

        const options = {
            series: series,
            chart: {
                height: 500,
                type: 'line',
                animations: {
                    enabled: false
                },
                fontFamily: 'inherit',
                foreColor: '#b8b8b8',
                background: 'transparent',
                toolbar: {
                    show: true,
                    tools: {
                        download: true,
                        selection: true,
                        zoom: true,
                        zoomin: true,
                        zoomout: true,
                        pan: true,
                        reset: true
                    }
                }
            },
            dataLabels: {
                enabled: false
            },
            stroke: {
                curve: 'smooth',
                width: 2
            },
            title: {
                text: 'Server Games Over Time',
                align: 'left',
                style: {
                    color: '#4da6ff',
                    fontFamily: 'Orbitron, sans-serif',
                    fontSize: '18px'
                }
            },
            grid: {
                borderColor: 'rgba(255, 255, 255, 0.1)',
                row: {
                    colors: ['transparent', 'transparent'],
                    opacity: 0.5
                }
            },
            xaxis: {
                type: 'datetime',
                labels: {
                    datetimeUTC: false,
                    style: {
                        colors: '#b8b8b8'
                    }
                },
                axisBorder: {
                    color: 'rgba(255, 255, 255, 0.3)'
                },
                axisTicks: {
                    color: 'rgba(255, 255, 255, 0.3)'
                }
            },
            yaxis: {
                title: {
                    text: graphConfigs.individualServers.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: isCumulativeView ? undefined : yAxisMax,
                labels: {
                    style: {
                        colors: '#b8b8b8'
                    }
                }
            },
            tooltip: {
                x: {
                    format: 'dd MMM yyyy'
                },
                theme: 'dark'
            },
            legend: {
                position: 'bottom',
                horizontalAlign: 'center',
                offsetY: 10,
                labels: {
                    colors: '#b8b8b8'
                },
                onItemHover: {
                    highlightDataSeries: true
                },
                onItemClick: {
                    toggleDataSeries: false
                }
            }
        };

        return new Promise((resolve, reject) => {
            try {
                if (currentChart) {
                    currentChart.destroy();
                }

                currentChart = new ApexCharts(this.chartContainer, options);
                currentChart.render().then(() => {
                    resolve();
                }).catch(err => {
                    reject(err);
                });
            } catch (error) {
                console.error('Error creating chart:', error);
                reject(error);
            }
        });
    }
}

class CombinedServersGraph extends BaseGraph {
    constructor() {
        super();
        if (!window.preventAutoChartCreation) {
            this.createChart();
        }
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const queryParams = new URLSearchParams({
            graphType: 'combinedServers',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return Promise.resolve();

        data = data.filter(d => {
            const date = new Date(d.date);
            return date >= startDate && date <= endDate;
        });

        if (isCumulativeView) {
            let total = 0;
            data = data.map(d => ({
                date: d.date,
                allServers: (total += d.allServers)
            }));
        }

        const series = [{
            name: 'All Servers',
            data: data.map(item => ({
                x: new Date(item.date).getTime(),
                y: item.allServers
            })),
            color: graphConfigs.combinedServers.serverTypes[0].color
        }];

        const options = {
            series: series,
            chart: {
                height: 500,
                type: 'area',
                animations: {
                    enabled: false
                },
                fontFamily: 'inherit',
                foreColor: '#b8b8b8',
                background: 'transparent',
                toolbar: {
                    show: true
                }
            },
            dataLabels: {
                enabled: false
            },
            stroke: {
                curve: 'smooth',
                width: 2
            },
            fill: {
                type: 'gradient',
                gradient: {
                    shadeIntensity: 1,
                    opacityFrom: 0.4,
                    opacityTo: 0.1,
                    stops: [0, 100]
                }
            },
            title: {
                text: 'Total Games Across All Servers',
                align: 'left',
                style: {
                    color: '#4da6ff',
                    fontFamily: 'Orbitron, sans-serif',
                    fontSize: '18px'
                }
            },
            grid: {
                borderColor: 'rgba(255, 255, 255, 0.1)',
                row: {
                    colors: ['transparent', 'transparent'],
                    opacity: 0.5
                }
            },
            xaxis: {
                type: 'datetime',
                labels: {
                    datetimeUTC: false,
                    style: {
                        colors: '#b8b8b8'
                    }
                },
                axisBorder: {
                    color: 'rgba(255, 255, 255, 0.3)'
                },
                axisTicks: {
                    color: 'rgba(255, 255, 255, 0.3)'
                }
            },
            yaxis: {
                title: {
                    text: graphConfigs.combinedServers.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: isCumulativeView ? undefined : yAxisMax,
                labels: {
                    style: {
                        colors: '#b8b8b8'
                    }
                }
            },
            tooltip: {
                x: {
                    format: 'dd MMM yyyy'
                },
                theme: 'dark'
            }
        };

        return new Promise((resolve, reject) => {
            try {
                if (currentChart) {
                    currentChart.destroy();
                }

                currentChart = new ApexCharts(this.chartContainer, options);
                currentChart.render().then(() => {
                    resolve();
                }).catch(err => {
                    reject(err);
                });
            } catch (error) {
                console.error('Error creating chart:', error);
                reject(error);
            }
        });
    }
}

class TotalHoursGraph extends BaseGraph {
    constructor() {
        super();
        this.initializeServerControls();
        if (!window.preventAutoChartCreation) {
            this.createChart();
        }
    }

    initializeServerControls() {
        document.getElementById('selectAllServers')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = true);
            document.querySelectorAll('.server-checkbox').forEach(box => box.classList.add('selected'));
            this.createChart();
        });

        document.getElementById('deselectAllServers')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = false);
            document.querySelectorAll('.server-checkbox').forEach(box => box.classList.remove('selected'));
            this.createChart();
        });

        if (typeof fetchServerHourCounts === 'function') {
            fetchServerHourCounts();
        }
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMaxInput = parseInt(document.getElementById('yAxisMax').value) || undefined;
        const playerName = this.getPlayerName();

        const selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedServers.length === 0) return Promise.resolve();

        const queryParams = new URLSearchParams({
            graphType: 'totalHoursPlayed',
            byServer: 'true',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return Promise.resolve();

        data = data.filter(d => {
            const date = new Date(d.date);
            return date >= startDate && date <= endDate;
        });

        let series;

        if (selectedServers.length === 1 && selectedServers[0] === 'allServers') {
            if (isCumulativeView) {
                let total = 0;
                data = data.map(d => ({
                    date: d.date,
                    totalHours: (total += d.totalHours)
                }));
            }

            series = [{
                name: 'Total Hours',
                data: data.map(item => ({
                    x: new Date(item.date).getTime(),
                    y: item.totalHours
                })),
                color: graphConfigs.totalHoursPlayed.serverTypes[0].color
            }];
        } else {
            if (isCumulativeView) {
                const serverTotals = {};
                data = data.map(dayData => {
                    const newDayData = { date: dayData.date };
                    selectedServers.forEach(server => {
                        serverTotals[server] = (serverTotals[server] || 0) + (dayData[server] || 0);
                        newDayData[server] = serverTotals[server];
                    });
                    return newDayData;
                });
            }

            series = selectedServers.map(server => {
                const serverConfig = graphConfigs.individualServers.serverTypes.find(type => type.key === server);
                return {
                    name: serverConfig ? serverConfig.name : server,
                    data: data.map(item => ({
                        x: new Date(item.date).getTime(),
                        y: item[server] || 0
                    })),
                    color: serverConfig ? serverConfig.color : '#' + Math.floor(Math.random() * 16777215).toString(16)
                };
            });
        }

        let calculatedYAxisMax = undefined;
        if (isCumulativeView) {
            const allValues = [];
            series.forEach(s => {
                s.data.forEach(point => {
                    if (typeof point.y === 'number') {
                        allValues.push(point.y);
                    }
                });
            });

            if (allValues.length > 0) {
                const maxValue = Math.max(...allValues);
                calculatedYAxisMax = Math.ceil(maxValue * 1.1);
            }
        }

        const options = {
            series: series,
            chart: {
                height: 500,
                type: 'area',
                animations: {
                    enabled: false
                },
                fontFamily: 'inherit',
                foreColor: '#b8b8b8',
                background: 'transparent',
                toolbar: {
                    show: true
                }
            },
            dataLabels: {
                enabled: false
            },
            stroke: {
                curve: 'smooth',
                width: 2
            },
            fill: {
                type: 'gradient',
                gradient: {
                    shadeIntensity: 1,
                    opacityFrom: 0.4,
                    opacityTo: 0.1,
                    stops: [0, 100]
                }
            },
            title: {
                text: isCumulativeView ? 'Cumulative Hours Played by Server' : 'Hours Played by Server',
                align: 'left',
                style: {
                    color: '#4da6ff',
                    fontFamily: 'Orbitron, sans-serif',
                    fontSize: '18px'
                }
            },
            grid: {
                borderColor: 'rgba(255, 255, 255, 0.1)',
                row: {
                    colors: ['transparent', 'transparent'],
                    opacity: 0.5
                }
            },
            xaxis: {
                type: 'datetime',
                labels: {
                    datetimeUTC: false,
                    style: {
                        colors: '#b8b8b8'
                    }
                },
                axisBorder: {
                    color: 'rgba(255, 255, 255, 0.3)'
                },
                axisTicks: {
                    color: 'rgba(255, 255, 255, 0.3)'
                }
            },
            yaxis: {
                title: {
                    text: graphConfigs.totalHoursPlayed.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: isCumulativeView ? calculatedYAxisMax : yAxisMaxInput,
                labels: {
                    formatter: function (val) {
                        if (isCumulativeView && val > 100) {
                            return Math.round(val);
                        }
                        return val.toFixed(1);
                    },
                    style: {
                        colors: '#b8b8b8'
                    }
                }
            },
            tooltip: {
                x: {
                    format: 'dd MMM yyyy'
                },
                y: {
                    formatter: function (val) {
                        return val.toFixed(2) + ' hours';
                    }
                },
                theme: 'dark'
            },
            legend: {
                position: 'bottom',
                horizontalAlign: 'center',
                offsetY: 10,
                labels: {
                    colors: '#b8b8b8'
                },
                onItemHover: {
                    highlightDataSeries: true
                }
            }
        };

        return new Promise((resolve, reject) => {
            try {
                if (currentChart) {
                    currentChart.destroy();
                }

                currentChart = new ApexCharts(this.chartContainer, options);
                currentChart.render().then(() => {
                    resolve();
                }).catch(err => {
                    reject(err);
                });
            } catch (error) {
                console.error('Error creating chart:', error);
                reject(error);
            }
        });
    }
}

class PopularTerritoriesGraph extends BaseGraph {
    constructor() {
        super();
        this.initializeTerritoryControls();
        if (!window.preventAutoChartCreation) {
            this.createChart();
        }
    }

    initializeTerritoryControls() {
        document.getElementById('selectAllTerritories')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="territory"]').forEach(checkbox => checkbox.checked = true);
            document.querySelectorAll('.server-checkbox').forEach(box => box.classList.add('selected'));
            this.createChart();
        });

        document.getElementById('deselectAllTerritories')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="territory"]').forEach(checkbox => checkbox.checked = false);
            document.querySelectorAll('.server-checkbox').forEach(box => box.classList.remove('selected'));
            this.createChart();
        });

        if (typeof fetchTerritoryCounts === 'function') {
            fetchTerritoryCounts();
        }
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMaxInput = parseInt(document.getElementById('yAxisMax').value) || undefined;
        const playerName = this.getPlayerName();

        const selectedTerritories = Array.from(document.querySelectorAll('input[name="territory"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedTerritories.length === 0) return Promise.resolve();

        const queryParams = new URLSearchParams({
            graphType: 'popularTerritories',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return Promise.resolve();

        data = data.filter(d => {
            const date = new Date(d.date);
            return date >= startDate && date <= endDate;
        });

        if (isCumulativeView) {
            const territoryTotals = {};
            data = data.map(dayData => {
                const newDayData = { date: dayData.date };
                selectedTerritories.forEach(territory => {
                    territoryTotals[territory] = (territoryTotals[territory] || 0) + (dayData[territory] || 0);
                    newDayData[territory] = territoryTotals[territory];
                });
                return newDayData;
            });
        }

        const series = selectedTerritories.map(territory => {
            const territoryConfig = graphConfigs.popularTerritories.serverTypes.find(type => type.key === territory);
            return {
                name: territoryConfig.name,
                data: data.map(item => ({
                    x: new Date(item.date).getTime(),
                    y: item[territory] || 0
                })),
                color: territoryConfig.color
            };
        });

        let calculatedYAxisMax = undefined;
        if (isCumulativeView) {
            const allValues = [];
            series.forEach(s => {
                s.data.forEach(point => {
                    if (typeof point.y === 'number') {
                        allValues.push(point.y);
                    }
                });
            });

            if (allValues.length > 0) {
                const maxValue = Math.max(...allValues);
                calculatedYAxisMax = Math.ceil(maxValue * 1.1);
            }
        }

        const options = {
            series: series,
            chart: {
                height: 500,
                type: 'line',
                animations: {
                    enabled: false
                },
                fontFamily: 'inherit',
                foreColor: '#b8b8b8',
                background: 'transparent',
                toolbar: {
                    show: true
                }
            },
            dataLabels: {
                enabled: false
            },
            stroke: {
                curve: 'smooth',
                width: 2
            },
            title: {
                text: isCumulativeView ? 'Cumulative Territory Popularity' : 'Territory Popularity',
                align: 'left',
                style: {
                    color: '#4da6ff',
                    fontFamily: 'Orbitron, sans-serif',
                    fontSize: '18px'
                }
            },
            grid: {
                borderColor: 'rgba(255, 255, 255, 0.1)',
                row: {
                    colors: ['transparent', 'transparent'],
                    opacity: 0.5
                }
            },
            xaxis: {
                type: 'datetime',
                labels: {
                    datetimeUTC: false,
                    style: {
                        colors: '#b8b8b8'
                    }
                },
                axisBorder: {
                    color: 'rgba(255, 255, 255, 0.3)'
                },
                axisTicks: {
                    color: 'rgba(255, 255, 255, 0.3)'
                }
            },
            yaxis: {
                title: {
                    text: graphConfigs.popularTerritories.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: isCumulativeView ? calculatedYAxisMax : yAxisMaxInput,
                labels: {
                    formatter: function (val) {
                        if (isCumulativeView && val > 100) {
                            return Math.round(val);
                        }
                        return val.toFixed(0);
                    },
                    style: {
                        colors: '#b8b8b8'
                    }
                }
            },
            tooltip: {
                x: {
                    format: 'dd MMM yyyy'
                },
                theme: 'dark'
            },
            legend: {
                position: 'bottom',
                horizontalAlign: 'center',
                offsetY: 10,
                labels: {
                    colors: '#b8b8b8'
                },
                onItemHover: {
                    highlightDataSeries: true
                }
            }
        };

        return new Promise((resolve, reject) => {
            try {
                if (currentChart) {
                    currentChart.destroy();
                }

                currentChart = new ApexCharts(this.chartContainer, options);
                currentChart.render().then(() => {
                    resolve();
                }).catch(err => {
                    reject(err);
                });
            } catch (error) {
                console.error('Error creating chart:', error);
                reject(error);
            }
        });
    }
}

class SetupStatisticsGraph extends BaseGraph {
    constructor() {
        super();
        this.territoryAbbreviations = {
            'North America': 'NA',
            'South America': 'SA',
            'Europe': 'EU',
            'Russia': 'RU',
            'Africa': 'AF',
            'Asia': 'AS',
            'East Asia': 'EA',
            'Australasia': 'AU',
            'West Asia': 'WA',
            'Antarctica': 'AN',
            'North Africa': 'NAF',
            'South Africa': 'SAF'
        };
        this.setupData = null;
        this.initializeSetupControls();
        if (!window.preventAutoChartCreation) {
            this.createChart();
        }
        window.setupStatsGraph = this;
    }

    formatSetupLabel(setup) {
        return setup.split('_vs_')
            .map(territory => this.territoryAbbreviations[territory] || territory)
            .join(' vs ');
    }

    initializeSetupControls() {
        document.getElementById('selectAllSetups')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="setup"]').forEach(checkbox => {
                checkbox.checked = true;
                checkbox.closest('.server-checkbox')?.classList.add('selected');
            });
            this.updateChart();
        });

        document.getElementById('deselectAllSetups')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="setup"]').forEach(checkbox => {
                checkbox.checked = false;
                checkbox.closest('.server-checkbox')?.classList.remove('selected');
            });
            this.updateChart();
        });

        document.getElementById('updateGraph')?.addEventListener('click', () => {
            this.updateChart();
        });
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const queryParams = new URLSearchParams({
            graphType: '1v1setupStatistics',
            playerName,
            startDate: startDate.toISOString().split('T')[0],
            endDate: endDate.toISOString().split('T')[0]
        });

        if (!this.setupData) {
            const data = await this.fetchData(queryParams);
            if (!data) return Promise.resolve();
            this.setupData = data;

            if (typeof populateSetupCheckboxes === 'function') {
                populateSetupCheckboxes(this.setupData);
            }
        }

        return this.updateChart();
    }

    async updateChart() {

        if (window.isRedirecting) {
            return Promise.resolve();
        }

        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);

        const selectedSetups = Array.from(document.querySelectorAll('input[name="setup"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedSetups.length === 0 || !this.setupData) return Promise.resolve();

        const filteredData = this.setupData.filter(setup => {
            const setupDate = new Date(setup.date);
            return setupDate >= startDate && setupDate <= endDate;
        }).filter(setup => selectedSetups.includes(setup.setup));

        return this.renderChart(filteredData, yAxisMax);
    }

    renderChart(data, yAxisMax) {
        const categories = [];
        const series1 = [];
        const series2 = [];
        const territory1Names = [];
        const territory2Names = [];

        data.forEach(setup => {
            const setupName = this.formatSetupLabel(setup.setup);
            categories.push(setupName);

            const [t1, t2] = setup.territories;
            const t1Wins = setup[t1] || 0;
            const t2Wins = setup[t2] || 0;

            series1.push(t1Wins);
            series2.push(t2Wins);
            territory1Names.push(t1);
            territory2Names.push(t2);
        });

        const options = {
            series: [
                {
                    name: 'Territory 1',
                    data: series1
                },
                {
                    name: 'Territory 2',
                    data: series2
                }
            ],
            chart: {
                height: 500,
                type: 'bar',
                animations: {
                    enabled: false
                },
                fontFamily: 'inherit',
                foreColor: '#b8b8b8',
                background: 'transparent',
                toolbar: {
                    show: true
                }
            },
            plotOptions: {
                bar: {
                    horizontal: false,
                    columnWidth: '80%',
                    borderRadius: 2
                }
            },
            colors: ['#4CAF50', '#FF4444'],
            dataLabels: {
                enabled: false
            },
            title: {
                text: '1v1 Setup Statistics',
                align: 'left',
                style: {
                    color: '#4da6ff',
                    fontFamily: 'Orbitron, sans-serif',
                    fontSize: '18px'
                }
            },
            grid: {
                borderColor: 'rgba(255, 255, 255, 0.1)',
                row: {
                    colors: ['transparent', 'transparent'],
                    opacity: 0.5
                }
            },
            xaxis: {
                categories: categories,
                labels: {
                    style: {
                        colors: '#b8b8b8',
                        fontSize: '12px'
                    },
                    rotate: -45,
                    rotateAlways: true
                },
                axisBorder: {
                    color: 'rgba(255, 255, 255, 0.3)'
                },
                axisTicks: {
                    color: 'rgba(255, 255, 255, 0.3)'
                }
            },
            yaxis: {
                title: {
                    text: graphConfigs['1v1setupStatistics'].yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: yAxisMax,
                labels: {
                    style: {
                        colors: '#b8b8b8'
                    }
                }
            },
            tooltip: {
                theme: 'dark',
                shared: false,
                intersect: true,
                custom: function ({ seriesIndex, dataPointIndex, w }) {
                    const territory = seriesIndex === 0 ?
                        territory1Names[dataPointIndex] :
                        territory2Names[dataPointIndex];

                    const wins = w.config.series[seriesIndex].data[dataPointIndex];
                    const setupData = data[dataPointIndex];
                    const totalGames = setupData.total_games;
                    const winRate = ((wins / totalGames) * 100).toFixed(1);

                    return `
                        <div class="apexcharts-tooltip-box">
                            <div class="apexcharts-tooltip-title" style="font-weight:bold;">${territory}</div>
                            <div>Wins: ${wins}</div>
                            <div>Win Rate: ${winRate}%</div>
                            <div>Total Games: ${totalGames}</div>
                        </div>
                    `;
                }
            },
            legend: {
                show: false
            }
        };

        return new Promise((resolve, reject) => {
            try {
                if (currentChart) {
                    currentChart.destroy();
                }

                currentChart = new ApexCharts(this.chartContainer, options);
                currentChart.render().then(() => {
                    resolve();
                }).catch(err => {
                    reject(err);
                });
            } catch (error) {
                console.error('Error creating chart:', error);
                reject(error);
            }
        });
    }
}

document.addEventListener('DOMContentLoaded', () => {
    const graphSelect = document.getElementById('graph-filter');
    if (graphSelect) {
        graphSelect.addEventListener('change', (e) => {
            const urlMap = {
                'individualServers': '/about',
                'combinedServers': '/about/combined-servers',
                'totalHoursPlayed': '/about/hours-played',
                'popularTerritories': '/about/popular-territories',
                '1v1setupStatistics': '/about/1v1-setup-statistics'
            };

            const newUrl = urlMap[e.target.value];
            if (newUrl) {
                window.location.href = newUrl;
            }
        });
    }
});

window.addEventListener('resize', _.debounce(() => {
    if (!window.currentChart) return;

    const pathname = window.location.pathname;
    switch (pathname) {
        case '/about':
            new IndividualServersGraph().createChart();
            break;
        case '/about/combined-servers':
            new CombinedServersGraph().createChart();
            break;
        case '/about/hours-played':
            new TotalHoursGraph().createChart();
            break;
        case '/about/popular-territories':
            new PopularTerritoriesGraph().createChart();
            break;
        case '/about/1v1-setup-statistics':
            if (window.setupStatsGraph) {
                window.setupStatsGraph.updateChart();
            } else {
                new SetupStatisticsGraph().createChart();
            }
            break;
    }
}, 250));