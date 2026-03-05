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
//Last Edited 19-09-2025

import { 
  formatDuration,
  getTimeAgo 
} from '../main/main.js';

import { getAuthState } from '../main/authentication.js';

import { 
  territoryMapping,
  DEMO_CARD_TEMPLATE
} from './constants.js';

import { 
  scoreCalculationLogic
} from './scorecalculation.js';

let demoIdPermissionCache = null;
let permissionCacheTime = 0;
const CACHE_DURATION = 5 * 60 * 1000;
let currentUserFetchPromise = null;

async function canViewDemoId() {
  const auth = await getAuthState();
  if (!auth.isLoggedIn) {
    demoIdPermissionCache = false;
    permissionCacheTime = Date.now();
    return false;
  }

  const now = Date.now();

  if (demoIdPermissionCache !== null && (now - permissionCacheTime) < CACHE_DURATION) {
    return demoIdPermissionCache;
  }

  if (currentUserFetchPromise) {
    return currentUserFetchPromise;
  }

  currentUserFetchPromise = (async () => {
    try {
      const response = await fetch('/api/current-user', { credentials: 'include' });
      if (!response.ok) {
        demoIdPermissionCache = false;
        permissionCacheTime = Date.now();
        return false;
      }

      const data = await response.json();
      if (!data.user || !data.user.permissions) {
        demoIdPermissionCache = false;
        permissionCacheTime = Date.now();
        return false;
      }

      const hasPermission = data.user.permissions.includes(204);
      demoIdPermissionCache = hasPermission;
      permissionCacheTime = Date.now();
      return hasPermission;
    } catch (error) {
      demoIdPermissionCache = false;
      permissionCacheTime = Date.now();
      return false;
    } finally {
      currentUserFetchPromise = null;
    }
  })();

  return currentUserFetchPromise;
}

function generateTerritoryMap(demo, parsedPlayers, colorSystem, usingAlliances) {
  const parsedGameData = typeof demo.players === 'string' ? JSON.parse(demo.players) : demo.players;
  const isUKMod = parsedGameData?.mapMod === 'UK' || 
                 (demo.game_type && (demo.game_type.includes('UK and Ireland') || demo.game_type.includes('UK Mod')));

  const baseMap = isUKMod ? 'basemapuk.png' : 'base_map.png';
  
  let territoryOverlays = '';
  parsedPlayers.forEach((player, index) => {
    if (territoryMapping[player.territory]) {
      const groupId = usingAlliances ? player.alliance : player.team;
      let colorValue = colorSystem[groupId]?.color || '#00bf00';
      colorValue = colorValue.replace('#', '');

      const overlayImage = `/images/${territoryMapping[player.territory]}${colorValue}.png`;

      territoryOverlays += DEMO_CARD_TEMPLATE.TERRITORY_OVERLAY
        .replace('{{OVERLAY_IMAGE}}', overlayImage)
        .replace('{{TERRITORY}}', player.territory)
        .replace('{{Z_INDEX}}', index + 2);
    }
  });

  return DEMO_CARD_TEMPLATE.TERRITORY_MAP
    .replace('{{BASE_MAP}}', baseMap)
    .replace('{{TERRITORY_OVERLAYS}}', territoryOverlays);
}

function generateResultsTable(parsedPlayers, sortedGroups, colorSystem, highestScore, usingAlliances) {
  let tableRows = '';

  if (parsedPlayers.length > 0) {
    sortedGroups.forEach(([groupId, _]) => {
      const groupColor = colorSystem[groupId]?.color || '#00bf00';
      parsedPlayers
        .filter(player => {
          const playerGroupId = usingAlliances ? player.alliance : player.team;
          return playerGroupId === Number(groupId);
        })
        .sort((a, b) => b.score - a.score)
        .forEach(player => {
          const playerNameWithCrown = `${player.name}${player.score === highestScore ? ' 👑' : ''}`;
          const playerIconLink = player.profileUrl
            ? `<a href="${player.profileUrl}" target="_blank"><i class="fa-solid fa-square-up-right"></i></a>`
            : '';

          tableRows += DEMO_CARD_TEMPLATE.RESULTS_ROW
            .replace('{{GROUP_COLOR}}', groupColor)
            .replace('{{PLAYER_ICON}}', playerIconLink)
            .replace('{{PLAYER_NAME}}', playerNameWithCrown)
            .replace('{{TERRITORY}}', player.territory || 'undefined')
            .replace('{{SCORE}}', player.score === 0 ? '???' : player.score);
        });
    });
  } else {
    tableRows = '<tr><td colspan="3">No player data available</td></tr>';
  }

  return DEMO_CARD_TEMPLATE.RESULTS_TABLE.replace('{{TABLE_ROWS}}', tableRows);
}

