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

export const SERVER_TYPES = [
    'new_player', 'training', 'defcon_random', 'defcon_best', 'defcon_afas',
    'defcon_eusa', 'defcon_default', 'defcon_2v2', 'tournament_2v2',
    'defcon_2v2_special', 'mojo_2v2', 'sony_hoov', 'defcon_3v3', 'defcon_4v4',
    'muricon', 'cg_2v2_2815', 'cg_2v2_28141', 'cg_1v1_2815', 'cg_1v1_28141',
    'defcon_8p_diplo', 'defcon_10p_diplo'
];

export const TERRITORY_TYPES = [
    'na', 'sa', 'eu', 'ru', 'af', 'as',
    'au', 'we', 'ea', 'ant', 'naf', 'saf'
];

export const GRAPH_CONFIGS = {
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

export const GRAPH_TYPES = {
    INDIVIDUAL_SERVERS: 'individualServers',
    COMBINED_SERVERS: 'combinedServers',
    TOTAL_HOURS: 'totalHoursPlayed',
    POPULAR_TERRITORIES: 'popularTerritories',
    SETUP_STATISTICS: '1v1setupStatistics'
};

export const PAGE_TO_GRAPH_TYPE = {
    '/about': GRAPH_TYPES.INDIVIDUAL_SERVERS,
    '/about/combined-servers': GRAPH_TYPES.COMBINED_SERVERS,
    '/about/hours-played': GRAPH_TYPES.TOTAL_HOURS,
    '/about/popular-territories': GRAPH_TYPES.POPULAR_TERRITORIES,
    '/about/1v1-setup-statistics': GRAPH_TYPES.SETUP_STATISTICS
};