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

import { BaseGraph } from './basegraph.js';
import { GRAPH_CONFIGS } from './constants.js';
import * as ui from './ui.js';

/**
 * Individual Servers Graph implementation
 */
export class IndividualServersGraph extends BaseGraph {
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

        document.getElementById('updateGraph')?.addEventListener('click', () => {
            window.fetchServerCounts();
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

        if (window.isCumulativeView) {
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
            const serverConfig = GRAPH_CONFIGS.individualServers.serverTypes.find(type => type.key === server);
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
                    text: GRAPH_CONFIGS.individualServers.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: window.isCumulativeView ? undefined : yAxisMax,
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
                if (window.currentChart) {
                    window.currentChart.destroy();
                }

                window.currentChart = new ApexCharts(this.chartContainer, options);
                window.currentChart.render().then(() => {
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

/**
 * Combined Servers Graph implementation
 */
export class CombinedServersGraph extends BaseGraph {
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

        if (window.isCumulativeView) {
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
            color: GRAPH_CONFIGS.combinedServers.serverTypes[0].color
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
                    text: GRAPH_CONFIGS.combinedServers.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: window.isCumulativeView ? undefined : yAxisMax,
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
                if (window.currentChart) {
                    window.currentChart.destroy();
                }

                window.currentChart = new ApexCharts(this.chartContainer, options);
                window.currentChart.render().then(() => {
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

/**
 * Total Hours Graph implementation
 */
export class TotalHoursGraph extends BaseGraph {
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

        if (typeof window.fetchServerHourCounts === 'function') {
            window.fetchServerHourCounts();
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
            if (window.isCumulativeView) {
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
                color: GRAPH_CONFIGS.totalHoursPlayed.serverTypes[0].color
            }];
        } else {
            if (window.isCumulativeView) {
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
                const serverConfig = GRAPH_CONFIGS.individualServers.serverTypes.find(type => type.key === server);
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
        if (window.isCumulativeView) {
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
                text: window.isCumulativeView ? 'Cumulative Hours Played by Server' : 'Hours Played by Server',
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
                    text: GRAPH_CONFIGS.totalHoursPlayed.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: window.isCumulativeView ? calculatedYAxisMax : yAxisMaxInput,
                labels: {
                    formatter: function (val) {
                        if (window.isCumulativeView && val > 100) {
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
                if (window.currentChart) {
                    window.currentChart.destroy();
                }

                window.currentChart = new ApexCharts(this.chartContainer, options);
                window.currentChart.render().then(() => {
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

/**
 * Popular Territories Graph implementation
 */
export class PopularTerritoriesGraph extends BaseGraph {
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

        if (typeof window.fetchTerritoryCounts === 'function') {
            window.fetchTerritoryCounts();
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

        if (window.isCumulativeView) {
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
            const territoryConfig = GRAPH_CONFIGS.popularTerritories.serverTypes.find(type => type.key === territory);
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
        if (window.isCumulativeView) {
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
                text: window.isCumulativeView ? 'Cumulative Territory Popularity' : 'Territory Popularity',
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
                    text: GRAPH_CONFIGS.popularTerritories.yAxisLabel,
                    style: {
                        color: '#b8b8b8'
                    }
                },
                max: window.isCumulativeView ? calculatedYAxisMax : yAxisMaxInput,
                labels: {
                    formatter: function (val) {
                        if (window.isCumulativeView && val > 100) {
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
                if (window.currentChart) {
                    window.currentChart.destroy();
                }

                window.currentChart = new ApexCharts(this.chartContainer, options);
                window.currentChart.render().then(() => {
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

/**
 * Setup Statistics Graph implementation
 */
export class SetupStatisticsGraph extends BaseGraph {
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

            if (typeof window.populateSetupCheckboxes === 'function') {
                window.populateSetupCheckboxes(this.setupData);
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
                    text: GRAPH_CONFIGS['1v1setupStatistics'].yAxisLabel,
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
                if (window.currentChart) {
                    window.currentChart.destroy();
                }

                window.currentChart = new ApexCharts(this.chartContainer, options);
                window.currentChart.render().then(() => {
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