// DefconExpanded, Created by...
// KezzaMcFezza - Main Developer
// Nexustini - Server Management
//
// Notable Mentions...
// Rad - For helping with python scripts.
// Bert_the_turtle - Doing everything with c++
//
// Inspired by Sievert and Wan May
// 
// Last Edited 14-04-2025

import { leaderboardFilters, defaultNames, serverPlaylists } from './constants.js';
import { displayLeaderboard, updateLeaderboardMetadata, displayMostActivePlayers, displayPreviousSeasonWinners } from './ui.js';

export async function fetchLeaderboard() {
    try {
        const queryParams = new URLSearchParams();

        Object.entries(leaderboardFilters).forEach(([key, value]) => {
            if (value) queryParams.set(key, value);
        });

        if (leaderboardFilters.playlist && serverPlaylists[leaderboardFilters.playlist]) {
            queryParams.delete('playlist');
            queryParams.set('serverList', serverPlaylists[leaderboardFilters.playlist].servers.join(','));
        }

        queryParams.set('excludeNames', defaultNames.join(','));

        queryParams.set('_t', Date.now());

        const tableBody = document.querySelector('#leaderboard-table tbody');
        if (tableBody) {
            tableBody.innerHTML = '<tr><td colspan="6" class="loading-row"><i class="fas fa-circle-notch fa-spin"></i> Loading leaderboard data...</td></tr>';
        }

        const response = await fetch(`/api/leaderboard?${queryParams.toString()}`);
        const data = await response.json();

        displayLeaderboard(data.leaderboard);
        updateLeaderboardMetadata(data);
    } catch (error) {
        console.error('Error fetching leaderboard:', error);

        const tableBody = document.querySelector('#leaderboard-table tbody');
        if (tableBody) {
            tableBody.innerHTML = '<tr><td colspan="6" class="error-row">Error loading leaderboard data. Please try again later.</td></tr>';
        }
    }
}

export async function fetchAllTimeMostActivePlayers() {
    try {
        const queryParams = new URLSearchParams();
        queryParams.set('excludeNames', defaultNames.join(','));

        const response = await fetch(`/api/most-active-players?${queryParams.toString()}`);
        const activePlayers = await response.json();

        displayMostActivePlayers(activePlayers);
    } catch (error) {
        console.error('Error fetching all-time most active players:', error);
    }
}

export async function fetchPreviousSeasonWinners() {
    try {
        const pastSeasons = [];
        const currentSeason = await getCurrentSeasonFromAPI();

        const today = new Date();
        const allSeasons = await getAllSeasonsFromAPI();
        
        for (const [key, season] of Object.entries(allSeasons)) {
            const endDate = new Date(season.endDate);
            if (endDate < today && key !== currentSeason.key) {
                pastSeasons.push({ key, ...season });
            }
        }

        if (pastSeasons.length === 0) {
            document.getElementById('previous-seasons-section').style.display = 'none';
            return;
        }

        const seasonWinners = [];

        for (const season of pastSeasons) {
            const queryParams = new URLSearchParams({
                startDate: season.startDate,
                endDate: season.endDate,
                sortBy: 'wins',
                limit: 1,
                excludeNames: defaultNames.join(','),
                includeDetailedStats: 'true'
            });

            const response = await fetch(`/api/leaderboard?${queryParams.toString()}`);
            const data = await response.json();

            if (data.leaderboard && data.leaderboard.length > 0) {
                const winner = data.leaderboard[0];

                const nemesisParams = new URLSearchParams({
                    playerName: winner.player_name,
                    startDate: season.startDate,
                    endDate: season.endDate,
                    getNemesisData: 'true'
                });

                const nemesisResponse = await fetch(`/api/player-nemesis?${nemesisParams.toString()}`);
                const nemesisData = await nemesisResponse.json();

                seasonWinners.push({
                    season: season.displayName,
                    playerName: winner.player_name,
                    wins: winner.wins,
                    losses: winner.losses,
                    gamesPlayed: winner.games_played,
                    avgScore: Math.round(winner.avg_score),
                    winRatio: (winner.win_ratio).toFixed(1),
                    archNemesis: nemesisData.nemesis || "None",
                    profileUrl: winner.profileUrl
                });
            }
        }

        displayPreviousSeasonWinners(seasonWinners);

    } catch (error) {
        console.error('Error fetching previous season winners:', error);
        document.getElementById('previous-seasons-section').style.display = 'none';
    }
}

async function getCurrentSeasonFromAPI() {
    try {
        const { getCurrentSeason } = await import('./seasons.js');
        return getCurrentSeason();
    } catch (error) {
        console.error('Error getting current season:', error);
        return {};
    }
}

async function getAllSeasonsFromAPI() {
    try {
        const { getAllSeasons } = await import('./seasons.js');
        return getAllSeasons();
    } catch (error) {
        console.error('Error getting all seasons:', error);
        return {};
    }
}