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
//Last Edited 03-01-2025

let isCumulativeView = false;

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
        this.margin = { top: 5, right: 30, bottom: 55, left: 50 };
        this.chartContainer = document.getElementById('gamesChart');
        this.width = this.chartContainer.offsetWidth - this.margin.left - this.margin.right;
        this.height = 350;

        this.initializeBaseControls();
        this.initializeEventListeners();
    }

    getPlayerName() {
        return document.querySelector('.player-input')?.value || '';
    }

    initializeBaseControls() {
        const endDate = new Date();
        const startDate = new Date();
        startDate.setMonth(startDate.getMonth() - 6);

        document.getElementById('startDate').valueAsDate = startDate;
        document.getElementById('endDate').valueAsDate = endDate;
    }

    initializeEventListeners() {
        window.addEventListener('resize', _.debounce(() => this.createChart(), 250));

        const updateButton = document.getElementById('updateGraph');
        if (updateButton) {
            updateButton.addEventListener('click', () => this.createChart());
        }

        const exportButton = document.getElementById('exportData');
        if (exportButton) {
            exportButton.addEventListener('click', () => this.exportData());
        }

        const toggleButton = document.getElementById('toggleView');
        if (toggleButton) {
            toggleButton.addEventListener('click', () => {
                isCumulativeView = !isCumulativeView;
                toggleButton.textContent = isCumulativeView ? 'Show Individual View' : 'Show Linear View';
                this.createChart();
            });
        }
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

    async exportData() {
        const graphTypes = ['individualServers', 'combinedServers', 'totalHoursPlayed', 'popularTerritories', '1v1setupStatistics'];
        const data = {};

        for (const graphType of graphTypes) {
            const queryParams = new URLSearchParams({ graphType });
            const response = await this.fetchData(queryParams);
            data[graphType] = response;
        }

        let csvContent = '';

        if (data.individualServers?.length > 0) {
            csvContent += 'Individual Servers Data\n';
            csvContent += 'Date,' + Object.keys(data.individualServers[0])
                .filter(key => key !== 'date')
                .join(',') + '\n';

            data.individualServers.forEach(row => {
                csvContent += `${new Date(row.date).toISOString().split('T')[0]},`;
                csvContent += Object.keys(row)
                    .filter(key => key !== 'date')
                    .map(key => row[key] || '0')
                    .join(',') + '\n';
            });
            csvContent += '\n\n';
        }

        if (data.combinedServers?.length > 0) {
            csvContent += 'Combined Servers Data\n';
            csvContent += 'Date,Total Games\n';
            data.combinedServers.forEach(row => {
                csvContent += `${new Date(row.date).toISOString().split('T')[0]},${row.allServers}\n`;
            });
            csvContent += '\n\n';
        }

        if (data.totalHoursPlayed?.length > 0) {
            csvContent += 'Total Hours Played Data\n';
            csvContent += 'Date,Hours\n';
            data.totalHoursPlayed.forEach(row => {
                csvContent += `${new Date(row.date).toISOString().split('T')[0]},${row.totalHours.toFixed(2)}\n`;
            });
            csvContent += '\n\n';
        }

        if (data.popularTerritories?.length > 0) {
            csvContent += 'Territory Popularity Data\n';
            csvContent += 'Date,' + Object.keys(data.popularTerritories[0])
                .filter(key => key !== 'date')
                .join(',') + '\n';

            data.popularTerritories.forEach(row => {
                csvContent += `${new Date(row.date).toISOString().split('T')[0]},`;
                csvContent += Object.keys(row)
                    .filter(key => key !== 'date')
                    .map(key => row[key] || '0')
                    .join(',') + '\n';
            });
            csvContent += '\n\n';
        }

        if (data['1v1setupStatistics']?.length > 0) {
            csvContent += '1v1 Setup Statistics\n';
            csvContent += 'Setup,Total Games,Territory 1,Territory 2,Territory 1 Wins,Territory 2 Wins\n';
            data['1v1setupStatistics'].forEach(row => {
                const [t1, t2] = row.territories;
                csvContent += `${row.setup},${row.total_games},${t1},${t2},${row[t1]},${row[t2]}\n`;
            });
        }

        const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
        const link = document.createElement('a');
        const url = URL.createObjectURL(blob);
        link.setAttribute('href', url);
        link.setAttribute('download', `DefconExpanded_Statistics${new Date().toISOString().split('T')[0]}.csv`);
        link.style.visibility = 'hidden';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }
}

