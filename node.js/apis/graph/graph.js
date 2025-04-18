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

router.get('/api/games-timeline', async (req, res) => {
    try {
        const { graphType = 'individualServers', playerName, startDate, endDate } = req.query;

        
        let query = `SELECT date, game_type, duration, players`;

        
        for (let i = 1; i <= 10; i++) {
            query += `, player${i}_territory`;
        }

        let queryParams = [];
        let conditions = [];

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

        
        if (playerName) {
            conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? 
                       OR player4_name LIKE ? OR player5_name LIKE ? OR player6_name LIKE ? 
                       OR player7_name LIKE ? OR player8_name LIKE ? OR player9_name LIKE ? 
                       OR player10_name LIKE ?)`);
            for (let i = 0; i < 10; i++) {
                queryParams.push(`%${playerName}%`);
            }
        }

        
        if (startDate) {
            conditions.push('date >= ?');
            queryParams.push(startDate);
        }

        if (endDate) {
            conditions.push('date <= ?');
            queryParams.push(endDate);
        }

        
        if (conditions.length > 0) {
            query += ` AND ${conditions.join(' AND ')}`;
        }

        query += ` ORDER BY date ASC`;

        const [rows] = await pool.query(query, queryParams);

        
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
                
                chartData = process1v1SetupData(rows, { startDate, endDate });
                break;
        }

        res.json(chartData);
    } catch (error) {
        console.error('Error fetching games timeline data:', error);
        res.status(500).json({ error: 'Unable to fetch games timeline data' });
    }
});

module.exports = router;