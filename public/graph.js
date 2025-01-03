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
            { key: "ant", name: "Antartica", color: "#85144b" },
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

function formatSetupLabel(setup) {
    return setup.split('_vs_')
        .map(territory => territoryAbbreviations[territory] || territory)
        .join(' vs ');
}

function initializeGraphControls() {
    const endDate = new Date();
    const startDate = new Date();
    startDate.setMonth(startDate.getMonth() - 6);

    document.getElementById('startDate').valueAsDate = startDate;
    document.getElementById('endDate').valueAsDate = endDate;

    document.getElementById('selectAllServers').addEventListener('click', () => {
        document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = true);
    });

    document.getElementById('deselectAllServers').addEventListener('click', () => {
        document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = false);
    });

    document.getElementById('updateGraph').addEventListener('click', createGamesChart);

    const yAxisControl = document.querySelector('.y-axis-control');
    const toggleButton = document.createElement('button');
    toggleButton.textContent = 'Show Linear View';
    toggleButton.className = 'cumulative-toggle-btn';
    toggleButton.addEventListener('click', () => {
        isCumulativeView = !isCumulativeView;
        toggleButton.textContent = isCumulativeView ? 'Show Individual View' : 'Show Linear View';
        createGamesChart();
    });
    yAxisControl.appendChild(toggleButton);

    document.getElementById('graph-filter').addEventListener('change', (e) => {
        const selectedType = e.target.value;
        updateGraphVisibility(selectedType);
        createGamesChart();
    });

    createGamesChart();
}

function updateGraphVisibility(graphType) {
    const serverFilters = document.querySelector('.server-filters');
    const cumulativeButton = document.querySelector('.cumulative-toggle-btn');
    const dateControls = document.querySelector('.date-controls');
    const resetApplyButtons = document.querySelector('.reset-apply-servers');

    if (cumulativeButton) {
        cumulativeButton.style.display = graphType === '1v1setupStatistics' ? 'none' : 'block';
    }

    if (dateControls) {
        dateControls.style.display = graphType === '1v1setupStatistics' ? 'none' : 'block';
    }
    
    const yAxisInput = document.getElementById('yAxisMax');
    if (graphType === '1v1setupStatistics') {
        yAxisInput.value = 40;

        if (!document.querySelector('.setup-checkboxes')) {
            const checkboxContainer = document.createElement('div');
            checkboxContainer.className = 'setup-checkboxes';
            document.querySelector('.stats-controls').appendChild(checkboxContainer);
        }

        const setupCheckboxes = document.querySelector('.setup-checkboxes');
        setupCheckboxes.style.display = 'block';
        setupCheckboxes.innerHTML = ''; 

        const selectAllButton = document.getElementById('selectAllServers');
        const deselectAllButton = document.getElementById('deselectAllServers');
        if (selectAllButton && deselectAllButton) {
            selectAllButton.addEventListener('click', () => {
                document.querySelectorAll('input[name="setup"]').forEach(checkbox => checkbox.checked = true);
                createGamesChart();
            });
            
            deselectAllButton.addEventListener('click', () => {
                document.querySelectorAll('input[name="setup"]').forEach(checkbox => checkbox.checked = false);
                createGamesChart();
            });
        }

        const updateGraphButton = document.getElementById('updateGraph');
        if (updateGraphButton) {
            updateGraphButton.remove();
        }

        createGamesChart();
    } else {
        yAxisInput.value = 16;
        const setupCheckboxes = document.querySelector('.setup-checkboxes');
        if (setupCheckboxes) {
            setupCheckboxes.style.display = 'none';
        }

        const selectAllButton = document.getElementById('selectAllServers');
        const deselectAllButton = document.getElementById('deselectAllServers');
        if (selectAllButton && deselectAllButton) {
            selectAllButton.onclick = () => {
                document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = true);
            };
            deselectAllButton.onclick = () => {
                document.querySelectorAll('input[name="server"]').forEach(checkbox => checkbox.checked = false);
            };
        }
    }

    if (graphType === 'individualServers') {
        serverFilters.style.display = 'block';
    } else {
        serverFilters.style.display = 'none';
    }
}