class IndividualServersGraph extends BaseGraph {
    constructor() {
        super();
        this.initializeServerControls();
        this.createChart();
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
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedServers.length === 0) return;

        const queryParams = new URLSearchParams({
            graphType: 'individualServers',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return;

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

        const svg = d3.select("#gamesChart svg").remove();
        const newSvg = d3.select("#gamesChart")
            .append("svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${this.height + this.margin.top + this.margin.bottom}`)
            .append("g")
            .attr("transform", `translate(${this.margin.left},${this.margin.top})`);

        const x = d3.scaleTime()
            .domain(d3.extent(data, d => new Date(d.date)))
            .range([0, this.width]);

        const y = d3.scaleLinear()
            .domain([0, isCumulativeView ?
                d3.max(data, d => d3.max(selectedServers, server => d[server] || 0)) :
                yAxisMax
            ])
            .range([this.height, 0]);

        newSvg.append("g")
            .attr("class", "grid")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x)
                .tickSize(-this.height)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        newSvg.append("g")
            .attr("class", "grid")
            .call(d3.axisLeft(y)
                .tickSize(-this.width)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        newSvg.append("g")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x))
            .style("color", "#b8b8b8");

        newSvg.append("g")
            .call(d3.axisLeft(y)
                .ticks(10)
                .tickFormat(d3.format("d")))
            .style("color", "#b8b8b8");

        newSvg.append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", -this.margin.left)
            .attr("x", -this.height / 2)
            .attr("dy", "1.5em")
            .style("text-anchor", "middle")
            .style("fill", "#b8b8b8")
            .text(graphConfigs.individualServers.yAxisLabel);

        selectedServers.forEach(server => {
            const line = d3.line()
                .x(d => x(new Date(d.date)))
                .y(d => y(d[server] || 0))
                .curve(d3.curveMonotoneX);

            const serverConfig = graphConfigs.individualServers.serverTypes
                .find(type => type.key === server);

            newSvg.append("path")
                .datum(data)
                .attr("fill", "none")
                .attr("stroke", serverConfig.color)
                .attr("stroke-width", 1.5)
                .attr("d", line)
                .style("opacity", 0.7)
                .on("mouseover", function () {
                    d3.select(this)
                        .style("opacity", 1)
                        .style("stroke-width", 2.5);
                })
                .on("mouseout", function () {
                    d3.select(this)
                        .style("opacity", 0.7)
                        .style("stroke-width", 1.5);
                });
        });

        this.addLegend(newSvg, selectedServers);
    }

