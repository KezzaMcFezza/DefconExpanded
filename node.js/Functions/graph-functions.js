const { pool } = require('./database-backup');

// Process data for combined servers graph
const processCombinedServersData = (rows) => {
  const gamesByDate = {};

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        allServers: 0
      };
    }

    gamesByDate[date].allServers++;
  });

  return Object.values(gamesByDate);
};

// Process data for total hours played graph
const processTotalHoursData = (rows) => {
  const gamesByDate = {};

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        totalHours: 0
      };
    }

    if (row.duration) {
      const [hours, minutes, seconds] = row.duration.split(':');
      const totalHours = parseFloat(hours) + parseFloat(minutes) / 60 + parseFloat(seconds.split('.')[0]) / 3600;
      gamesByDate[date].totalHours += totalHours;
    }
  });

  return Object.values(gamesByDate);
};

// Process data for popular territories graph
const processTerritoriesData = (rows) => {
  const gamesByDate = {};
  const territoryMapping = {
    'North America': 'na',
    'South America': 'sa',
    'Europe': 'eu',
    'Russia': 'ru',
    'Africa': 'af',
    'Asia': 'as',
    'Australasia': 'au',
    'West Asia': 'we',
    'East Asia': 'ea',
    'Antartica': 'ant',
    'North Africa': 'naf',
    'South Africa': 'saf'
  };

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        na: 0, sa: 0, eu: 0, ru: 0, af: 0, as: 0,
        au: 0, we: 0, ea: 0, ant: 0, naf: 0, saf: 0
      };
    }

    // Count territories from all player slots
    for (let i = 1; i <= 10; i++) {
      const territory = row[`player${i}_territory`];
      if (territory) {
        const key = territoryMapping[territory];
        if (key && gamesByDate[date][key] !== undefined) {
          gamesByDate[date][key]++;
        }
      }
    }
  });

  return Object.values(gamesByDate);
};

// Process data for 1v1 setup statistics
const process1v1SetupData = (rows) => {
  const setupStats = {};

  rows.forEach(row => {
    let gameData = { players: [], spectators: [] };
    try {
      if (!row.players) return;

      const parsedData = JSON.parse(row.players);
      if (typeof parsedData === 'object') {
        if (parsedData.players && Array.isArray(parsedData.players)) {
          gameData.players = parsedData.players;
        } else if (Array.isArray(parsedData)) {
          gameData.players = parsedData;
        } else {
          return;
        }
      }

      // Make sure it's a valid 1v1 game
      if (gameData.players.length !== 2) return;

      const [player1, player2] = gameData.players;
      if (!player1?.territory || !player2?.territory ||
        player1.score === undefined || player2.score === undefined) {
        return;
      }

      // Create setup key (territories alphabetically ordered)
      const territories = [player1.territory, player2.territory].sort();
      const setupKey = territories.join('_vs_');

      if (!setupStats[setupKey]) {
        setupStats[setupKey] = {
          date: new Date(row.date).toISOString().split('T')[0],
          total_games: 0,
          territories: territories,
          [territories[0]]: 0,
          [territories[1]]: 0,
          total_duration: 0,
          average_score_diff: 0,
          games_with_score: 0
        };
      }

      setupStats[setupKey].total_games++;

      // Track winner
      const winner = player1.score > player2.score ? player1 : player2;
      setupStats[setupKey][winner.territory]++;

      // Track duration if available
      if (row.duration) {
        const [hours, minutes, seconds] = row.duration.split(':');
        const durationInMinutes = (parseFloat(hours) * 60) + parseFloat(minutes) + (parseFloat(seconds) / 60);
        setupStats[setupKey].total_duration += durationInMinutes;
      }

      // Track score differences
      const scoreDiff = Math.abs(player1.score - player2.score);
      setupStats[setupKey].average_score_diff =
        ((setupStats[setupKey].average_score_diff * setupStats[setupKey].games_with_score) + scoreDiff) /
        (setupStats[setupKey].games_with_score + 1);
      setupStats[setupKey].games_with_score++;

    } catch (error) {
      console.error('Error processing 1v1 game:', error);
      console.log('Problematic row:', row);
    }
  });

  // Convert to array and calculate final stats
  const sortedSetups = Object.entries(setupStats)
    .map(([key, stats]) => ({
      setup: key,
      ...stats,
      average_duration: stats.total_duration / stats.total_games,
      win_rate: {
        [stats.territories[0]]: ((stats[stats.territories[0]] / stats.total_games) * 100).toFixed(2) + '%',
        [stats.territories[1]]: ((stats[stats.territories[1]] / stats.total_games) * 100).toFixed(2) + '%'
      }
    }))
    .sort((a, b) => b.total_games - a.total_games);

  return sortedSetups;
};

