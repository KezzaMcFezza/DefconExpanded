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
//Last Edited 01-04-2025

const territoryMapping = {
    'North America': 'northamerica',
    'South America': 'southamerica',
    'Europe': 'europe',
    'Africa': 'africa',
    'Asia': 'asia',
    'Russia': 'russia',
    'North Africa': 'northafrica',
    'South Africa': 'southafrica',
    'East Asia': 'eastasia',
    'West Asia': 'westasia',
    'South Asia': 'southasia',
    'Australasia': 'australasia',
    'Antartica': 'antartica'
};

const filterState = {
    territories: new Set(),
    players: [],
    combineMode: false,
    scoreFilter: '',
    durationFilter: '',
    excludeNewPlayers: true,
    scoreDifferenceFilter: '',
    dateRange: {
        start: null,
        end: null
    },
    gamesPlayed: ''
};

const territoryMappingUI = {
    'na': 'North America',
    'sa': 'South America',
    'eu': 'Europe',
    'ru': 'Russia',
    'af': 'Africa',
    'as': 'Asia',
    'au': 'Australasia',
    'we': 'West Asia',
    'ea': 'East Asia',
    'ant': 'Antartica',
    'naf': 'North Africa',
    'saf': 'South Africa'
};

export {
    territoryMapping,
    filterState,
    territoryMappingUI
};