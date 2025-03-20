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
//Last Edited 03-03-2025

document.addEventListener('DOMContentLoaded', async () => {
    const pathArray = window.location.pathname.split('/');
    const username = pathArray[pathArray.length - 1];

    const vanillaTerritories = {
        'North America': 'northamerica',
        'South America': 'southamerica',
        'Europe': 'europe',
        'Africa': 'africa',
        'Asia': 'asia',
        'Russia': 'russia',
    };

    const eightPlayerTerritories = {
        'North America': 'northamerica',
        'South America': 'southamerica',
        'Europe': 'europe',
        'Africa': 'africa',
        'East Asia': 'eastasia',
        'West Asia': 'westasia',
        'Russia': 'russia',
        'Australasia': 'australasia',
    };

    const tenPlayerTerritories = {
        'North America': 'northamerica',
        'South America': 'southamerica',
        'Europe': 'europe',
        'North Africa': 'northafrica',
        'South Africa': 'southafrica',
        'Russia': 'russia',
        'East Asia': 'eastasia',
        'South Asia': 'southasia',
        'Australasia': 'australasia',
        'Antartica': 'antartica',
    };

    let currentTerritories = vanillaTerritories;

    function formatDuration(minutes) {
        if (!minutes || isNaN(minutes)) return '0 min';

        if (minutes < 60) {
            return `${Math.round(minutes)} min`;
        } else {
            const hours = Math.floor(minutes / 60);
            const remainingMinutes = Math.round(minutes % 60);
            return `${hours} hr ${remainingMinutes} min`;
        }
    }

    function formatGameDuration(duration) {
        if (!duration) return 'Unknown';
        const [hours, minutes] = duration.split(':').map(Number);
        if (hours === 0) {
            return `${minutes} min`;
        } else {
            return `${hours} hr ${minutes} min`;
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
                ? `https://defconexpanded.com${userProfile.profile_picture}`
                : '/images/icon3.png';
            profilePictureElement.src = profilePicture;
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
            averageGameDuration.textContent = formatDuration(avgDuration);
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
                let currentTerritories = vanillaTerritories;
                if (selectedMode === '8player') {
                    currentTerritories = eightPlayerTerritories;
                } else if (selectedMode === '10player') {
                    currentTerritories = tenPlayerTerritories;
                }

                userProfile = await fetchProfileData(username, selectedMode);

                updateTerritoryStats(userProfile, currentTerritories);
            });
        }

        updateTerritoryStats(userProfile, currentTerritories);

        if (userProfile.defcon_username) {
            await loadRecentGame(userProfile.defcon_username);
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
        bestTerritoryElement.textContent = `${userProfile.username}'s best territory is: ${userProfile.territoryStats.bestTerritory || 'N/A'}`;
    }

    if (worstTerritoryElement) {
        worstTerritoryElement.textContent = `${userProfile.username}'s worst territory is: ${userProfile.territoryStats.worstTerritory || 'N/A'}`;
    }

    const mapContainer = document.querySelector('.heatmap');
    if (mapContainer) {
        mapContainer.innerHTML = `<img src="/images/base_map.png" class="base-map" alt="Base Map" style="width: 100%;">`;

        const bestTerritory = userProfile.territoryStats.bestTerritory;
        const worstTerritory = userProfile.territoryStats.worstTerritory;

        if (territories[bestTerritory]) {
            mapContainer.innerHTML += `<img src="/images/${territories[bestTerritory]}00bf00.png" class="territory-overlay" alt="${bestTerritory}" style="position: absolute; top: 0; left: 0; width: 100%;">`;
        }

        if (territories[worstTerritory]) {
            mapContainer.innerHTML += `<img src="/images/${territories[worstTerritory]}ff4949.png" class="territory-overlay" alt="${worstTerritory}" style="position: absolute; top: 0; left: 0; width: 100%;">`;
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    const editButton = document.getElementById('edit-profile-btn');
    const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
    const profilePictureContainer = document.getElementById('profile-picture-container');
    const profileBannerOverlay = document.getElementById('profile-banner-overlay');
    const profilePictureOverlay = document.getElementById('profile-picture-overlay');
    const imageEditorModal = document.getElementById('image-editor-modal');
    const cropOverlay = document.getElementById('crop-overlay');
    const zoomControl = document.getElementById('zoom-control');
    const skipEditBtn = document.getElementById('skip-edit');
    const cancelEditBtn = document.getElementById('cancel-edit');
    const imageUpload = document.getElementById('image-upload');
    const imageToEdit = document.getElementById('image-to-edit');
    const applyEditBtn = document.getElementById('apply-edit');

    let isEditing = false;
    let currentEditingElement = null;
    let isProfilePicture = false;
    let scale = 1;
    let translateX = 0;
    let translateY = 0;

    if (profileBannerOverlay && profilePictureOverlay) {
        profileBannerOverlay.style.display = 'none';
        profilePictureOverlay.style.display = 'none';
    }

    if (editButton) {
        editButton.addEventListener('click', () => {
            isEditing = !isEditing;
            if (editButton) {
                editButton.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
                editButton.innerHTML = isEditing ?
                    '<i class="fas fa-save"></i> Save Changes' :
                    '<i class="fas fa-pencil-alt"></i> Edit Profile';
            }
            if (editButtonMobile) {
                editButtonMobile.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
                editButtonMobile.innerHTML = isEditing ?
                    '<i class="fas fa-save"></i> Save Changes' :
                    '<i class="fas fa-pencil-alt"></i> Edit Profile';
            }
            toggleEditMode();
        });
    }

    function toggleEditMode() {
        document.body.classList.toggle('editing');
        if (editButton) editButton.classList.toggle('editing');

        if (profileBannerOverlay) profileBannerOverlay.style.display = isEditing ? 'flex' : 'none';
        if (profilePictureOverlay) profilePictureOverlay.style.display = isEditing ? 'flex' : 'none';
    }

    function showImageEditor(imageElement, isProfile) {
        currentEditingElement = imageElement;
        isProfilePicture = isProfile;
        imageToEdit.onload = () => {
            imageEditorModal.style.display = 'block';
            resetZoom();
            updateCropOverlay();
            centerImage();
        };
        imageToEdit.src = `${imageUrl}?t=${new Date().getTime()}`;
    }

    function centerImage() {
        const containerWidth = imageToEdit.parentElement.offsetWidth;
        const containerHeight = imageToEdit.parentElement.offsetHeight;
        const imgWidth = imageToEdit.naturalWidth * scale;
        const imgHeight = imageToEdit.naturalHeight * scale;

        translateX = (containerWidth - imgWidth) / 2;
        translateY = (containerHeight - imgHeight) / 2;

        applyTransform();
    }

    if (imageToEdit) {
        imageToEdit.addEventListener('wheel', (e) => {
            e.preventDefault();
            const delta = e.deltaY > 0 ? -0.1 : 0.1;
            const newZoom = Math.min(Math.max(zoomControl.value + delta * 100, zoomControl.min), zoomControl.max);
            zoomControl.value = newZoom;
            applyZoom();
        });
    }

    function resetZoom() {
        scale = 1;
        if (zoomControl) zoomControl.value = 100;
        translateX = 0;
        translateY = 0;
        applyTransform();
    }

    function applyZoom() {
        const newScale = zoomControl.value / 100;
        const deltaScale = newScale - scale;

        const containerWidth = imageToEdit.parentElement.offsetWidth;
        const containerHeight = imageToEdit.parentElement.offsetHeight;
        translateX -= (containerWidth / 2 - translateX) * deltaScale / newScale;
        translateY -= (containerHeight / 2 - translateY) * deltaScale / newScale;

        scale = newScale;
        applyTransform();
    }

    function updateCropOverlay() {
        if (!cropOverlay) return;

        const profilePictureDimensions = 480;
        if (isProfilePicture) {
            cropOverlay.style.borderRadius = '50%';
            cropOverlay.style.width = `${profilePictureDimensions}px`;
            cropOverlay.style.height = `${profilePictureDimensions}px`;
        } else {
            cropOverlay.style.borderRadius = '0';
            cropOverlay.style.width = '100%';
            cropOverlay.style.height = '33.33%';
        }
    }

    if (profilePictureContainer) {
        profilePictureContainer.addEventListener('click', (e) => {
            if (isEditing && e.target.closest('.edit-overlay')) {
                currentEditingElement = document.getElementById('profile-picture');
                isProfilePicture = true;
                imageUpload.click();
            }
        });
    }

    if (imageUpload) {
        imageUpload.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (file) {
                const reader = new FileReader();
                reader.onload = (e) => {
                    imageToEdit.src = e.target.result;
                    showImageEditor(currentEditingElement, isProfilePicture);
                };
                reader.readAsDataURL(file);
            }
        });
    }

    if (zoomControl) {
        zoomControl.max = 400;
        zoomControl.addEventListener('input', applyZoom);
    }

    function applyTransform() {
        if (!imageToEdit) return;
        imageToEdit.style.transform = `translate(-50%, -50%) scale(${scale})`;
        imageToEdit.style.left = '50%';
        imageToEdit.style.top = '50%';
    }

    if (skipEditBtn) {
        skipEditBtn.addEventListener('click', () => {
            if (imageEditorModal) imageEditorModal.style.display = 'none';
        });
    }

    if (cancelEditBtn) {
        cancelEditBtn.addEventListener('click', () => {
            if (imageEditorModal) imageEditorModal.style.display = 'none';
        });
    }

    if (applyEditBtn) {
        applyEditBtn.addEventListener('click', () => {
            const canvas = document.createElement('canvas');
            const ctx = canvas.getContext('2d');
            let containerWidth, containerHeight;

            if (isProfilePicture) {
                containerWidth = containerHeight = 480;
            } else {
                containerWidth = imageToEdit.parentElement.offsetWidth;
                containerHeight = imageToEdit.parentElement.offsetHeight / 3;
            }

            canvas.width = containerWidth;
            canvas.height = containerHeight;

            const scale = zoomControl.value / 100;
            const imgWidth = imageToEdit.naturalWidth * scale;
            const imgHeight = imageToEdit.naturalHeight * scale;

            const sx = (imgWidth - containerWidth) / 2 / scale;
            const sy = (imgHeight - containerHeight) / 2 / scale;

            ctx.drawImage(
                imageToEdit,
                sx, sy, containerWidth / scale, containerHeight / scale,
                0, 0, containerWidth, containerHeight
            );

            canvas.toBlob(async (blob) => {
                const formData = new FormData();
                formData.append('image', blob, isProfilePicture ? 'profile_image.png' : 'banner_image.png');
                formData.append('type', isProfilePicture ? 'profile' : 'banner');

                try {
                    const response = await fetch('/api/upload-profile-image', {
                        method: 'POST',
                        body: formData
                    });
                    const result = await response.json();
                    if (result.success) {
                        const newImageUrl = result.imageUrl + '?t=' + new Date().getTime();
                        if (isProfilePicture) {
                            currentEditingElement.src = newImageUrl;
                        } else {
                            currentEditingElement.style.backgroundImage = `url(${newImageUrl})`;
                        }
                        imageEditorModal.style.display = 'none';
                    } else {
                        alert('Failed to upload image. Please try again.');
                    }
                } catch (error) {
                    alert('An error occurred while uploading the image.');
                }
            }, 'image/png');
        });
    }
});