    addLegend(svg, selectedServers) {
        const selectedServerTypes = graphConfigs.individualServers.serverTypes
            .filter(type => selectedServers.includes(type.key));

        const legend = svg.append("g")
            .attr("class", "legend")
            .attr("transform", `translate(0,${this.height + 40})`);

        const desiredColumns = 5;
        const legendItemWidth = this.width / desiredColumns;
        const legendItemHeight = 20;
        const itemsPerRow = desiredColumns;

        selectedServerTypes.forEach((serverType, i) => {
            const row = Math.floor(i / itemsPerRow);
            const col = i % itemsPerRow;

            const legendItem = legend.append("g")
                .attr("transform", `translate(${col * legendItemWidth},${row * legendItemHeight})`);

            legendItem.append("rect")
                .attr("width", 15)
                .attr("height", 15)
                .attr("fill", serverType.color)
                .style("opacity", 0.7);

            legendItem.append("text")
                .attr("x", 24)
                .attr("y", 12)
                .text(serverType.name)
                .style("fill", "#b8b8b8")
                .style("font-size", "12px");

            legendItem
                .style("cursor", "pointer")
                .on("mouseover", function () {
                    svg.selectAll("path")
                        .style("opacity", 0.1);
                    svg.selectAll("path")
                        .filter(function () {
                            return d3.select(this).attr("stroke") === serverType.color;
                        })
                        .style("opacity", 1)
                        .style("stroke-width", 2.5);

                    d3.select(this).select("rect")
                        .style("opacity", 1);
                })
                .on("mouseout", function () {
                    svg.selectAll("path")
                        .style("opacity", 0.7)
                        .style("stroke-width", 1.5);

                    d3.select(this).select("rect")
                        .style("opacity", 0.7);
                });
        });

        const legendRows = Math.ceil(selectedServerTypes.length / itemsPerRow);
        const totalHeight = this.height + this.margin.top + this.margin.bottom + (legendRows * legendItemHeight);
        d3.select("#gamesChart svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${totalHeight}`);
    }
}

class CombinedServersGraph extends BaseGraph {
    constructor() {
        super();
        this.createChart();
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
        if (!data) return;

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

        const svg = d3.select("#gamesChart svg").remove();
        const newSvg = d3.select("#gamesChart")
            .append("svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${this.height + this.margin.top + this.margin.bottom}`)
            .append("g")
            .attr("transform", `translate(${this.margin.left},${this.margin.top})`);

        const x = d3.scaleTime()
            .domain(d3.extent(data, d => new Date(d.date)))
            .range([0, this.width]);

        const y = d3.scaleLinear()
            .domain([0, isCumulativeView ?
                d3.max(data, d => d.allServers) :
                yAxisMax
            ])
            .range([this.height, 0]);

        this.addGridAndAxes(newSvg, x, y);

        const line = d3.line()
            .x(d => x(new Date(d.date)))
            .y(d => y(d.allServers))
            .curve(d3.curveMonotoneX);

        newSvg.append("path")
            .datum(data)
            .attr("fill", "none")
            .attr("stroke", graphConfigs.combinedServers.serverTypes[0].color)
            .attr("stroke-width", 2)
            .attr("d", line)
            .style("opacity", 0.7);
    }

    addGridAndAxes(svg, x, y) {
        svg.append("g")
            .attr("class", "grid")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x)
                .tickSize(-this.height)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("class", "grid")
            .call(d3.axisLeft(y)
                .tickSize(-this.width)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x))
            .style("color", "#b8b8b8");

        svg.append("g")
            .call(d3.axisLeft(y)
                .ticks(10)
                .tickFormat(d3.format("d")))
            .style("color", "#b8b8b8");

        svg.append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", -this.margin.left)
            .attr("x", -this.height / 2)
            .attr("dy", "0.8em")
            .style("text-anchor", "middle")
            .style("fill", "#b8b8b8")
            .text(graphConfigs.combinedServers.yAxisLabel);
    }
}

