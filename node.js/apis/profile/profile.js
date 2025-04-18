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
const path = require('path');
const fs = require('fs');

const {
    pool,
    upload
} = require('../../constants');

const {
    authenticateToken,
}   = require('../../authentication')

router.get('/api/profile/:username', async (req, res) => {
    const username = req.params.username;
    const mode = req.query.mode || 'vanilla';

    try {
        const query = `
            SELECT users.username, user_profiles.*
            FROM users
            JOIN user_profiles ON users.id = user_profiles.user_id
            WHERE users.username = ?
        `;

        const [rows] = await pool.query(query, [username]);

        if (rows.length === 0) {
            return res.status(404).json({ error: 'Profile not found' });
        }

        const userProfile = rows[0];
        const territories = {
            vanilla: ['North America', 'South America', 'Europe', 'Africa', 'Asia', 'Russia'],
            '8player': ['North America', 'South America', 'Europe', 'Africa', 'East Asia', 'West Asia', 'Russia', 'Australasia'],
            '10player': ['North America', 'South America', 'Europe', 'North Africa', 'South Africa', 'Russia', 'East Asia', 'South Asia', 'Australasia', 'Antartica']
        };

        const validTerritories = territories[mode] || territories.vanilla;
        const responseData = {
            ...userProfile,
            winLossRatio: 'Not enough data',
            totalGames: 0,
            wins: 0,
            losses: 0,
            favoriteMods: [],
            main_contributions: userProfile.main_contributions ? userProfile.main_contributions.split(',') : [],
            guides_and_mods: userProfile.guides_and_mods ? userProfile.guides_and_mods.split(',') : [],
            record_score: userProfile.record_score || 0,
            territoryStats: {
                bestTerritory: 'N/A',
                worstTerritory: 'N/A'
            }
        };

        if (userProfile.defcon_username) {
            const gamesQuery = `SELECT players, duration FROM demos WHERE players LIKE ?`;
            const [games] = await pool.query(gamesQuery, [`%${userProfile.defcon_username}%`]);

            let territoryStats = {};
            let highestScore = userProfile.record_score || 0;
            let totalDuration = 0;
            let gamesWithDuration = 0;

            games.forEach(game => {
                try {

                    const parsedData = JSON.parse(game.players);
                    const playersArray = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                    const userPlayer = playersArray.find(p => p.name === userProfile.defcon_username);

                    if (game.duration) {
                        const [hours, minutes] = game.duration.split(':').map(Number);
                        const durationInMinutes = (hours * 60) + minutes;
                        totalDuration += durationInMinutes;
                        gamesWithDuration++;
                    }

                    if (userPlayer && validTerritories.includes(userPlayer.territory)) {
                        const usingAlliances = userPlayer.alliance !== undefined;
                        const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;
                        const scores = {};
                        playersArray.forEach(player => {
                            const groupId = usingAlliances ? player.alliance : player.team;
                            scores[groupId] = (scores[groupId] || 0) + player.score;
                        });

                        const userGroupScore = scores[userGroupId] || 0;
                        const userGroupIsWinner = Object.entries(scores)
                            .every(([groupId, score]) => groupId === userGroupId.toString() || score <= userGroupScore);

                        const territory = userPlayer.territory;
                        territoryStats[territory] = territoryStats[territory] || { wins: 0, losses: 0 };

                        if (userGroupIsWinner) {
                            responseData.wins++;
                            territoryStats[territory].wins++;
                        } else {
                            responseData.losses++;
                            territoryStats[territory].losses++;
                        }

                        responseData.totalGames++;
                        highestScore = Math.max(highestScore, userPlayer.score);
                    }
                } catch (err) {
                    console.error('Error processing game data:', err);
                }
            });

            responseData.avgGameDuration = gamesWithDuration > 0 ? totalDuration / gamesWithDuration : 0;

            if (responseData.totalGames > 0) {
                responseData.winLossRatio = (responseData.wins / responseData.totalGames).toFixed(2);
            }

            let bestRatio = -1;
            let worstRatio = 2;

            Object.entries(territoryStats).forEach(([territory, stats]) => {
                const total = stats.wins + stats.losses;
                if (total >= 3) {
                    const ratio = stats.wins / total;

                    if (ratio > bestRatio) {
                        bestRatio = ratio;
                        responseData.territoryStats.bestTerritory = territory;
                    }

                    if (ratio < worstRatio) {
                        worstRatio = ratio;
                        responseData.territoryStats.worstTerritory = territory;
                    }
                }
            });

            if (highestScore > responseData.record_score) {
                await pool.query(
                    'UPDATE user_profiles SET record_score = ? WHERE user_id = ?',
                    [highestScore, userProfile.user_id]
                );
                responseData.record_score = highestScore;
            }
        }

        if (userProfile.favorites) {
            const favoriteModIds = userProfile.favorites.split(',').filter(id => id);
            if (favoriteModIds.length > 0) {
                const [mods] = await pool.query(
                    'SELECT * FROM modlist WHERE id IN (?)',
                    [favoriteModIds]
                );
                responseData.favoriteMods = mods;
            }
        }

        res.json(responseData);

    } catch (error) {
        console.error('Error fetching profile:', error);
        res.status(500).json({ error: 'Internal server error' });
    }
});

