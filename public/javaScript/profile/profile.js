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
//Last Edited 14-04-2025

import { formatDurationProfile } from '../main/main.js';
import { vanillaTerritories, eightPlayerTerritories, tenPlayerTerritories, territoryImages } from './constants.js';
import { createModCard } from './modcard.js';
import { initializeProfileEditing, initializeProfileImageEditing } from './profileedit.js';
import { createDemoCard } from '../demos/democard.js';
import { toggleSpectators } from '../demos/democard.js';
import { showReportOptions } from '../main/reporting.js';

window.toggleSpectators = toggleSpectators;
window.showReportOptions = showReportOptions;

document.addEventListener('DOMContentLoaded', async () => {
    const pathArray = window.location.pathname.split('/');
    const username = pathArray[pathArray.length - 1];

    let currentTerritories = vanillaTerritories;

    try {
        let userProfile = await fetchProfileData(username);

        document.title = `${userProfile.username}'s Profile - DEFCON Expanded`;

        const currentUserResponse = await fetch('/api/current-user');
        const currentUserData = await currentUserResponse.json();

        const isOwnProfile = currentUserData.user && currentUserData.user.username === username;

        const editButton = document.getElementById('edit-profile-btn');
        const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
        if (editButton) {
            editButton.style.display = isOwnProfile ? 'block' : 'none';
        }
        if (editButtonMobile) {
            editButtonMobile.style.display = isOwnProfile ? 'block' : 'none';
        }

        const profilePictureElement = document.getElementById('profile-picture');
        if (profilePictureElement) {
            const profilePicture = userProfile.profile_picture
                ? `${userProfile.profile_picture}`  
                : '/images/icon3.png';
            
            profilePictureElement.src = profilePicture;
        }

        const profileBanner = document.getElementById('profile-banner');
        if (profileBanner && userProfile.banner_image) {
            profileBanner.style.backgroundImage = `url("${userProfile.banner_image}")`;
        } else if (profileBanner) {
            profileBanner.style.backgroundImage = 'url("/images/backgroundprofile.png")';
        }

        const usernameLabel = document.getElementById('profile-username-label');
        if (usernameLabel) {
            usernameLabel.textContent = userProfile.username || 'N/A';
        }

        const bioText = document.getElementById('bio-text');
        if (bioText) {
            bioText.textContent = userProfile.bio || 'No bio.';
        }

        const discordInfoMobile = document.getElementById('discord-info-mobile');
        const steamInfoMobile = document.getElementById('steam-info-mobile');
        const discordInfoDesktop = document.getElementById('discord-info');
        const steamInfoDesktop = document.getElementById('steam-info');

        if (discordInfoMobile) discordInfoMobile.textContent = userProfile.discord_username || 'N/A';
        if (steamInfoMobile) steamInfoMobile.textContent = userProfile.steam_id || 'N/A';

        if (discordInfoDesktop) discordInfoDesktop.textContent = userProfile.discord_username || 'N/A';
        if (steamInfoDesktop) steamInfoDesktop.textContent = userProfile.steam_id || 'N/A';

        const defconUsername = document.getElementById('defcon-username');
        const yearsOfService = document.getElementById('years-of-service');
        const winLossRatio = document.getElementById('win-loss-ratio');
        const totalGames = document.getElementById('total-games');
        const recordScore = document.getElementById('record-score');
        const averageGameDuration = document.getElementById('average-game-duration');
        const archNemesisName = document.getElementById('arch-nemesis-name');

        if (defconUsername) defconUsername.textContent = userProfile.defcon_username || 'N/A';
        if (yearsOfService) yearsOfService.textContent = userProfile.years_played || '0';
        if (winLossRatio) winLossRatio.textContent = userProfile.winLossRatio || 'Not enough data';
        if (totalGames) totalGames.textContent = userProfile.totalGames || '0';
        if (recordScore) recordScore.textContent = userProfile.record_score || '0';

        if (averageGameDuration) {
            const avgDuration = userProfile.avgGameDuration || 0;
            averageGameDuration.textContent = formatDurationProfile(avgDuration);
        }

        if (archNemesisName) {
            archNemesisName.textContent = userProfile.archNemesis || 'None yet';
        }

        const totalNemesisGames = document.getElementById('total-nemesis-games');
        const nemesisWinsElement = document.getElementById('nemesis-wins');
        const nemesisLossesElement = document.getElementById('nemesis-losses');

        if (totalNemesisGames) {
            const competitiveGames = (userProfile.nemesisWins || 0) + (userProfile.nemesisLosses || 0);
            totalNemesisGames.textContent = `Games: ${competitiveGames}`;
        }

        if (nemesisWinsElement) {
            nemesisWinsElement.textContent = `W: ${userProfile.nemesisWins || 0}`;
        }

        if (nemesisLossesElement) {
            nemesisLossesElement.textContent = `L: ${userProfile.nemesisLosses || 0}`;
        }

        const nemesisContainer = document.querySelector('.nemesis-container');
        if (nemesisContainer && userProfile.nemesisGames > 0) {
            const sameTeam = userProfile.sameTeamGames || 0;
            const ties = userProfile.tieGames || 0;
            const other = userProfile.otherOutcomes || 0;

            nemesisContainer.title =
                `Games vs ${userProfile.archNemesis}: ${userProfile.nemesisGames}\n` +
                `Wins: ${userProfile.nemesisWins}\n` +
                `Losses: ${userProfile.nemesisLosses}\n` +
                `Same Team: ${sameTeam}\n` +
                `Ties: ${ties}\n` +
                `Other outcomes: ${other}`;
        }

        const mainContributionsContainer = document.getElementById('main-contributions-list');
        if (mainContributionsContainer) {
            mainContributionsContainer.innerHTML = '';
            if (userProfile.main_contributions && userProfile.main_contributions.length > 0) {
                userProfile.main_contributions.forEach(contribution => {
                    const listItem = document.createElement('li');
                    listItem.textContent = contribution.trim();
                    mainContributionsContainer.appendChild(listItem);
                });
            } else {
                const listItem = document.createElement('li');
                listItem.textContent = 'No main contributions yet.';
                mainContributionsContainer.appendChild(listItem);
            }
        }

        const guidesAndModsContainer = document.getElementById('guides-and-mods-list');
        if (guidesAndModsContainer) {
            guidesAndModsContainer.innerHTML = '';
            if (userProfile.guides_and_mods && userProfile.guides_and_mods.length > 0) {
                userProfile.guides_and_mods.forEach(guide => {
                    const listItem = document.createElement('li');
                    listItem.textContent = guide.trim();
                    guidesAndModsContainer.appendChild(listItem);
                });
            } else {
                const listItem = document.createElement('li');
                listItem.textContent = 'No guides or mods yet.';
                guidesAndModsContainer.appendChild(listItem);
            }
        }

        const favModsContainer = document.getElementById('favouriteModsContainer');
        if (favModsContainer) {
            favModsContainer.innerHTML = '';

            if (userProfile.favoriteMods && userProfile.favoriteMods.length > 0) {
                userProfile.favoriteMods.forEach(mod => {
                    const modElement = createModCard(mod);
                    favModsContainer.innerHTML += modElement;
                });
            }
        }

        const dropdown = document.getElementById('game-mode-dropdown');
        if (dropdown) {
            dropdown.addEventListener('change', async (event) => {
                const selectedMode = event.target.value;
                if (selectedMode === '8player') {
                    currentTerritories = eightPlayerTerritories;
                } else if (selectedMode === '10player') {
                    currentTerritories = tenPlayerTerritories;
                } else {
                    currentTerritories = vanillaTerritories;
                }

                userProfile = await fetchProfileData(username, selectedMode);
                updateTerritoryStats(userProfile, currentTerritories);
            });
        }

        updateTerritoryStats(userProfile, currentTerritories);

        if (userProfile.defcon_username) {
            await loadRecentGame(userProfile.defcon_username);
        }

        if (isOwnProfile) {
            initializeProfileEditing();
            initializeProfileImageEditing();
        }

    } catch (error) {
        console.error('Error loading profile:', error);
        document.title = 'Profile Not Found - DEFCON Expanded';
    }
});