class TotalHoursGraph extends BaseGraph {
    constructor() {
        super();
        this.createChart();
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const queryParams = new URLSearchParams({
            graphType: 'totalHoursPlayed',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return;

        data = data.filter(d => {
            const date = new Date(d.date);
            return date >= startDate && date <= endDate;
        });

        if (isCumulativeView) {
            let total = 0;
            data = data.map(d => ({
                date: d.date,
                totalHours: (total += d.totalHours)
            }));
        }

        const svg = d3.select("#gamesChart svg").remove();
        const newSvg = d3.select("#gamesChart")
            .append("svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${this.height + this.margin.top + this.margin.bottom}`)
            .append("g")
            .attr("transform", `translate(${this.margin.left},${this.margin.top})`);

        const x = d3.scaleTime()
            .domain(d3.extent(data, d => new Date(d.date)))
            .range([0, this.width]);

        const y = d3.scaleLinear()
            .domain([0, d3.max(data, d => d.totalHours)])
            .range([this.height, 0]);

        this.addGridAndAxes(newSvg, x, y);

        const line = d3.line()
            .x(d => x(new Date(d.date)))
            .y(d => y(d.totalHours))
            .curve(d3.curveMonotoneX);

        newSvg.append("path")
            .datum(data)
            .attr("fill", "none")
            .attr("stroke", graphConfigs.totalHoursPlayed.serverTypes[0].color)
            .attr("stroke-width", 2)
            .attr("d", line)
            .style("opacity", 0.7);
    }

    addGridAndAxes(svg, x, y) {
        svg.append("g")
            .attr("class", "grid")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x)
                .tickSize(-this.height)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("class", "grid")
            .call(d3.axisLeft(y)
                .tickSize(-this.width)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x))
            .style("color", "#b8b8b8");

        svg.append("g")
            .call(d3.axisLeft(y)
                .ticks(10)
                .tickFormat(d3.format("d")))
            .style("color", "#b8b8b8");

        svg.append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", -this.margin.left)
            .attr("x", -this.height / 2)
            .attr("dy", "0.8em")
            .style("text-anchor", "middle")
            .style("fill", "#b8b8b8")
            .text(graphConfigs.combinedServers.yAxisLabel);
    }
}

class PopularTerritoriesGraph extends BaseGraph {
    constructor() {
        super();
        this.initializeTerritoryControls();
        this.createChart();
    }