router.get('/api/profile-arch-nemesis', async (req, res) => {
    try {
        const { playerName } = req.query;

        if (!playerName) {
            return res.status(400).json({ error: 'Player name is required' });
        }

        const [demos] = await pool.query(
            `SELECT * FROM demos 
         WHERE player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ? OR 
               player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ? OR 
               player9_name = ? OR player10_name = ?
         ORDER BY date DESC`,
            Array(10).fill(playerName)
        );

        const playerInteractions = {};

        for (const demo of demos) {
            try {

                let playersData = [];

                if (demo.players) {
                    let parsedData;

                    try {
                        parsedData = JSON.parse(demo.players);
                    } catch (parseError) {
                        console.error('JSON parse error:', parseError);
                        continue;
                    }

                    if (parsedData && typeof parsedData === 'object') {

                        if (Array.isArray(parsedData.players)) {
                            playersData = parsedData.players;
                        } else if (Array.isArray(parsedData)) {
                            playersData = parsedData;
                        }
                    }
                }

                if (playersData.length < 2) continue;

                const userPlayer = playersData.find(p => p.name === playerName);
                if (!userPlayer) continue;

                const usingAlliances = playersData.some(p => p.alliance !== undefined);
                const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;

                if (userGroupId === undefined) continue;

                const groupScores = {};
                playersData.forEach(player => {
                    const groupId = usingAlliances ? player.alliance : player.team;
                    if (groupId === undefined) return;

                    if (!groupScores[groupId]) {
                        groupScores[groupId] = 0;
                    }
                    groupScores[groupId] += player.score || 0;
                });

                const sortedGroups = Object.entries(groupScores).sort((a, b) => b[1] - a[1]);
                const isTie = sortedGroups.length >= 2 && sortedGroups[0][1] === sortedGroups[1][1];

                let winningGroupId = null;
                if (!isTie && sortedGroups.length > 0) {
                    winningGroupId = Number(sortedGroups[0][0]);
                }

                const userTeamWon = winningGroupId !== null && userGroupId === winningGroupId;

                playersData.forEach(opponent => {
                    if (opponent.name === playerName) return;

                    const opponentGroupId = usingAlliances ? opponent.alliance : opponent.team;
                    if (opponentGroupId === undefined) return;


                    if (!playerInteractions[opponent.name]) {
                        playerInteractions[opponent.name] = {
                            games: 0,
                            wins: 0,
                            losses: 0,
                            sameTeam: 0,
                            ties: 0,
                            otherOutcomes: 0
                        };
                    }

                    playerInteractions[opponent.name].games++;

                    if (opponentGroupId === userGroupId) {

                        playerInteractions[opponent.name].sameTeam++;
                    } else if (isTie) {

                        playerInteractions[opponent.name].ties++;
                    } else if (userTeamWon) {

                        playerInteractions[opponent.name].wins++;
                    } else if (opponentGroupId === winningGroupId) {

                        playerInteractions[opponent.name].losses++;
                    } else {

                        playerInteractions[opponent.name].otherOutcomes++;
                    }
                });

            } catch (error) {
                console.error('Error processing demo for arch nemesis:', error);
            }
        }

        let archNemesis = null;
        let maxGames = 0;
        let nemesisWins = 0;
        let nemesisLosses = 0;
        let totalGames = 0;
        let sameTeamGames = 0;
        let tieGames = 0;
        let otherOutcomes = 0;

        Object.entries(playerInteractions).forEach(([opponent, record]) => {

            if (record.games >= 3 && record.games > maxGames) {
                archNemesis = opponent;
                maxGames = record.games;
                nemesisWins = record.wins;
                nemesisLosses = record.losses;
                totalGames = record.games;
                sameTeamGames = record.sameTeam;
                tieGames = record.ties;
                otherOutcomes = record.otherOutcomes;
            }
        });

        res.json({
            playerName,
            archNemesis: archNemesis || 'None yet',
            gamesPlayed: totalGames,
            userWins: nemesisWins,
            userLosses: nemesisLosses,
            sameTeamGames,
            tieGames,
            otherOutcomes,
            debug: {
                recordedGames: demos.length,
                trackedInteractions: Object.keys(playerInteractions).length
            }
        });

    } catch (error) {
        console.error('Error calculating arch nemesis:', error);
        res.status(500).json({ error: 'Unable to calculate arch nemesis', details: error.message });
    }
});