// Process data for individual servers graph
const processIndividualServersData = (rows) => {
  const gamesByDate = {};

  rows.forEach(row => {
    const date = new Date(row.date).toISOString().split('T')[0];

    if (!gamesByDate[date]) {
      gamesByDate[date] = {
        date,
        new_player: 0,
        training: 0,
        defcon_random: 0,
        defcon_best: 0,
        defcon_default: 0,
        defcon_afas: 0,
        defcon_eusa: 0,
        defcon_2v2: 0,
        tournament_2v2: 0,
        defcon_2v2_special: 0,
        mojo_2v2: 0,
        sony_hoov: 0,
        defcon_3v3: 0,
        muricon: 0,
        cg_2v2_2815: 0,
        cg_2v2_28141: 0,
        cg_1v1_2815: 0,
        cg_1v1_28141: 0,
        defcon_ffa: 0,
        defcon_8p_diplo: 0,
        defcon_4v4: 0,
        defcon_10p_diplo: 0,
      };
    }

    // Map game types to counters
    if (row.game_type.toLowerCase() === 'new player server'.toLowerCase()) {
      gamesByDate[date].new_player++;
    } else if (row.game_type.toLowerCase() === 'new player server - training game'.toLowerCase()) {
      gamesByDate[date].training++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_random++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | best setups only!'.toLowerCase()) {
      gamesByDate[date].defcon_best++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | af vs as | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_afas++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | eu vs sa | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_eusa++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 1v1 | default'.toLowerCase()) {
      gamesByDate[date].defcon_default++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 2v2 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_2v2++;
    } else if (row.game_type.toLowerCase() === '2v2 tournament'.toLowerCase()) {
      gamesByDate[date].tournament_2v2++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 2v2 | na-sa-eu-af | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_2v2_special++;
    } else if (row.game_type.toLowerCase() === 'mojo\'s 2v2 arena - quick victory'.toLowerCase()) {
      gamesByDate[date].mojo_2v2++;
    } else if (row.game_type.toLowerCase() === 'sony and hoov\'s hideout'.toLowerCase()) {
      gamesByDate[date].sony_hoov++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 3v3 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_3v3++;
    } else if (row.game_type.toLowerCase() === 'muricon | 1v1 default | 2.8.15'.toLowerCase()) {
      gamesByDate[date].muricon++;
    } else if (row.game_type.toLowerCase() === '509 cg | 2v2 | totally random | 2.8.15'.toLowerCase()) {
      gamesByDate[date].cg_2v2_2815++;
    } else if (row.game_type.toLowerCase() === '509 cg | 2v2 | totally random | 2.8.14.1'.toLowerCase()) {
      gamesByDate[date].cg_2v2_28141++;
    } else if (row.game_type.toLowerCase() === '509 cg | 1v1 | totally random | 2.8.15'.toLowerCase()) {
      gamesByDate[date].cg_1v1_2815++;
    } else if (row.game_type.toLowerCase() === '509 cg | 1v1 | totally random | 2.8.14.1'.toLowerCase()) {
      gamesByDate[date].cg_1v1_28141++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | free for all | random cities'.toLowerCase()) {
      gamesByDate[date].defcon_ffa++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 8 player | diplomacy'.toLowerCase()) {
      gamesByDate[date].defcon_8p_diplo++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 4v4 | totally random'.toLowerCase()) {
      gamesByDate[date].defcon_4v4++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 10 player | diplomacy'.toLowerCase()) {
      gamesByDate[date].defcon_10p_diplo++;
    } else if (row.game_type.toLowerCase() === 'defconexpanded | 16 player | test server'.toLowerCase()) {
      gamesByDate[date].defcon_16p++;
    }
  });

  return Object.values(gamesByDate);
};