function generateSpectatorsSection(demo) {
  if (!demo.spectators) return '';

  try {
    const parsedSpectators = JSON.parse(demo.spectators);
    if (parsedSpectators.length === 0) return '';

    const spectatorRows = parsedSpectators
      .map(spectator => DEMO_CARD_TEMPLATE.SPECTATOR_ROW.replace('{{SPECTATOR_NAME}}', spectator.name))
      .join('');

    return DEMO_CARD_TEMPLATE.SPECTATORS_SECTION
      .replace('{{SPECTATOR_COUNT}}', parsedSpectators.length)
      .replace('{{SPECTATOR_ROWS}}', spectatorRows);
  } catch (e) {
    console.error('Error parsing spectators JSON:', e);
    return '';
  }
}

function generateDemoActions(demo, canViewId) {
  
  let result = DEMO_CARD_TEMPLATE.DEMO_ACTIONS
    .replace(/\{\{DEMO_NAME\}\}/g, demo.name)
    .replace(/\{\{DEMO_NAME_ENCODED\}\}/g, encodeURIComponent(demo.name))
    .replace('{{DEMO_ID}}', demo.id)
    .replace('{{DOWNLOAD_COUNT}}', demo.download_count || 0)
    .replace('{{WATCH_COUNT}}', demo.watch_count || 0);
  
  
  return result;
}

function formatGameType(gameType) {
  if (!gameType) return 'Unknown';
  
  let formatted = gameType;
  
  formatted = formatted.replace(/NA-SA-EU-AF/g, 'NA SA EU AF');
  formatted = formatted.replace(/[|-]/g, '');
  formatted = formatted.replace(/(DefconExpanded|New Player Server|MURICON)\s*(1v1|2v2|3v3)/g, '$1 $2');
  formatted = formatted.replace(/(DefconExpanded|New Player Server|MURICON)\s*(Free For All|8 Player|10 Player|16 Player)/g, '$1<br>$2');
  formatted = formatted.replace(/\s*(Totally Random)$/g, '<br>$1');
  
  return formatted;
}

function replaceTemplate(template, replacements) {
  let result = template;
  for (const [key, value] of Object.entries(replacements)) {
    result = result.replace(new RegExp(`{{${key}}}`, 'g'), value);
  }
  return result;
}

async function createDemoCard(demo) {
  const demoDate = new Date(demo.date);
  const demoCard = document.createElement('div');
  demoCard.className = 'demo-card';
  demoCard.dataset.demoId = demo.id;

  const scoreData = scoreCalculationLogic(demo);
  const canViewId = await canViewDemoId();

  const territoryMap = generateTerritoryMap(demo, scoreData.parsedPlayers, scoreData.colorSystem, scoreData.usingAlliances);
  const resultsTable = generateResultsTable(scoreData.parsedPlayers, scoreData.sortedGroups, scoreData.colorSystem, scoreData.highestScore, scoreData.usingAlliances);
  const spectatorsSection = generateSpectatorsSection(demo);
  const demoActions = generateDemoActions(demo, canViewId);

  const templateInsertions = {
    GAME_TYPE: formatGameType(demo.game_type),
    TIME_AGO: getTimeAgo(demo.date),
    GAME_TIME: demoDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
    GAME_DATE: demoDate.toLocaleDateString(),
    GAME_DURATION: formatDuration(demo.duration),
    WINNING_MESSAGE: scoreData.winningMessage,
    DEMO_ID_DISPLAY: canViewId 
      ? `<span style="color: #888888b0; text-shadow: unset; text-shadow: 0px 0px 0px currentColor; display: flex; justify-content: flex-end;">Demo ID: ${demo.id}</span>` 
      : '',
    TERRITORY_MAP: territoryMap,
    RESULTS_TABLE: resultsTable,
    SPECTATORS_SECTION: spectatorsSection,
    DEMO_ACTIONS: demoActions
  };

  const demoCardHtml = replaceTemplate(DEMO_CARD_TEMPLATE.MAIN_STRUCTURE, templateInsertions);
  demoCard.innerHTML = demoCardHtml;
  
  return demoCard;
}