function createGamesChart() {
    const margin = { top: 5, right: 20, bottom: 35, left: 40 };
    const chartContainer = document.getElementById('gamesChart');
    const width = chartContainer.offsetWidth - margin.left - margin.right;
    const height = 350;
    const startDate = document.getElementById('startDate').valueAsDate;
    const endDate = document.getElementById('endDate').valueAsDate;
    const yAxisMax = parseInt(document.getElementById('yAxisMax').value);
    const graphType = document.getElementById('graph-filter').value || 'individualServers';
    const playerName = document.querySelector('.player-input')?.value || '';

    d3.select("#gamesChart svg").remove();

    const queryParams = new URLSearchParams({
        graphType: graphType
    });
    
    if (playerName) {
        queryParams.append('playerName', playerName);
    }

    fetch(`/api/games-timeline?${queryParams.toString()}`)
        .then(response => response.json())
        .then(data => {
            let filteredData = data.filter(d => {
                const date = new Date(d.date);
                return date >= startDate && date <= endDate;
            });

            const svg = d3.select("#gamesChart")
                .append("svg")
                .attr("viewBox", `0 0 ${width + margin.left + margin.right} ${height + margin.top + margin.bottom}`)
                .attr("preserveAspectRatio", "xMidYMid meet")
                .append("g")
                .attr("transform", `translate(${margin.left},${margin.top})`);

            if (graphType === '1v1setupStatistics') {
                create1v1SetupChart(filteredData, svg, width, height, margin);
            } else {
                createRegularChart(filteredData, svg, width, height, margin, graphType, yAxisMax);
            }
        })
        .catch(error => {
            console.error('Error fetching games data:', error);
            chartContainer.innerHTML = 'Error loading chart data';
        });
}