// Get games timeline data
const getGamesTimelineData = async (graphType, playerName) => {
  try {
    // Base query for all types
    let query = `SELECT date, game_type, duration, players`;

    // Add territory columns for territory stats
    for (let i = 1; i <= 10; i++) {
      query += `, player${i}_territory`;
    }

    let queryParams = [];

    let baseQuery = ` FROM demos WHERE game_type IN (
      'New Player Server',
      'New Player Server - Training Game',
      'DefconExpanded | 1v1 | Totally Random',
      'DefconExpanded | 1V1 | Best Setups Only!',
      'DefconExpanded | 1v1 | AF vs AS | Totally Random',
      'DefconExpanded | 1v1 | EU vs SA | Totally Random',
      'DefconExpanded | 1v1 | Default',
      'DefconExpanded | 2v2 | Totally Random',
      '2v2 Tournament',
      'DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random',
      'Mojo\\'s 2v2 Arena - Quick Victory',
      'Sony and Hoov\\'s Hideout',
      'DefconExpanded | 3v3 | Totally Random',
      'MURICON | 1v1 Default | 2.8.15',
      '509 CG | 2v2 | Totally Random | 2.8.15',
      '509 CG | 2v2 | Totally Random | 2.8.14.1',
      '509 CG | 1v1 | Totally Random | 2.8.15',
      '509 CG | 1v1 | Totally Random | 2.8.14.1',
      'DefconExpanded | Free For All | Random Cities',
      'DefconExpanded | 8 Player | Diplomacy',
      'DefconExpanded | 4V4 | Totally Random',
      'DefconExpanded | 10 Player | Diplomacy'
    )`;

    query += baseQuery;

    // Add player name filter if provided
    if (playerName) {
      query += ` AND (player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? 
                     OR player4_name LIKE ? OR player5_name LIKE ? OR player6_name LIKE ? 
                     OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? 
                     OR player10_name LIKE ?)`;
      queryParams = Array(10).fill(`%${playerName}%`);
    }

    query += ` ORDER BY date ASC`;

    const [rows] = await pool.query(query, queryParams);

    // Process data based on graph type
    let chartData;
    switch (graphType) {
      case 'combinedServers':
        chartData = processCombinedServersData(rows);
        break;

      case 'totalHoursPlayed':
        chartData = processTotalHoursData(rows);
        break;

      case 'popularTerritories':
        chartData = processTerritoriesData(rows);
        break;

      case 'individualServers':
      default:
        chartData = processIndividualServersData(rows);
        break;

      case '1v1setupStatistics':
        chartData = process1v1SetupData(rows);
        break;
    }

    return chartData;
  } catch (error) {
    console.error('Error fetching games timeline data:', error);
    throw error;
  }
};

// Get earliest game date
const getEarliestGameDate = async () => {
  try {
    const [rows] = await pool.query('SELECT MIN(date) as earliestDate FROM demos');
    return rows[0].earliestDate;
  } catch (error) {
    console.error('Error getting earliest game date:', error);
    throw error;
  }
};

// Get most active players
const getMostActivePlayers = async (serverName, limit = 5) => {
  try {
    // Build query with optional server filter
    let query = 'SELECT * FROM demos';
    let params = [];

    if (serverName) {
      query += ' WHERE game_type LIKE ?';
      params.push(`%${serverName}%`);
    }

    const [demos] = await pool.query(query, params);

    // Get blacklisted players
    const [blacklist] = await pool.query('SELECT player_name FROM leaderboard_whitelist');
    const blacklistedPlayers = new Set(blacklist.map(entry => entry.player_name.toLowerCase()));

    const playerCounts = {};

    // Process each demo to count player appearances
    for (const demo of demos) {
      try {
        let playersData = [];
        if (demo.players) {
          const parsedData = JSON.parse(demo.players);
          if (typeof parsedData === 'object') {
            playersData = Array.isArray(parsedData.players) ? parsedData.players :
              (Array.isArray(parsedData) ? parsedData : []);
          }
        }

        // Count appearances for each player
        for (const player of playersData) {
          if (player.name && !blacklistedPlayers.has(player.name.toLowerCase())) {
            playerCounts[player.name] = (playerCounts[player.name] || 0) + 1;
          }
        }
      } catch (error) {
        console.error('Error processing demo player data:', error);
      }
    }

    // Convert to array and sort by games played
    const activePlayers = Object.entries(playerCounts)
      .map(([player_name, games_played]) => ({ player_name, games_played }))
      .sort((a, b) => b.games_played - a.games_played)
      .slice(0, limit);

    return activePlayers;
  } catch (error) {
    console.error('Error calculating most active players:', error);
    throw error;
  }
};

module.exports = {
  processCombinedServersData,
  processTotalHoursData,
  processTerritoriesData,
  process1v1SetupData,
  processIndividualServersData,
  getGamesTimelineData,
  getEarliestGameDate,
  getMostActivePlayers
};