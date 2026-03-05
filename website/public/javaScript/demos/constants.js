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
//Last Edited 18-09-2025

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
    'Antartica': 'antartica',
    'Northern Ireland': 'northernireland',
    'Southern Ireland': 'southernireland',
    'South West England': 'southwestengland',
    'South East England': 'southeastengland',
    'Midlands': 'midlands',
    'Scotland': 'scotland',
    'Ireland': 'ireland',
    'Wales': 'wales',
    'South England': 'southengland',
    'N. Ireland': 'northireland'
};

const teamColors = {
    0: { color: '#00bf00', name: 'Green' },
    1: { color: '#ff4949', name: 'Red' },
    2: { color: '#00bf00', name: 'Green' },
    3: { color: '#2da6ff', name: 'Light Blue' },
    4: { color: '#ffa700', name: 'Orange' },
    5: { color: '#3d5cff', name: 'Blue' },
    6: { color: '#e5cb00', name: 'Yellow' },
    7: { color: '#00e5ff', name: 'Turq' },
    8: { color: '#e72de0', name: 'Pink' },
    9: { color: '#e5e5e5', name: 'White' },
    10: { color: '#1fc36a', name: 'Teal' }
};

const vanillaAllianceColors = {
    0: { color: '#ff4949', name: 'Red' },
    1: { color: '#00bf00', name: 'Green' },
    2: { color: '#3d5cff', name: 'Blue' },
    3: { color: '#e5cb00', name: 'Yellow' },
    4: { color: '#ffa700', name: 'Orange' },
    5: { color: '#00e5ff', name: 'Turq' }
};

const expandedAllianceColors = {
    0: { color: '#00bf00', name: 'Green' },
    1: { color: '#ff4949', name: 'Red' },
    2: { color: '#3d5cff', name: 'Blue' },
    3: { color: '#e5cb00', name: 'Yellow' },
    4: { color: '#00e5ff', name: 'Turq' },
    5: { color: '#e72de0', name: 'Pink' },
    6: { color: '#4c4c4c', name: 'Black' },
    7: { color: '#ffa700', name: 'Orange' },
    8: { color: '#28660a', name: 'Olive' },
    9: { color: '#660011', name: 'Scarlet' },
    10: { color: '#2a00ff', name: 'Indigo' },
    11: { color: '#4c4c00', name: 'Gold' },
    12: { color: '#004c3e', name: 'Teal' },
    13: { color: '#6a007f', name: 'Purple' },
    14: { color: '#e5e5e5', name: 'White' },
    15: { color: '#964B00', name: 'Brown' }
};

const filterState = {
    territories: new Set(),
    players: [],
    combineMode: false,
    scoreFilter: '',
    durationFilter: '',
    includeNewPlayers: true,
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
    'saf': 'South Africa',
    'ni': 'Northern Ireland',
    'si': 'Southern Ireland',
    'swe': 'South West England',
    'see': 'South East England',
    'mid': 'Midlands',
    'sco': 'Scotland',
    'ire': 'Ireland',
    'wa': 'Wales',
    'se': 'South England',
    'nir': 'N. Ireland'
};

// HTML Template constants
const DEMO_CARD_TEMPLATE = {
    MAIN_STRUCTURE: `
        <div class="demo-content">
            <h3 class="game-type">{{GAME_TYPE}}</h3>
            <div style="display: flex; justify-content: space-between;">
                <p class="time-info">
                    <span class="time-ago">{{TIME_AGO}}</span><br>
                    <span class="game-added">Game Added - {{GAME_TIME}}</span><br>
                    <span class="game-date">Date - {{GAME_DATE}}</span><br>
                    <span class="game-duration">Game Duration - {{GAME_DURATION}}</span>
                </p>
                <div>
                <p class="winning-team-info" style="text-align: right; color: #b8b8b8;">
                    {{WINNING_MESSAGE}}
                </p>
                {{DEMO_ID_DISPLAY}}
                </div>
            </div>
            {{TERRITORY_MAP}}
            {{RESULTS_TABLE}}
            {{SPECTATORS_SECTION}}
            {{DEMO_ACTIONS}}
        </div>
    `,
    
    TERRITORY_MAP: `
        <div class="territory-map-container" style="position: relative;">
            <img src="/images/{{BASE_MAP}}" class="base-map" alt="Base Map" style="width: 100%; position: relative; z-index: 1;">
            {{TERRITORY_OVERLAYS}}
        </div>
    `,
    
    TERRITORY_OVERLAY: `
        <img src="{{OVERLAY_IMAGE}}" class="territory-overlay" alt="{{TERRITORY}} Overlay" 
             style="position: absolute; top: 0; left: 0; width: 100%; z-index: {{Z_INDEX}};">
    `,
    
    RESULTS_TABLE: `
        <table class="game-results">
            <tr>
                <th>Player</th>
                <th>Territory</th>
                <th>Score</th>
            </tr>
            {{TABLE_ROWS}}
        </table>
    `,
    
    RESULTS_ROW: `
        <tr>
            <td style="color: {{GROUP_COLOR}}">
                {{PLAYER_ICON}} {{PLAYER_NAME}}
            </td>
            <td>{{TERRITORY}}</td>
            <td>{{SCORE}}</td>
        </tr>
    `,
    
    SPECTATORS_SECTION: `
        <div class="spectators-section">
            <button class="spectators-toggle" onclick="toggleSpectators(this)">
                <i class="fas fa-eye"></i> Show Spectators ({{SPECTATOR_COUNT}})
            </button>
            <div class="spectators-list">
                <table class="spectators-table" style="margin-bottom: 0;">
                    <tr></tr>
                    {{SPECTATOR_ROWS}}
                </table>
            </div>
        </div>
    `,
    
    SPECTATOR_ROW: `
        <tr>
            <td style="color: #919191;">{{SPECTATOR_NAME}}</td>
        </tr>
    `,
    
    DEMO_ACTIONS: `
        <div class="demo-actions">
            <a href="/api/download/{{DEMO_NAME_ENCODED}}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
            <button class="download-btn-demo" style="border: none; max-width: 150px; min-height: 30.39px;" onclick="launchReplayViewer('{{DEMO_NAME}}')"><i class="fas fa-play"></i> Watch Online</button>
            <div class="demo-stats">
                <span class="downloads-count"><i class="fas fa-download"></i> {{DOWNLOAD_COUNT}}</span>
                <span class="watch-count"><i class="fas fa-eye"></i> {{WATCH_COUNT}}</span>
            </div>
        </div>
    `
};

export {
    territoryMapping,
    teamColors,
    vanillaAllianceColors,
    expandedAllianceColors,
    filterState,
    territoryMappingUI,
    DEMO_CARD_TEMPLATE
};