function createRegularChart(filteredData, svg, width, height, margin, graphType, yAxisMax) {
    let selectedServers;
    if (graphType === 'individualServers') {
        selectedServers = Array.from(document.querySelectorAll('input[name="server"]:checked'))
            .map(checkbox => checkbox.value);
    } else {
        selectedServers = graphConfigs[graphType].serverTypes.map(type => type.key);
    }

    if (isCumulativeView) {
        const cumulativeData = [];
        const serverTotals = {};

        filteredData.forEach((dayData) => {
            const newDayData = { date: dayData.date };
            selectedServers.forEach(server => {
                serverTotals[server] = (serverTotals[server] || 0) + (dayData[server] || 0);
                newDayData[server] = serverTotals[server];
            });
            cumulativeData.push(newDayData);
        });

        filteredData = cumulativeData;
    }

    const x = d3.scaleTime()
        .domain(d3.extent(filteredData, d => new Date(d.date)))
        .range([0, width]);

    const yMax = isCumulativeView
        ? d3.max(filteredData, d =>
            d3.max(selectedServers, server => d[server] || 0))
        : yAxisMax;

    const y = d3.scaleLinear()
        .domain([0, yMax])
        .range([height, 0]);

    svg.append("g")
        .attr("class", "grid")
        .attr("transform", `translate(0,${height})`)
        .call(d3.axisBottom(x)
            .tickSize(-height)
            .tickFormat(""))
        .style("color", "rgba(255, 255, 255, 0.1)");

    svg.append("g")
        .attr("class", "grid")
        .call(d3.axisLeft(y)
            .tickSize(-width)
            .tickFormat(""))
        .style("color", "rgba(255, 255, 255, 0.1)");

    svg.append("g")
        .attr("transform", `translate(0,${height})`)
        .call(d3.axisBottom(x))
        .style("color", "#b8b8b8");

    svg.append("g")
        .attr("class", "y-axis")
        .call(d3.axisLeft(y)
            .ticks(10)
            .tickFormat(d3.format("d")))
        .style("color", "#b8b8b8");

    svg.append("text")
        .attr("transform", "rotate(-90)")
        .attr("y", -margin.left)
        .attr("x", -height / 2)
        .attr("dy", "0.8em")
        .style("text-anchor", "middle")
        .style("fill", "#b8b8b8")
        .text(graphConfigs[graphType].yAxisLabel);

    const config = graphConfigs[graphType];
    selectedServers.forEach(server => {
        const line = d3.line()
            .x(d => x(new Date(d.date)))
            .y(d => y(d[server] || 0))
            .curve(d3.curveMonotoneX);

        const serverConfig = config.serverTypes.find(type => type.key === server);

        svg.append("path")
            .datum(filteredData)
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

    const selectedServerTypes = config.serverTypes.filter(type =>
        selectedServers.includes(type.key)
    );

    const legend = svg.append("g")
        .attr("class", "legend")
        .attr("transform", `translate(0,${height + 40})`);

    const desiredColumns = 5;
    const legendItemWidth = width / desiredColumns;
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
    const totalHeight = height + margin.top + margin.bottom + (legendRows * legendItemHeight);
    d3.select("#gamesChart svg")
        .attr("viewBox", `0 0 ${width + margin.left + margin.right} ${totalHeight}`);
}

function create1v1SetupChart(data, svg, width, height, margin) {
    const config = graphConfigs['1v1setupStatistics'];
    const barPadding = 0.3;
    const yAxisMax = parseInt(document.getElementById('yAxisMax').value);

    const mainTerritories = [
        'Russia',
        'Europe',
        'Asia',
        'Africa',
        'South America',
        'North America'
    ];

    const territoryAbbreviations = {
        'North America': 'NA',
        'South America': 'SA',
        'Europe': 'EU',
        'Russia': 'RU',
        'Africa': 'AF',
        'Asia': 'AS'
    };

    const defaultSetups = [
        'Africa_vs_Asia',
        'Europe_vs_North America',
        'Africa_vs_South America',
        'Europe_vs_South America',
        'North America_vs_Russia',
        'Africa_vs_North America'
    ];

    function formatSetupLabel(setup) {
        return setup.split('_vs_')
            .map(territory => territoryAbbreviations[territory] || territory)
            .join(' vs ');
    }

    const validSetups = data.filter(setup => {
        const [t1, t2] = setup.territories;
        return mainTerritories.includes(t1) && mainTerritories.includes(t2);
    });

    const selectedSetups = Array.from(document.querySelectorAll('input[name="setup"]:checked'))
        .map(checkbox => checkbox.value);

    const topSetups = validSetups.filter(setup => selectedSetups.includes(setup.setup));

    const chartContainer = document.getElementById('gamesChart');
    chartContainer.innerHTML = '';

    svg = d3.select("#gamesChart")
        .append("svg")
        .attr("viewBox", `0 0 ${width + margin.left + margin.right} ${height + margin.top + margin.bottom}`)
        .attr("preserveAspectRatio", "xMidYMid meet")
        .style("width", "100%")
        .style("height", "100%")
        .append("g")
        .attr("transform", `translate(${margin.left},${margin.top})`);

    const x0 = d3.scaleBand()
        .domain(topSetups.map(d => d.setup))
        .range([0, width])
        .padding(barPadding);

    const x1 = d3.scaleBand()
        .domain([0, 1])
        .range([0, x0.bandwidth()])
        .padding(0.05);

    const dataMax = d3.max(topSetups, d =>
        Math.max(d[d.territories[0]], d[d.territories[1]])
    );

    const yMax = Math.max(dataMax, yAxisMax);

    const y = d3.scaleLinear()
        .domain([0, yMax])
        .range([height, 0]);

    const xAxis = d3.axisBottom(x0)
        .tickFormat(d => formatSetupLabel(d));

    svg.append("g")
        .attr("class", "grid")
        .call(d3.axisLeft(y)
            .tickSize(-width)
            .tickFormat(""))
        .style("color", "rgba(255, 255, 255, 0.1)");

    svg.append("g")
        .attr("transform", `translate(0,${height})`)
        .call(xAxis)
        .selectAll("text")
        .style("text-anchor", "end")
        .attr("dx", "1.75rem")
        .attr("dy", ".15em")
        .style("color", "#b8b8b8");

    svg.append("g")
        .call(d3.axisLeft(y)
            .ticks(10)
            .tickFormat(d3.format("d")))
        .style("color", "#b8b8b8");

    svg.append("text")
        .attr("transform", "rotate(-90)")
        .attr("y", -margin.left)
        .attr("x", -height / 2)
        .attr("dy", "0.8em")
        .style("text-anchor", "middle")
        .style("fill", "#b8b8b8")
        .text(config.yAxisLabel);

    topSetups.forEach(setup => {
        const setupGroup = svg.append("g")
            .attr("transform", `translate(${x0(setup.setup)},0)`);

        const wins1 = setup[setup.territories[0]];
        const wins2 = setup[setup.territories[1]];
        const setupColors = wins1 > wins2 ? ['#4CAF50', '#FF4444'] : ['#FF4444', '#4CAF50'];

        setupGroup.append("rect")
            .attr("x", x1(0))
            .attr("y", y(setup[setup.territories[0]]))
            .attr("width", x1.bandwidth())
            .attr("height", height - y(setup[setup.territories[0]]))
            .attr("fill", setupColors[0])
            .style("opacity", 0.7)
            .on("mouseover", function (event) {
                d3.select(this)
                    .style("opacity", 1);

                const tooltip = d3.select("body").append("div")
                    .attr("class", "tooltip")
                    .style("position", "absolute")
                    .style("background-color", "#333")
                    .style("padding", "10px")
                    .style("border-radius", "5px")
                    .style("color", "#fff")
                    .html(`
                        <strong>${setup.territories[0]}</strong><br>
                        Wins: ${setup[setup.territories[0]]}<br>
                        Win Rate: ${((setup[setup.territories[0]] / setup.total_games) * 100).toFixed(1)}%<br>
                        Total Games: ${setup.total_games}
                    `);

                tooltip.style("left", (event.pageX + 10) + "px")
                    .style("top", (event.pageY - 10) + "px");
            })
            .on("mouseout", function () {
                d3.select(this)
                    .style("opacity", 0.7);
                d3.select(".tooltip").remove();
            });

        setupGroup.append("rect")
            .attr("x", x1(1))
            .attr("y", y(setup[setup.territories[1]]))
            .attr("width", x1.bandwidth())
            .attr("height", height - y(setup[setup.territories[1]]))
            .attr("fill", setupColors[1])
            .style("opacity", 0.7)
            .on("mouseover", function (event) {
                d3.select(this)
                    .style("opacity", 1);

                const tooltip = d3.select("body").append("div")
                    .attr("class", "tooltip")
                    .style("position", "absolute")
                    .style("background-color", "#333")
                    .style("padding", "10px")
                    .style("border-radius", "5px")
                    .style("color", "#fff")
                    .html(`
                        <strong>${setup.territories[1]}</strong><br>
                        Wins: ${setup[setup.territories[1]]}<br>
                        Win Rate: ${((setup[setup.territories[1]] / setup.total_games) * 100).toFixed(1)}%<br>
                        Total Games: ${setup.total_games}
                    `);

                tooltip.style("left", (event.pageX + 10) + "px")
                    .style("top", (event.pageY - 10) + "px");
            })
            .on("mouseout", function () {
                d3.select(this)
                    .style("opacity", 0.7);
                d3.select(".tooltip").remove();
            });
    });

    const checkboxContainer = document.querySelector('.setup-checkboxes');
    checkboxContainer.innerHTML = `
        <div class="server-filters">
            <div class="filter-header">
                <h3 class="filter-title">Setup Selection</h3>
                <div class="reset-apply-servers">
                    <button id="selectAllServers" class="apply-servers-btn">Select All</button>
                    <button id="deselectAllServers" class="reset-servers-btn">Deselect All</button>
                </div>
            </div>
            <div class="server-checkboxes">
                <div class="checkbox-column">
                </div>
                <div class="checkbox-column">
                </div>
            </div>
        </div>
        <button id="updateGraph" class="update-graph-btn">Update Graph</button>
    `;

    const updateGraphButton = document.getElementById('updateGraph');
    if (updateGraphButton) {
        updateGraphButton.addEventListener('click', createGamesChart);
    }

    const leftColumn = checkboxContainer.querySelector('.checkbox-column:first-child');
    const rightColumn = checkboxContainer.querySelector('.checkbox-column:last-child');

    const allCombinations = [];
    for (let i = 0; i < mainTerritories.length; i++) {
        for (let j = i + 1; j < mainTerritories.length; j++) {
            const territories = [mainTerritories[i], mainTerritories[j]].sort();
            allCombinations.push(`${territories[0]}_vs_${territories[1]}`);
        }
    }

    allCombinations.forEach((setup, index) => {
        const label = document.createElement('label');
        label.style.display = 'block';
        label.style.marginBottom = '5px';
        label.style.color = '#b8b8b8';

        const isChecked = selectedSetups.length > 0
            ? selectedSetups.includes(setup)
            : defaultSetups.includes(setup);

        label.innerHTML = `
            <input type="checkbox" name="setup" value="${setup}" 
                   ${isChecked ? 'checked' : ''}>
            ${formatSetupLabel(setup)}
        `;
        
        (index % 2 === 0 ? leftColumn : rightColumn).appendChild(label);
    });
}

function downloadAPIData() {
    fetch(`/api/games-timeline?graphType=1v1setupStatistics`)
        .then(response => response.json())
        .then(data => {
            const jsonString = JSON.stringify(data, null, 2);
            const blob = new Blob([jsonString], { type: 'application/json' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = '1v1_setup_data.json';

            document.body.appendChild(a);
            a.click();

            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        })
        .catch(error => {
            console.error('Error downloading data:', error);
        });
}

document.addEventListener('DOMContentLoaded', initializeGraphControls);

window.addEventListener('resize', _.debounce(createGamesChart, 250));