function toggleSpectators(button) {
  const spectatorsList = button.nextElementSibling;
  const isHidden = !spectatorsList.classList.contains('show');

  spectatorsList.classList.toggle('show', isHidden);
  button.innerHTML = isHidden ?
    `<i class="fas fa-eye-slash"></i> Hide Spectators` :
    `<i class="fas fa-eye"></i> Show Spectators`;
}

async function launchReplayViewer(demoName) {
  
  if (demoName && demoName.includes('{{')) {
    console.error('ERROR: Demo name still contains template placeholders:', demoName);
    alert('Error: Template replacement failed. Demo name contains placeholders: ' + demoName);
    return;
  }
  
  try {
    try {
      await fetch(`/api/watch/${encodeURIComponent(demoName)}`, {
        method: 'POST',
        credentials: 'include'
      });
    } catch (watchError) {
      console.warn('Failed to increment watch count:', watchError);
    }

    const apiUrl = `/api/replay-viewer/launch/${encodeURIComponent(demoName)}`;
  
    const response = await fetch(apiUrl, {
      method: 'GET',
      credentials: 'include'
    });
    
    
    const data = await response.json();
    
    if (response.ok && data.success) {
      window.open(data.replayUrl, '_blank');
    } else {
      console.error('API Error Response:', {
        status: response.status,
        statusText: response.statusText,
        data: data
      });
      
      let errorMessage = data.message || data.error || 'Unknown error';
      if (data.searchedFor) {
        errorMessage += `\nSearched for: "${data.searchedFor}"`;
      }
      if (data.sampleDemos) {
        errorMessage += `\nSample demos in DB: ${data.sampleDemos.join(', ')}`;
      }
      
      alert(`Failed to launch replay viewer: ${errorMessage}`);
    }
  } catch (error) {
    console.error('Error launching replay viewer:', error);
    alert('Failed to launch replay viewer. Please try again.');
  }
}

async function displayDemos(demos) {
  const demoContainer = document.getElementById('demo-container');
  if (!demoContainer) {
    console.error('Demo container not found.');
    return;
  }

  demoContainer.innerHTML = '';

  let columnCount;
  if (window.innerWidth < 500) {
    columnCount = 1;
  } else if (window.innerWidth < 1024) {
    columnCount = 2;
  } else {
    columnCount = 3;
  }

  const columns = Array.from({ length: columnCount }, () => document.createElement('div'));
  columns.forEach(column => column.className = 'demo-column');

  const demoCardPromises = demos.map(async (demo, index) => {
    const columnIndex = index % columnCount;
    try {
      const demoCard = await createDemoCard(demo);
      return { demoCard, columnIndex };
    } catch (error) {
      console.error(`Error creating demo card for demo ID ${demo.id}:`, error);
      return null;
    }
  });

  const demoCardResults = await Promise.all(demoCardPromises);
  
  demoCardResults.forEach(result => {
    if (result) {
      columns[result.columnIndex].appendChild(result.demoCard);
    }
  });

  columns.forEach(column => demoContainer.appendChild(column));
}

export { 
  createDemoCard, 
  toggleSpectators, 
  displayDemos,
  launchReplayViewer
};