router.get('/api/recent-game/:username', async (req, res) => {
    const { username } = req.params;

    try {
        const query = `
        SELECT * FROM demos
        WHERE player1_name = ? OR player2_name = ? OR player3_name = ? OR player4_name = ?
              OR player5_name = ? OR player6_name = ? OR player7_name = ? OR player8_name = ?
              OR player9_name = ? OR player10_name = ?
        ORDER BY date DESC
        LIMIT 1
      `;
        const [recentGame] = await pool.query(query, Array(10).fill(username));

        if (recentGame.length === 0) {
            return res.status(404).json({ error: 'No recent games found' });
        }

        res.json(recentGame[0]);
    } catch (error) {
        console.error('Error fetching recent game:', error);
        res.status(500).json({ error: 'Unable to fetch recent game' });
    }
});



router.post('/api/update-profile', authenticateToken, async (req, res) => {
    const userId = req.user.id;
    const {
        discord_username,
        steam_id,
        bio,
        defcon_username,
        years_played,
        main_contributions,
        guides_and_mods
    } = req.body;

    try {
        await pool.query(`
            UPDATE user_profiles 
            SET discord_username = ?, 
                steam_id = ?,
                bio = ?,
                defcon_username = ?, 
                years_played = ?,
                main_contributions = ?,
                guides_and_mods = ?
            WHERE user_id = ?
        `, [
            discord_username,
            steam_id,
            bio,
            defcon_username,
            years_played,
            main_contributions.join(','),
            guides_and_mods.join(','),
            userId
        ]);

        res.json({ success: true, message: 'Profile updated successfully' });
    } catch (error) {
        console.error('Error updating profile:', error);
        res.status(500).json({ success: false, message: 'Failed to update profile' });
    }
});



router.post('/api/upload-profile-image', upload.single('image'), async (req, res) => {
    if (!req.file) {
        console.error('No file uploaded');
        return res.status(400).json({ success: false, error: 'No file uploaded' });
    }

    const userId = req.user.id;
    const imageType = req.body.type;

    try {
        const fileExtension = path.extname(req.file.originalname);
        const newFileName = `${userId}_${imageType}${fileExtension}`;
        const newFilePath = path.join('public', 'uploads', newFileName);

        fs.renameSync(req.file.path, newFilePath);

        const imageUrl = `/uploads/${newFileName}`;

        const updateField = imageType === 'profile' ? 'profile_picture' : 'banner_image';
        await pool.query(`UPDATE user_profiles SET ${updateField} = ? WHERE user_id = ?`, [imageUrl, userId]);

        console.log(`Image uploaded and database updated for user ${userId}, type: ${imageType}`);
        res.json({ success: true, imageUrl: imageUrl });
    } catch (error) {
        console.error('Error updating profile image:', error);
        res.status(500).json({ success: false, error: 'Failed to update profile image' });
    }
});

module.exports = router;