function getTimeAgo(date) {
    const seconds = Math.floor((new Date() - new Date(date)) / 1000);
    let interval = seconds / 31536000;
    if (interval > 1) return Math.floor(interval) + " years ago";
    interval = seconds / 2592000;
    if (interval > 1) return Math.floor(interval) + " months ago";
    interval = seconds / 86400;
    if (interval > 1) return Math.floor(interval) + " days ago";
    interval = seconds / 3600;
    if (interval > 1) return Math.floor(interval) + " hours ago";
    interval = seconds / 60;
    if (interval > 1) return Math.floor(interval) + " minutes ago";
    return Math.floor(seconds) + " seconds ago";
}

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
        0: { color: '#ff4949', name: 'Red', emoji: '🔴' },
        1: { color: '#00bf00', name: 'Green', emoji: '🟢' },
        2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
        3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
        4: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
        5: { color: '#00e5ff', name: 'Turq', emoji: '🔷' }
    };

    const expandedAllianceColors = {
        0: { color: '#00bf00', name: 'Green', emoji: '🟢' },
        1: { color: '#ff4949', name: 'Red', emoji: '🔴' },
        2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
        3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
        4: { color: '#00e5ff', name: 'Turq', emoji: '🔷' },
        5: { color: '#e72de0', name: 'Pink', emoji: '🟣' },
        6: { color: '#4c4c4c', name: 'Black', emoji: '⚫' },
        7: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
        8: { color: '#28660a', name: 'Olive', emoji: '🟢' },
        9: { color: '#660011', name: 'Scarlet', emoji: '🔴' },
        10: { color: '#2a00ff', name: 'Indigo', emoji: '🔵' },
        11: { color: '#4c4c00', name: 'Gold', emoji: '🟡' },
        12: { color: '#004c3e', name: 'Teal', emoji: '🔷' },
        13: { color: '#6a007f', name: 'Purple', emoji: '🟣' },
        14: { color: '#e5e5e5', name: 'White', emoji: '⚪' },
        15: { color: '#964B00', name: 'Brown', emoji: '🟤' }
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
            const scoreDifference = winningScore - secondScore;
            const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
            const secondGroupName = colorSystem[secondGroupId]?.name || `Team ${secondGroupId}`;
            const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';
            const secondGroupColor = colorSystem[secondGroupId]?.color || '#b8b8b8';

            winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won against <span style="color: ${secondGroupColor}">${secondGroupName}</span> by ${scoreDifference} points.`;
        }
        else if (isAllSoloPlayers) {
            const sortedPlayers = [...parsedPlayers].sort((a, b) => (b.score || 0) - (a.score || 0));
            const winner = sortedPlayers[0];
            const secondPlace = sortedPlayers[1];
            const scoreDifference = winner.score - secondPlace.score;
            const winnerGroupId = usingAlliances ? winner.alliance : winner.team;
            const secondPlaceGroupId = usingAlliances ? secondPlace.alliance : secondPlace.team;
            const winnerColor = colorSystem[winnerGroupId]?.color || '#b8b8b8';
            const secondPlaceColor = colorSystem[secondPlaceGroupId]?.color || '#b8b8b8';

            winningMessage = `<span style="color: ${winnerColor}">${winner.name}</span> won with ${scoreDifference} more points than <span style="color: ${secondPlaceColor}">${secondPlace.name}</span>.`;
        }
        else {
            const [winningGroupId, winningScore] = sortedGroups[0];
            const winningGroupName = colorSystem[winningGroupId]?.name || `Team ${winningGroupId}`;
            const winningGroupColor = colorSystem[winningGroupId]?.color || '#b8b8b8';

            if (uniqueGroups >= 3) {
                winningMessage = `<span style="color: ${winningGroupColor}">${winningGroupName}</span> won with ${winningScore} points.`;
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

    const territoryImages = {
        'North America': 'northamerica',
        'South America': 'southamerica',
        'Europe': 'europe',
        'Africa': 'africa',
        'Asia': 'asia',
        'Russia': 'russia',
        'North Africa': 'northafrica',
        'South Africa': 'southafrica',
        'East Asia': 'eastasia',
        'West Asia': 'westasia',
        'South Asia': 'southasia',
        'Australasia': 'australasia',
        'Antartica': 'antartica'
    };

    parsedPlayers.forEach((player, index) => {
        if (territoryImages[player.territory]) {
            const groupId = usingAlliances ? player.alliance : player.team;
            let colorValue = colorSystem[groupId]?.color || '#00bf00';
            colorValue = colorValue.replace('#', '');

            const overlayImage = `/images/${territoryImages[player.territory]}${colorValue}.png`;

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

window.toggleSpectators = toggleSpectators;

function formatDuration(duration) {
    if (!duration) return 'Unknown';

    if (typeof duration === 'string') {
        const [hours, minutes] = duration.split(':').map(Number);
        if (hours === 0) {
            return `${minutes} min`;
        } else {
            return `${hours} hr ${minutes} min`;
        }
    } else if (typeof duration === 'number') {
        if (duration < 60) {
            return `${Math.round(duration)} min`;
        } else {
            const hours = Math.floor(duration / 60);
            const remainingMinutes = Math.round(duration % 60);
            return `${hours} hr ${remainingMinutes} min`;
        }
    }

    return 'Unknown';
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
                recentGameContainer.innerHTML = '' + demoCard.outerHTML;
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

function createModCard(mod) {
    const headerImagePath = mod.preview_image_path
        ? '/' + mod.preview_image_path.split('/').slice(-2).join('/')
        : '/modpreviews/icon3.png';

    return `
        <div class="mod-item" data-mod-id="${mod.id}">
            <div class="mod-header">
                <div class="mod-header-background" style="background-image: url('${headerImagePath}')"></div>
                <div class="mod-header-overlay"></div>
                <div class="mod-title-container">
                    <h3 class="mod-title">${mod.name}</h3>
                    <h3 class="mod-author">By (${mod.creator})</h3>
                    <span class="file-size"><i class="fas fa-file-archive"></i> ${formatBytes(mod.size)}</span>
                    <p class="mod-subtitle">${mod.description}</p>
                </div>
            </div>
            <div class="mod-info">
                <div class="mod-meta">
                    <span class="downloads"><i class="fas fa-download"></i> ${mod.download_count || 0}</span>
                    <span class="release-date"><i class="fas fa-calendar-alt"></i> ${new Date(mod.release_date).toLocaleDateString()}</span>
                    <span class="compatibility">${mod.compatibility || 'Unknown'}</span>
                    <a href="/api/download-mod/${mod.id}" class="download-btn"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                </div>
            </div>
        </div>
    `;
}

function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';

    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];

    const i = Math.floor(Math.log(bytes) / Math.log(k));

    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

document.addEventListener('DOMContentLoaded', () => {
    const editButton = document.getElementById('edit-profile-btn');
    const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
    const editableFields = document.querySelectorAll('.editable');
    const editableLists = document.querySelectorAll('.editable-list');
    let isEditing = false;

    if (editButton) {
        editButton.addEventListener('click', toggleEdit);
    }

    if (editButtonMobile) {
        editButtonMobile.addEventListener('click', toggleEdit);
    }

    function toggleEdit() {
        isEditing = !isEditing;
        document.body.classList.toggle('editing');

        if (editButton) {
            editButton.classList.toggle('editing');
            editButton.innerHTML = isEditing ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.toggle('editing');
            editButtonMobile.innerHTML = isEditing ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (isEditing) {
            enableEditing();
        } else {
            saveChanges();
        }
    }

    function enableEditing() {
        editableFields.forEach(field => {
            field.contentEditable = true;
            field.classList.add('editing');
        });

        editableLists.forEach(list => {
            const addItemBtn = document.createElement('button');
            addItemBtn.textContent = 'Add Item';
            addItemBtn.classList.add('add-item-btn');
            addItemBtn.addEventListener('click', (e) => {
                e.preventDefault();
                addListItem(list);
            });
            list.parentNode.insertBefore(addItemBtn, list.nextSibling);

            list.querySelectorAll('li').forEach(item => addDeleteButton(item));
        });
    }

    function addListItem(list) {
        const newItem = document.createElement('li');
        const textSpan = document.createElement('span');
        textSpan.contentEditable = true;
        textSpan.textContent = 'New item';
        newItem.appendChild(textSpan);
        addDeleteButton(newItem);
        list.appendChild(newItem);
    }

    function addDeleteButton(item) {
        const deleteBtn = document.createElement('button');
        deleteBtn.innerHTML = '<i class="fas fa-times"></i>';
        deleteBtn.classList.add('delete-item-btn');
        deleteBtn.addEventListener('click', () => item.remove());
        item.appendChild(deleteBtn);
    }

    async function saveChanges() {
        try {
            const currentUserResponse = await fetch('/api/current-user');
            const currentUserData = await currentUserResponse.json();
            const profileUsername = document.getElementById('profile-username-label').textContent;

            if (!currentUserData.user || currentUserData.user.username !== profileUsername) {
                alert('You are not authorized to edit this profile.');
                location.reload();
                return;
            }

            const profileData = {
                discord_username: document.getElementById('discord-info').textContent,
                steam_id: document.getElementById('steam-info').textContent,
                bio: document.getElementById('bio-text').textContent,
                defcon_username: document.getElementById('defcon-username').textContent,
                years_played: document.getElementById('years-of-service').textContent,
                main_contributions: Array.from(document.getElementById('main-contributions-list').children).map(li => li.textContent.replace('X', '').trim()),
                guides_and_mods: Array.from(document.getElementById('guides-and-mods-list').children).map(li => li.textContent.replace('X', '').trim())
            };

            const response = await fetch('/api/update-profile', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(profileData),
            });

            const result = await response.json();

            if (result.success) {
                alert('Profile updated successfully!');
            } else {
                alert('Failed to update profile. Please try again.');
            }
        } catch (error) {
            console.error('Error updating profile:', error);
            alert('An error occurred while updating the profile.');
        }

        editableFields.forEach(field => {
            field.contentEditable = false;
            field.classList.remove('editing');
        });

        editableLists.forEach(list => {
            const addItemBtn = list.parentNode.querySelector('.add-item-btn');
            if (addItemBtn) addItemBtn.remove();
            list.querySelectorAll('.delete-item-btn').forEach(btn => btn.remove());
        });
    }
});