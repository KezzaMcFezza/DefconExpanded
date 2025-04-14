// DefconExpanded, Created by...
// KezzaMcFezza - Main Developer
// Nexustini - Server Management
//
// Notable Mentions...
// Rad - For helping with python scripts.
// Bert_the_turtle - Doing everything with c++
//
// Inspired by Sievert and Wan May
// 
// Last Edited 14-04-2025

// Default filters used for the leaderboard
export const leaderboardFilters = {
    serverName: '',
    sortBy: 'wins',
    startDate: '',
    endDate: '',
    minGames: '1',
    playlist: ''
};

// Server playlists for grouping
export const serverPlaylists = {
    all: { name: "All Servers", servers: [] },
    oneVsOne: {
        name: "1v1 Servers",
        servers: [
            "New Player Server",
            "New Player Server - Training Game",
            "DefconExpanded | 1v1 | Totally Random",
            "DefconExpanded | 1V1 | Best Setups Only!",
            "DefconExpanded | 1v1 | AF vs AS | Totally Random",
            "DefconExpanded | 1v1 | EU vs SA | Totally Random",
            "DefconExpanded | 1v1 | Default",
            "MURICON | 1v1 Default | 2.8.15"
        ]
    },
    twoVsTwo: {
        name: "2v2 Servers",
        servers: [
            "DefconExpanded | 2v2 | Totally Random",
            "DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random",
            "Mojo's 2v2 Arena - Quick Victory",
            "Sony and Hoov's Hideout"
        ]
    },
    Tournament: {
        name: "2v2 Tournament",
        servers: [
            "2v2 Tournament"
        ]
    },
    threeVsThree: {
        name: "3v3 Servers",
        servers: [
            "DefconExpanded | 3v3 | Totally Random"
        ]
    },
    fourVsFour: {
        name: "4v4 Servers",
        servers: [
            "DefconExpanded | 4V4 | Totally Random"
        ]
    },
    ffa: {
        name: "Free For All",
        servers: [
            "DefconExpanded | Free For All | Random Cities",
            "DefconExpanded | 8 Player | Diplomacy",
            "DefconExpanded | 10 Player | Diplomacy"
        ]
    }
};

// First season information
export const firstSeason = {
    name: 'The Beginning',
    displayName: 'The Beginning Y1S1',
    startDate: '2024-08-27',
    endDate: '2025-02-26',
    year: 1,
    season: 1
};

// Season boundary definitions
export const seasonBoundaries = [
    { startMonth: 2, startDay: 27, endMonth: 4, endDay: 26 },
    { startMonth: 4, startDay: 27, endMonth: 8, endDay: 31 },
    { startMonth: 9, startDay: 1, endMonth: 12, endDay: 31 },
    { startMonth: 1, startDay: 1, endMonth: 2, endDay: 26 }
];

// Default player names to exclude from leaderboards
export const defaultNames = [
    'NewPlayer',
    'NewPlayer_1',
    'NewPlayer_2',
    'NewPlayer_3',
    'NewPlayer_4',
    'NewPlayer_5',
    'NewPlayer_6',
    'NewPlayer_7',
    'NewPlayer_8',
    'NewPlayer_9'
];