function updateTerritoryStats(userProfile, territories) {
    const bestTerritoryElement = document.getElementById('best-territory-username');
    const worstTerritoryElement = document.getElementById('worst-territory-username');

    if (bestTerritoryElement) {
        bestTerritoryElement.textContent = `${userProfile.username}'s best territory is: ${userProfile.territoryStats?.bestTerritory || 'N/A'}`;
    }

    if (worstTerritoryElement) {
        worstTerritoryElement.textContent = `${userProfile.username}'s worst territory is: ${userProfile.territoryStats?.worstTerritory || 'N/A'}`;
    }

    const mapContainer = document.querySelector('.heatmap');
    if (mapContainer) {
        mapContainer.innerHTML = `<img src="/images/base_map.png" class="base-map" alt="Base Map" style="width: 100%;">`;

        const bestTerritory = userProfile.territoryStats?.bestTerritory;
        const worstTerritory = userProfile.territoryStats?.worstTerritory;

        if (territories[bestTerritory]) {
            mapContainer.innerHTML += `<img src="/images/${territories[bestTerritory]}00bf00.png" class="territory-overlay" alt="${bestTerritory}" style="position: absolute; top: 0; left: 0; width: 100%;">`;
        }

        if (territories[worstTerritory]) {
            mapContainer.innerHTML += `<img src="/images/${territories[worstTerritory]}ff4949.png" class="territory-overlay" alt="${worstTerritory}" style="position: absolute; top: 0; left: 0; width: 100%;">`;
        }
    }
}

