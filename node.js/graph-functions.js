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
//Last Edited 18-04-2025

function processCombinedServersData(rows) {
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
}

function processTotalHoursData(rows) {
    const gamesByDate = {};
    const serverTypeMapping = {
        'New Player Server': 'new_player',
        'New Player Server - Training Game': 'training',
        'DefconExpanded | 1v1 | Totally Random': 'defcon_random',
        'DefconExpanded | 1V1 | Best Setups Only!': 'defcon_best',
        'DefconExpanded | 1v1 | AF vs AS | Totally Random': 'defcon_afas',
        'DefconExpanded | 1v1 | EU vs SA | Totally Random': 'defcon_eusa',
        'DefconExpanded | 1v1 | Default': 'defcon_default',
        'DefconExpanded | 2v2 | Totally Random': 'defcon_2v2',
        '2v2 Tournament': 'tournament_2v2',
        'DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random': 'defcon_2v2_special',
        'Mojo\'s 2v2 Arena - Quick Victory': 'mojo_2v2',
        'Sony and Hoov\'s Hideout': 'sony_hoov',
        'DefconExpanded | 3v3 | Totally Random': 'defcon_3v3',
        'MURICON | 1v1 Default | 2.8.15': 'muricon',
        '509 CG | 2v2 | Totally Random | 2.8.15': 'cg_2v2_2815',
        '509 CG | 2v2 | Totally Random | 2.8.14.1': 'cg_2v2_28141',
        '509 CG | 1v1 | Totally Random | 2.8.15': 'cg_1v1_2815',
        '509 CG | 1v1 | Totally Random | 2.8.14.1': 'cg_1v1_28141',
        'DefconExpanded | Free For All | Random Cities': 'defcon_ffa',
        'DefconExpanded | 8 Player | Diplomacy': 'defcon_8p_diplo',
        'DefconExpanded | 8 Player | Diplomacy ': 'defcon_8p_diplo',
        'DefconExpanded | 4V4 | Totally Random': 'defcon_4v4',
        'DefconExpanded | 10 Player | Diplomacy': 'defcon_10p_diplo'
    };

    const diagnostics = {
        totalGames: rows.length,
        gamesByType: {},
        gamesWithDuration: 0,
        gamesWithoutDuration: 0,
        unknownGameTypes: [],
        totalHoursByType: {}
    };

    Object.keys(serverTypeMapping).forEach(type => {
        diagnostics.gamesByType[type] = 0;
        diagnostics.totalHoursByType[type] = 0;
    });

    const caseInsensitiveMapping = {};
    Object.keys(serverTypeMapping).forEach(key => {
        caseInsensitiveMapping[key.toLowerCase()] = {
            originalKey: key,
            serverKey: serverTypeMapping[key]
        };
    });

    rows.forEach(row => {
        const date = new Date(row.date).toISOString().split('T')[0];

        let serverKey = 'unknown';
        let originalGameType = '';

        if (row.game_type) {
            const lookup = caseInsensitiveMapping[row.game_type.toLowerCase()];
            if (lookup) {
                serverKey = lookup.serverKey;
                originalGameType = lookup.originalKey;
                diagnostics.gamesByType[originalGameType]++;
            } else {
                
                diagnostics.unknownGameTypes.push(row.game_type);
                console.warn(`Unknown game type: "${row.game_type}"`);
            }
        }

        if (!gamesByDate[date]) {
            gamesByDate[date] = {
                date,
                totalHours: 0
            };

            Object.values(serverTypeMapping).forEach(key => {
                gamesByDate[date][key] = 0;
            });
        }

        if (row.duration) {
            diagnostics.gamesWithDuration++;
            try {
                
                const [hours, minutes, seconds] = row.duration.split(':');
                const h = parseFloat(hours) || 0;
                const m = parseFloat(minutes) || 0;
                const s = parseFloat((seconds || '0').split('.')[0]) || 0;
                const totalHours = h + m / 60 + s / 3600;

                gamesByDate[date].totalHours += totalHours;
                gamesByDate[date][serverKey] += totalHours;

                if (originalGameType) {
                    diagnostics.totalHoursByType[originalGameType] += totalHours;
                }
            } catch (error) {
                console.error('Error parsing duration:', row.duration, error);
            }
        } else {
            diagnostics.gamesWithoutDuration++;
        }
    });

    const uniqueUnknown = [...new Set(diagnostics.unknownGameTypes)];
    if (uniqueUnknown.length > 0) {
        console.log('Unknown game types:', uniqueUnknown);
    }

    return Object.values(gamesByDate);
}

function processTerritoriesData(rows) {
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
}

function process1v1SetupData(rows, options = {}) {
    const { startDate, endDate } = options;
    const setupStats = {};
    const parsedStartDate = startDate ? new Date(startDate) : null;
    const parsedEndDate = endDate ? new Date(endDate) : null;

    rows.forEach(row => {
        const rowDate = new Date(row.date);

        if (parsedStartDate && rowDate < parsedStartDate) return;
        if (parsedEndDate && rowDate > parsedEndDate) return;

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

            if (gameData.players.length !== 2) return;

            const [player1, player2] = gameData.players;
            if (!player1?.territory || !player2?.territory ||
                player1.score === undefined || player2.score === undefined) {
                return;
            }

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

            const winner = player1.score > player2.score ? player1 : player2;
            setupStats[setupKey][winner.territory]++;

            if (row.duration) {
                const [hours, minutes, seconds] = row.duration.split(':');
                const durationInMinutes = (parseFloat(hours) * 60) + parseFloat(minutes) + (parseFloat(seconds) / 60);
                setupStats[setupKey].total_duration += durationInMinutes;
            }
            
            const scoreDiff = Math.abs(player1.score - player2.score);
            setupStats[setupKey].average_score_diff =
                ((setupStats[setupKey].average_score_diff * setupStats[setupKey].games_with_score) + scoreDiff) /
                (setupStats[setupKey].games_with_score + 1);
            setupStats[setupKey].games_with_score++;

        } catch (error) {
            console.error('Error processing 1v1 game:', error);
        }
    });

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
}

function processIndividualServersData(rows) {
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
}

module.exports = {
    process1v1SetupData,
    processCombinedServersData,
    processIndividualServersData,
    processTerritoriesData,
    processTotalHoursData,
};