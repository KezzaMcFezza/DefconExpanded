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
//Last Edited 25-05-2025

const express = require('express');
const router = express.Router();

const {
    pool
} = require('../../constants');

const {
    process1v1SetupData,
    processCombinedServersData,
    processIndividualServersData,
    processTerritoriesData,
    processTotalHoursData,
} = require('../../graph-functions')


const debug = require('../../debug-helpers');

router.get('/api/games-timeline', async (req, res) => {
    const startTime = debug.enter('getGamesTimeline', [req.query], 1);
    try {
        const { graphType = 'individualServers', playerName, startDate, endDate } = req.query;
        debug.level2('Games timeline request:', { graphType, playerName, startDate, endDate });

        let query = `SELECT date, game_type, duration, players`;

        for (let i = 1; i <= 10; i++) {
            query += `, player${i}_territory`;
        }

        let queryParams = [];
        let conditions = [];

        let baseQuery = ` FROM demos WHERE game_type IN (
        'New Player Server',
        'New Player Server - Training Game',
        'DefconExpanded | Training Server',
        'DefconExpanded Test Server',
        'DefconExpanded | 1v1 | Totally Random',
        'DefconExpanded | 1V1 | Best Setups Only!',
        'DefconExpanded | 1v1 | Cursed Setups Only!',
        'DefconExpanded | 1v1 | Lots of Units!',
        'DefconExpanded | 1v1 | UK and Ireland',
        'DefconExpanded | 1v1 | Quick Match',
        'Raizer\\'s Russia vs USA | Totally Random',
        'DefconExpanded | 1v1 | AF vs AS | Totally Random',
        'DefconExpanded | 1v1 | EU vs SA | Totally Random',
        'DefconExpanded | 1v1 | Default',
        'Christmas Tournament 2025',
        'DefconExpanded | 2v2 | Totally Random',
        'DefconExpanded | 2v2 | UK and Ireland',
        '2v2 Tournament',
        'DefconExpanded | 2v2 | Max Cities / Pop',
        '2v2 Tournament',
        'DefconExpanded | 2v2 | NA-SA-EU-AF | Totally Random',
        'Mojo\\'s 2v2 Arena - Quick Victory',
        'Sony and Hoov\\'s Hideout',
        'DefconExpanded | 3v3 | Totally Random',
        'MURICON | UK Mod',
        'DefconExpanded | Diplomacy | UK and Ireland',
        'MURICON | 1v1 Default | 2.8.15',
        'MURICON | 1V1 | Totally Random | 2.8.15',
        '509 CG | 2v2 | Totally Random | 2.8.15',
        '509 CG | 2v2 | Totally Random | 2.8.14.1',
        '509 CG | 1v1 | Totally Random | 2.8.15',
        '509 CG | 1v1 | Totally Random | 2.8.14.1',
        'DefconExpanded | Free For All | Random Cities',
        'DefconExpanded | 8 Player | Diplomacy',
        'DefconExpanded | 4V4 | Totally Random',
        'DefconExpanded | 5v5 | Totally Random',
        'DefconExpanded | 10 Player | Diplomacy',
        'DefconExpanded | 16 Player | Test Server'
      )`;

        query += baseQuery;

        if (playerName) {
            debug.level3('Adding player name filter:', playerName);
            conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? 
                       OR player4_name LIKE ? OR player5_name LIKE ? OR player6_name LIKE ? 
                       OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? 
                       OR player10_name LIKE ?)`);
            for (let i = 0; i < 10; i++) {
                queryParams.push(`%${playerName}%`);
            }
        }

        if (startDate) {
            debug.level3('Adding start date filter:', startDate);
            conditions.push('date >= ?');
            queryParams.push(startDate);
        }

        if (endDate) {
            debug.level3('Adding end date filter:', endDate);
            conditions.push('date <= ?');
            queryParams.push(endDate);
        }

        if (conditions.length > 0) {
            query += ` AND ${conditions.join(' AND ')}`;
        }

        query += ` ORDER BY date ASC`;

        debug.dbQuery(query, queryParams, 2);
        const [rows] = await pool.query(query, queryParams);
        debug.dbResult(rows, 2);

        debug.level2('Processing data with graph type:', graphType);
        let chartData;
        switch (graphType) {
            case 'combinedServers':
                debug.level3('Processing combined servers data');
                chartData = processCombinedServersData(rows);
                break;

            case 'totalHoursPlayed':
                debug.level3('Processing total hours data');
                chartData = processTotalHoursData(rows);
                break;

            case 'popularTerritories':
                debug.level3('Processing territories data');
                chartData = processTerritoriesData(rows);
                break;

            case 'individualServers':
            default:
                debug.level3('Processing individual servers data');
                chartData = processIndividualServersData(rows);
                break;

            case '1v1setupStatistics':
                debug.level3('Processing 1v1 setup statistics');
                chartData = process1v1SetupData(rows, { startDate, endDate });
                break;
        }

        debug.level2('Returning chart data for graph type:', graphType);
        debug.exit('getGamesTimeline', startTime, { graphType, dataPoints: chartData?.datasets?.[0]?.data?.length || 0 }, 1);
        res.json(chartData);
    } catch (error) {
        debug.error('getGamesTimeline', error, 1);
        debug.exit('getGamesTimeline', startTime, 'error', 1);
        res.status(500).json({ error: 'Unable to fetch games timeline data' });
    }
});

module.exports = router;