async function fetchProfileData(username, mode = 'vanilla') {
    try {
        const response = await fetch(`/api/profile/${username}?mode=${mode}`);
        if (!response.ok) {
            throw new Error('Profile not found');
        }
        const userProfile = await response.json();

        if (userProfile.defcon_username) {
            const nemesisData = await fetchNemesisData(userProfile.defcon_username);
            userProfile.archNemesis = nemesisData.archNemesis;
            userProfile.nemesisWins = nemesisData.userWins;
            userProfile.nemesisLosses = nemesisData.userLosses;
            userProfile.nemesisGames = nemesisData.gamesPlayed;
        }

        return userProfile;
    } catch (error) {
        console.error('Error fetching profile data:', error);
        throw error;
    }
}

async function fetchNemesisData(defconUsername) {
    try {
        const response = await fetch(`/api/profile-arch-nemesis?playerName=${defconUsername}`);
        if (!response.ok) {
            throw new Error('Failed to fetch arch nemesis data');
        }
        return await response.json();
    } catch (error) {
        console.error('Error fetching arch nemesis data:', error);
        return {
            archNemesis: 'None yet',
            gamesPlayed: 0,
            userWins: 0,
            userLosses: 0,
            sameTeamGames: 0,
            tieGames: 0,
            otherOutcomes: 0
        };
    }
}

async function calculateNemesisData(defconUsername) {
    try {
        const response = await fetch(`/api/demo-profile-panel?playerName=${defconUsername}`);
        if (!response.ok) {
            throw new Error('Failed to fetch games for nemesis calculation');
        }

        const { demos } = await response.json();
        const playerInteractions = {};

        demos.forEach(demo => {
            try {
                let playersData = [];

                if (demo.players) {
                    const parsedData = JSON.parse(demo.players);
                    playersData = Array.isArray(parsedData.players) ? parsedData.players :
                        (Array.isArray(parsedData) ? parsedData : []);
                }

                if (playersData.length < 2) return;
                const userPlayer = playersData.find(p => p.name === defconUsername);
                if (!userPlayer) return;

                const usingAlliances = playersData.some(p => p.alliance !== undefined);
                const userGroupId = usingAlliances ? userPlayer.alliance : userPlayer.team;

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

                if (isTie) return;

                const winningGroupId = Number(sortedGroups[0][0]);
                const didUserWin = userGroupId === winningGroupId;

                playersData.forEach(opponent => {
                    if (opponent.name === defconUsername) return;

                    const opponentGroupId = usingAlliances ? opponent.alliance : opponent.team;
                    if (opponentGroupId === undefined) return;

                    if (!playerInteractions[opponent.name]) {
                        playerInteractions[opponent.name] = { wins: 0, losses: 0 };
                    }

                    if (didUserWin && opponentGroupId !== userGroupId) {
                        playerInteractions[opponent.name].wins++;
                    } else if (!didUserWin && opponentGroupId === winningGroupId) {
                        playerInteractions[opponent.name].losses++;
                    }
                });

            } catch (error) {
                console.error('Error processing demo for nemesis:', error);
            }
        });

        let nemesis = null;
        let maxLosses = 0;
        let nemesisWins = 0;

        Object.entries(playerInteractions).forEach(([opponent, record]) => {
            const totalGames = record.wins + record.losses;
            if (totalGames >= 3 && record.losses > maxLosses) {
                nemesis = opponent;
                maxLosses = record.losses;
                nemesisWins = record.wins;
            }
        });

        return {
            nemesis: nemesis || 'None yet',
            nemesisWins: nemesisWins,
            nemesisLosses: maxLosses,
            allOpponents: playerInteractions
        };

    } catch (error) {
        console.error('Error calculating nemesis data:', error);
        return {
            nemesis: 'None yet',
            nemesisWins: 0,
            nemesisLosses: 0,
            allOpponents: {}
        };
    }
}

async function loadRecentGame(defconUsername) {
    try {
        const response = await fetch(`/api/demo-profile-panel?playerName=${defconUsername}`);
        if (!response.ok) {
            throw new Error('Failed to fetch recent games');
        }

        const { demos } = await response.json();
        const recentGameContainer = document.getElementById('recent-game-container');

        if (recentGameContainer) {
            if (demos && demos.length > 0) {
                const demoCard = createDemoCard(demos[0]);
                recentGameContainer.innerHTML = demoCard.outerHTML;
            } else {
                recentGameContainer.innerHTML = '<p>No recent games available.</p>';
            }
        }

        const cleanUrl = `${window.location.origin}${window.location.pathname}`;
        window.history.pushState({}, '', cleanUrl);

    } catch (error) {
        console.error('Error loading recent game:', error);
    }
}

export {
    updateTerritoryStats,
    fetchProfileData,
    fetchNemesisData,
    calculateNemesisData,
    loadRecentGame
};