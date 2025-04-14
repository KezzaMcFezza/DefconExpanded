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

import { formatDuration, getTimeAgo } from '../main/main.js';
import { territoryMapping } from './constants.js';

function createDemoCard(demo) {
  const demoDate = new Date(demo.date);
  const demoCard = document.createElement('div');
  demoCard.className = 'demo-card';
  demoCard.dataset.demoId = demo.id;

  const isExpandedGame = (demo.game_type && (
    demo.game_type.includes('8 Player') || demo.game_type.includes('4v4') ||
    demo.game_type.includes('10 Player') || demo.game_type.includes('5v5') ||
    demo.game_type.includes('16 Player') || demo.game_type.includes('8v8') ||
    demo.game_type.includes('509') || demo.game_type.includes('CG') ||
    demo.game_type.includes('MURICON')
  ));

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

  let parsedPlayers = [];
  let groupScores = {};
  let highestScore = 0;
  let usingAlliances = false;

  if (demo.players) {
    try {
      parsedPlayers = JSON.parse(demo.players);
      usingAlliances = parsedPlayers.some(player => player.alliance !== undefined);

      const allianceColors = isExpandedGame ? expandedAllianceColors : vanillaAllianceColors;
      const colorSystem = usingAlliances ? allianceColors : teamColors;

      parsedPlayers.forEach((player, index) => {
        const groupId = usingAlliances ? player.alliance : player.team;

        if (!groupScores[groupId]) {
          groupScores[groupId] = 0;
        }

        groupScores[groupId] += player.score;

        if (player.score > highestScore) {
          highestScore = player.score;
        }

        player.profileUrl = demo[`player${index + 1}_name_profileUrl`] || null;
      });
    } catch (e) {
      console.error('Error parsing players JSON:', e);
    }
  }

  const sortedGroups = Object.entries(groupScores).sort((a, b) => b[1] - a[1]);
  let winningMessage = 'No winning team determined.';
  const colorSystem = usingAlliances ? (isExpandedGame ? expandedAllianceColors : vanillaAllianceColors) : teamColors;

  if (parsedPlayers.length > 0) {
    const playersPerGroup = {};
    parsedPlayers.forEach(player => {
      const groupId = usingAlliances ? player.alliance : player.team;
      playersPerGroup[groupId] = (playersPerGroup[groupId] || 0) + 1;
    });

    const uniqueGroups = Object.keys(playersPerGroup).length;
    const isAllSoloPlayers = Object.values(playersPerGroup).every(count => count === 1);

    if (uniqueGroups === 2) {
      const [winningGroupId, winningScore] = sortedGroups[0];
      const [secondGroupId, secondScore] = sortedGroups[1];

      if (winningScore === secondScore) {
        winningMessage = 'The game is a tie, nobody wins or loses!';
      } else {
        const scoreDifference = winningScore - secondScore;
        const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
        const secondGroupName = colorSystem[secondGroupId]?.name || `Team ${secondGroupId}`;
        const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';
        const secondGroupColor = colorSystem[secondGroupId]?.color || '#b8b8b8';
        winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won against <span style="color: ${secondGroupColor}">${secondGroupName}</span> by ${scoreDifference} points.`;
      }
    }
    else if (isAllSoloPlayers) {
      const sortedPlayers = [...parsedPlayers].sort((a, b) => b.score - a.score);
      const winner = sortedPlayers[0];
      const secondPlace = sortedPlayers[1];

      if (winner.score === secondPlace.score) {
        winningMessage = 'The game is a tie, nobody wins or loses!';
      } else {
        const scoreDifference = winner.score - secondPlace.score;
        const winnerGroupId = usingAlliances ? winner.alliance : winner.team;
        const secondPlaceGroupId = usingAlliances ? secondPlace.alliance : secondPlace.team;
        const winnerColor = colorSystem[winnerGroupId]?.color || '#b8b8b8';
        const secondPlaceColor = colorSystem[secondPlaceGroupId]?.color || '#b8b8b8';
        winningMessage = `<span style="color: ${winnerColor}">${winner.name}</span> won with ${scoreDifference} more points than <span style="color: ${secondPlaceColor}">${secondPlace.name}</span>.`;
      }
    }
    else {
      const [winningGroupId, winningScore] = sortedGroups[0];

      const isTie = sortedGroups.some(([groupId, score], index) => 
        index > 0 && score === winningScore
      );

      if (isTie) {
        winningMessage = 'The game is a tie, nobody wins or loses!';
      } else {
        const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
        const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';

        if (uniqueGroups >= 3) {
          winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won with ${winningScore} points.`;
        }
      }
    }
  }

  let demoCardHtml = `
    <div class="demo-content">
      <h3 class="game-type">${demo.game_type || 'Unknown'}</h3>
      <div style="display: flex; justify-content: space-between;">
        <p class="time-info">
          <span class="time-ago">${getTimeAgo(demo.date)}</span><br>
          <span class="game-added">Game Added - ${demoDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</span><br>
          <span class="game-date">Date - ${demoDate.toLocaleDateString()}</span><br>
          <span class="game-duration">Game Duration - ${formatDuration(demo.duration)}</span>
        </p>
        <p class="winning-team-info" style="text-align: right; color: #b8b8b8;">
          ${winningMessage}
        </p>
      </div>`;

  demoCardHtml += `
    <div class="territory-map-container" style="position: relative;">
      <img src="/images/base_map.png" class="base-map" alt="Base Map" style="width: 100%; position: relative; z-index: 1;">`;

  parsedPlayers.forEach((player, index) => {
    if (territoryMapping[player.territory]) {
      const groupId = usingAlliances ? player.alliance : player.team;
      let colorValue = colorSystem[groupId]?.color || '#00bf00';
      colorValue = colorValue.replace('#', '');

      const overlayImage = `/images/${territoryMapping[player.territory]}${colorValue}.png`;

      demoCardHtml += `
        <img src="${overlayImage}" class="territory-overlay" alt="${player.territory} Overlay" 
             style="position: absolute; top: 0; left: 0; width: 100%; z-index: ${index + 2};">`;
    }
  });

  demoCardHtml += `</div>`;

  demoCardHtml += `
    <table class="game-results">
      <tr>
        <th>Player</th>
        <th>Territory</th>
        <th>Score</th>
      </tr>`;

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

          demoCardHtml += `
            <tr>
              <td style="color: ${groupColor}">
                ${playerIconLink} ${playerNameWithCrown}
              </td>
              <td>${player.territory || 'undefined'}</td>
              <td>${player.score}</td>
            </tr>`;
        });
    });
  } else {
    demoCardHtml += '<tr><td colspan="3">No player data available</td></tr>';
  }

  demoCardHtml += `</table>`;

  if (demo.spectators) {
    try {
      const parsedSpectators = JSON.parse(demo.spectators);
      if (parsedSpectators.length > 0) {
        demoCardHtml += `
          <div class="spectators-section">
            <button class="spectators-toggle" onclick="toggleSpectators(this)">
              <i class="fas fa-eye"></i> Show Spectators (${parsedSpectators.length})
            </button>
            <div class="spectators-list">
              <table class="spectators-table" style="margin-bottom: 0;">
                <tr>
                </tr>
                ${parsedSpectators.map(spectator => `
                  <tr>
                    <td style="color: #919191;">${spectator.name}</td>
                  </tr>
                `).join('')}
              </table>
            </div>
          </div>`;
      }
    } catch (e) {
      console.error('Error parsing spectators JSON:', e);
    }
  }

  demoCardHtml += `
    <div class="demo-actions">
      <a href="/api/download/${demo.name}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
      <button class="btn-report" onclick="showReportOptions(${demo.id}, event)">Report</button>
      ${window.userRole !== undefined && window.userRole <= 5 ? `
        <span style="color: #888888b0; text-shadow: unset; text-shadow: 0px 0px 0px currentColor; margin-left: auto;">Demo ID: ${demo.id}</span>
      ` : ''}
      <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
    </div>
  </div>`;

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

function displayDemos(demos) {
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

  const ensureUserRole = async () => {
    if (window.userRole === undefined) {
      try {
        const response = await fetch('/api/current-user');
        const data = await response.json();
        if (data.user) {
          window.userRole = data.user.role;
        }
      } catch (error) {
        console.error('Error fetching user role:', error);
      }
    }

    demos.forEach((demo, index) => {
      const columnIndex = index % columnCount;
      try {
        const demoCard = createDemoCard(demo);
        columns[columnIndex].appendChild(demoCard);
      } catch (error) {
        console.error(`Error creating demo card for demo ID ${demo.id}:`, error);
      }
    });

    columns.forEach(column => demoContainer.appendChild(column));
  };

  ensureUserRole();
}

export { 
  createDemoCard, 
  toggleSpectators, 
  displayDemos
};