    initializeTerritoryControls() {
        document.getElementById('selectAllTerritories')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="territory"]').forEach(checkbox => checkbox.checked = true);
            this.createChart();
        });

        document.getElementById('deselectAllTerritories')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="territory"]').forEach(checkbox => checkbox.checked = false);
            this.createChart();
        });
    }

    async createChart() {
        const startDate = document.getElementById('startDate').valueAsDate;
        const endDate = document.getElementById('endDate').valueAsDate;
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const selectedTerritories = Array.from(document.querySelectorAll('input[name="territory"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedTerritories.length === 0) return;

        const queryParams = new URLSearchParams({
            graphType: 'popularTerritories',
            playerName
        });

        let data = await this.fetchData(queryParams);
        if (!data) return;

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

        const svg = d3.select("#gamesChart svg").remove();
        const newSvg = d3.select("#gamesChart")
            .append("svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${this.height + this.margin.top + this.margin.bottom}`)
            .append("g")
            .attr("transform", `translate(${this.margin.left},${this.margin.top})`);

        const x = d3.scaleTime()
            .domain(d3.extent(data, d => new Date(d.date)))
            .range([0, this.width]);

        const y = d3.scaleLinear()
            .domain([0, isCumulativeView ?
                d3.max(data, d => d3.max(selectedTerritories, territory => d[territory] || 0)) :
                yAxisMax
            ])
            .range([this.height, 0]);

        this.addGridAndAxes(newSvg, x, y);

        selectedTerritories.forEach(territory => {
            const line = d3.line()
                .x(d => x(new Date(d.date)))
                .y(d => y(d[territory] || 0))
                .curve(d3.curveMonotoneX);

            const territoryConfig = graphConfigs.popularTerritories.serverTypes
                .find(type => type.key === territory);

            newSvg.append("path")
                .datum(data)
                .attr("fill", "none")
                .attr("stroke", territoryConfig.color)
                .attr("stroke-width", 1.5)
                .attr("d", line)
                .style("opacity", 0.7)
                .on("mouseover", function () {
                    d3.select(this)
                        .style("opacity", 1)
                        .style("stroke-width", 2.5);
                })
                .on("mouseout", function () {
                    d3.select(this)
                        .style("opacity", 0.7)
                        .style("stroke-width", 1.5);
                });
        });

        this.addLegend(newSvg, selectedTerritories);
    }

    addGridAndAxes(svg, x, y) {
        svg.append("g")
            .attr("class", "grid")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x)
                .tickSize(-this.height)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("class", "grid")
            .call(d3.axisLeft(y)
                .tickSize(-this.width)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x))
            .style("color", "#b8b8b8");

        svg.append("g")
            .call(d3.axisLeft(y)
                .ticks(10)
                .tickFormat(d3.format("d")))
            .style("color", "#b8b8b8");

        svg.append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", -this.margin.left)
            .attr("x", -this.height / 2)
            .attr("dy", "0.8em")
            .style("text-anchor", "middle")
            .style("fill", "#b8b8b8")
            .text(graphConfigs.combinedServers.yAxisLabel);
    }

    addLegend(svg, selectedTerritories) {
        const selectedTerritoryTypes = graphConfigs.popularTerritories.serverTypes
            .filter(type => selectedTerritories.includes(type.key));

        const legend = svg.append("g")
            .attr("class", "legend")
            .attr("transform", `translate(0,${this.height + 40})`);

        const desiredColumns = 5;
        const legendItemWidth = this.width / desiredColumns;
        const legendItemHeight = 20;
        const itemsPerRow = desiredColumns;

        selectedTerritoryTypes.forEach((territoryType, i) => {
            const row = Math.floor(i / itemsPerRow);
            const col = i % itemsPerRow;

            const legendItem = legend.append("g")
                .attr("transform", `translate(${col * legendItemWidth},${row * legendItemHeight})`);

            legendItem.append("rect")
                .attr("width", 15)
                .attr("height", 15)
                .attr("fill", territoryType.color)
                .style("opacity", 0.7);

            legendItem.append("text")
                .attr("x", 24)
                .attr("y", 12)
                .text(territoryType.name)
                .style("fill", "#b8b8b8")
                .style("font-size", "12px");

            legendItem
                .style("cursor", "pointer")
                .on("mouseover", function () {
                    svg.selectAll("path")
                        .style("opacity", 0.1);
                    svg.selectAll("path")
                        .filter(function () {
                            return d3.select(this).attr("stroke") === territoryType.color;
                        })
                        .style("opacity", 1)
                        .style("stroke-width", 2.5);

                    d3.select(this).select("rect")
                        .style("opacity", 1);
                })
                .on("mouseout", function () {
                    svg.selectAll("path")
                        .style("opacity", 0.7)
                        .style("stroke-width", 1.5);

                    d3.select(this).select("rect")
                        .style("opacity", 0.7);
                });
        });

        const legendRows = Math.ceil(selectedTerritoryTypes.length / itemsPerRow);
        const totalHeight = this.height + this.margin.top + this.margin.bottom + (legendRows * legendItemHeight);
        d3.select("#gamesChart svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${totalHeight}`);
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
            'Antartica': 'AN',
            'North Africa': 'NAF',
            'South Africa': 'SAF'
        };
        this.setupData = null;
        this.initializeSetupControls();
        this.createChart();
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
            });
            this.updateChart();
        });

        document.getElementById('deselectAllSetups')?.addEventListener('click', () => {
            document.querySelectorAll('input[name="setup"]').forEach(checkbox => {
                checkbox.checked = false;
            });
            this.updateChart();
        });

        document.getElementById('updateGraph')?.addEventListener('click', () => {
            this.updateChart();
        });
    }

    async createChart() {
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const queryParams = new URLSearchParams({
            graphType: '1v1setupStatistics',
            playerName
        });

        if (!this.setupData) {
            const data = await this.fetchData(queryParams);
            if (!data) return;
            this.setupData = data;

            this.populateSetupCheckboxes(this.setupData);
        }

        this.updateChart();
    }

    async updateChart() {
        const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
        const playerName = this.getPlayerName();

        const queryParams = new URLSearchParams({
            graphType: '1v1setupStatistics',
            playerName
        });

        const data = await this.fetchData(queryParams);
        if (!data) return;
        this.setupData = data;

        this.populateSetupCheckboxes(this.setupData);

        const selectedSetups = Array.from(document.querySelectorAll('input[name="setup"]:checked'))
            .map(checkbox => checkbox.value);

        if (selectedSetups.length === 0) return;

        const filteredData = this.setupData.filter(setup => selectedSetups.includes(setup.setup));
        this.renderChart(filteredData, yAxisMax);
    }

    populateSetupCheckboxes(data) {
        const checkboxContainer = document.querySelector('.setup-checkboxes');
        if (!checkboxContainer) return;

        const currentStates = new Map();
        document.querySelectorAll('input[name="setup"]').forEach(checkbox => {
            currentStates.set(checkbox.value, checkbox.checked);
        });

        checkboxContainer.innerHTML = `
            <div class="server-filters">
                <div class="server-checkboxes">
                    <div class="checkbox-column"></div>
                    <div class="checkbox-column"></div>
                </div>
            </div>
        `;

        const leftColumn = checkboxContainer.querySelector('.checkbox-column:first-child');
        const rightColumn = checkboxContainer.querySelector('.checkbox-column:last-child');

        const sortedSetups = [...data].sort((a, b) => b.total_games - a.total_games);

        sortedSetups.forEach((setup, index) => {
            const label = document.createElement('label');
            label.style.fontSize = '15px';
            label.style.display = 'block';
            label.style.marginBottom = '5px';
            label.style.color = '#b8b8b8';

            const isChecked = currentStates.has(setup.setup) ?
                currentStates.get(setup.setup) :
                true;

            label.innerHTML = `
                <input type="checkbox" name="setup" value="${setup.setup}" ${isChecked ? 'checked' : ''}>
                ${this.formatSetupLabel(setup.setup)}
            `;

            (index % 2 === 0 ? leftColumn : rightColumn).appendChild(label);
        });
    }

    renderChart(data, yAxisMax) {
        const svg = d3.select("#gamesChart svg").remove();
        const newSvg = d3.select("#gamesChart")
            .append("svg")
            .attr("viewBox", `0 0 ${this.width + this.margin.left + this.margin.right} ${this.height + this.margin.top + this.margin.bottom}`)
            .append("g")
            .attr("transform", `translate(${this.margin.left},${this.margin.top})`);

        const x0 = d3.scaleBand()
            .domain(data.map(d => d.setup))
            .range([0, this.width])
            .padding(0.3);

        const x1 = d3.scaleBand()
            .domain([0, 1])
            .range([0, x0.bandwidth()])
            .padding(0.05);

        const y = d3.scaleLinear()
            .domain([0, yAxisMax])
            .range([this.height, 0]);

        this.addGridAndAxes(newSvg, x0, y);
        this.addBars(newSvg, data, x0, x1, y);
    }

    addGridAndAxes(svg, x, y) {
        svg.append("g")
            .attr("class", "grid")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x)
                .tickFormat(d => this.formatSetupLabel(d))
                .tickSize(-this.height))
            .style("color", "rgba(255, 255, 255, 0.1)")
            .selectAll("text")
            .style("text-anchor", "end")
            .attr("dx", "-.8em")
            .attr("dy", ".15em")
            .attr("transform", "rotate(-45)");

        svg.append("g")
            .attr("class", "grid")
            .call(d3.axisLeft(y)
                .tickSize(-this.width)
                .tickFormat(""))
            .style("color", "rgba(255, 255, 255, 0.1)");

        svg.append("g")
            .attr("transform", `translate(0,${this.height})`)
            .call(d3.axisBottom(x).tickFormat(d => this.formatSetupLabel(d)))
            .style("color", "#b8b8b8")
            .selectAll("text")
            .style("text-anchor", "end")
            .attr("dx", "-.8em")
            .attr("dy", ".15em")
            .attr("transform", "rotate(-45)");

        svg.append("g")
            .call(d3.axisLeft(y)
                .ticks(10)
                .tickFormat(d3.format("d")))
            .style("color", "#b8b8b8");

        svg.append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", -this.margin.left)
            .attr("x", -this.height / 2)
            .attr("dy", "0.8em")
            .style("text-anchor", "middle")
            .style("fill", "#b8b8b8")
            .text(graphConfigs["1v1setupStatistics"].yAxisLabel);
    }

    addBars(svg, data, x0, x1, y) {
        data.forEach(setup => {
            const setupGroup = svg.append("g")
                .attr("transform", `translate(${x0(setup.setup)},0)`);

            const [t1, t2] = setup.territories;
            const wins1 = setup[t1];
            const wins2 = setup[t2];
            const setupColors = wins1 > wins2 ? ['#4CAF50', '#FF4444'] : ['#FF4444', '#4CAF50'];

            const createTooltip = (territory, wins, total_games) => {
                d3.selectAll('.tooltip').remove();
                return d3.select("body").append("div")
                    .attr("class", "tooltip")
                    .style("position", "absolute")
                    .style("background-color", "#333")
                    .style("padding", "10px")
                    .style("border-radius", "5px")
                    .style("color", "#fff")
                    .style("pointer-events", "none")
                    .html(`
                        <strong>${territory}</strong><br>
                        Wins: ${wins}<br>
                        Win Rate: ${((wins / total_games) * 100).toFixed(1)}%<br>
                        Total Games: ${total_games}
                    `);
            };

            setupGroup.append("rect")
                .attr("x", x1(0))
                .attr("y", y(setup[t1]))
                .attr("width", x1.bandwidth())
                .attr("height", this.height - y(setup[t1]))
                .attr("fill", setupColors[0])
                .style("opacity", 0.7)
                .on("mouseover", function (event) {
                    d3.select(this).style("opacity", 1);
                    const tooltip = createTooltip(t1, setup[t1], setup.total_games);
                    tooltip.style("left", (event.pageX + 10) + "px")
                        .style("top", (event.pageY - 10) + "px");
                })
                .on("mousemove", function (event) {
                    d3.select('.tooltip')
                        .style("left", (event.pageX + 10) + "px")
                        .style("top", (event.pageY - 10) + "px");
                })
                .on("mouseout", function () {
                    d3.select(this).style("opacity", 0.7);
                    d3.selectAll('.tooltip').remove();
                });

            setupGroup.append("rect")
                .attr("x", x1(1))
                .attr("y", y(setup[t2]))
                .attr("width", x1.bandwidth())
                .attr("height", this.height - y(setup[t2]))
                .attr("fill", setupColors[1])
                .style("opacity", 0.7)
                .on("mouseover", function (event) {
                    d3.select(this).style("opacity", 1);
                    const tooltip = createTooltip(t2, setup[t2], setup.total_games);
                    tooltip.style("left", (event.pageX + 10) + "px")
                        .style("top", (event.pageY - 10) + "px");
                })
                .on("mousemove", function (event) {
                    d3.select('.tooltip')
                        .style("left", (event.pageX + 10) + "px")
                        .style("top", (event.pageY - 10) + "px");
                })
                .on("mouseout", function () {
                    d3.select(this).style("opacity", 0.7);
                    d3.selectAll('.tooltip').remove();
                });
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

    const pathname = window.location.pathname;
    switch (pathname) {
        case '/about':
            new IndividualServersGraph();
            break;
        case '/about/combined-servers':
            new CombinedServersGraph();
            break;
        case '/about/hours-played':
            new TotalHoursGraph();
            break;
        case '/about/popular-territories':
            new PopularTerritoriesGraph();
            break;
        case '/about/1v1-setup-statistics':
            new SetupStatisticsGraph();
            break;
    }
});

window.addEventListener('resize', _.debounce(() => {
    const pathname = window.location.pathname;
    switch (pathname) {
        case '/about':
            new IndividualServersGraph();
            break;
        case '/about/combined-servers':
            new CombinedServersGraph();
            break;
        case '/about/hours-played':
            new TotalHoursGraph();
            break;
        case '/about/popular-territories':
            new PopularTerritoriesGraph();
            break;
        case '/about/1v1-setup-statistics':
            new SetupStatisticsGraph();
            break;
    }
}, 250));