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
//Last Edited 07-06-2025

import { 
  teamColors,
  vanillaAllianceColors,
  expandedAllianceColors
} from './constants.js';

function scoreCalculationLogic(demo) {
  const isExpandedGame = (demo.game_type && (
    demo.game_type.includes('8 Player') || demo.game_type.includes('4v4') ||
    demo.game_type.includes('10 Player') || demo.game_type.includes('5v5') ||
    demo.game_type.includes('16 Player') || demo.game_type.includes('8v8') ||
    demo.game_type.includes('509') || demo.game_type.includes('CG') ||
    demo.game_type.includes('MURICON')
  ));

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
  const colorSystem = usingAlliances ? (isExpandedGame ? expandedAllianceColors : vanillaAllianceColors) : teamColors;
  
  const winningMessage = calculateWinningMessage(parsedPlayers, sortedGroups, colorSystem, usingAlliances);

  return {
    parsedPlayers,
    groupScores,
    sortedGroups,
    highestScore,
    usingAlliances,
    colorSystem,
    isExpandedGame,
    winningMessage
  };
}

function calculateWinningMessage(parsedPlayers, sortedGroups, colorSystem, usingAlliances) {
  let winningMessage = 'No winning team determined.';

  if (parsedPlayers.length > 0) {
      const allPlayersHaveZeroScore = parsedPlayers.every(player => player.score === 0);
      if (allPlayersHaveZeroScore) {
        return '<span style="color: #ffa500;">Game may be incomplete</span>';
      }
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

        if (Object.keys(colorSystem).length >= 3) {
          winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won with ${winningScore} points.`;
        }
      }
    }
  }

  return winningMessage;
}

export { 
  scoreCalculationLogic,
  calculateWinningMessage
}; 