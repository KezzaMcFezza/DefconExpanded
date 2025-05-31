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

    let currentTerritories = vanillaTerritories; // Default to Vanilla

    async function fetchProfileData(username, mode = 'vanilla') {
        const response = await fetch(`/api/profile/${username}?mode=${mode}`);
        if (!response.ok) {
            throw new Error('Profile not found');
        }
        return await response.json();
    }

    try {
        // Fetch the default profile data
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

        const profileCard = document.querySelector('.profile-card');
        if (profileCard) {
            const profileBanner = userProfile.banner_image 
                ? `https://defconexpanded.com${userProfile.banner_image}`
                : '/images/backgroundprofile.png';  
            profileCard.style.backgroundImage = `url('${profileBanner}')`;
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

        if (defconUsername) defconUsername.textContent = userProfile.defcon_username || 'N/A';
        if (yearsOfService) yearsOfService.textContent = userProfile.years_played || '0';
        if (winLossRatio) winLossRatio.textContent = userProfile.winLossRatio || 'Not enough data';
        if (totalGames) totalGames.textContent = userProfile.totalGames || '0';
        if (recordScore) recordScore.textContent = userProfile.record_score || '0';

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
                mainContributionsContainer.textContent = 'No main contributions yet.';
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
                guidesAndModsContainer.textContent = 'No guides or mods yet.';
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

        // Add event listener to dropdown after userProfile is fetched
        const dropdown = document.getElementById('game-mode-dropdown');
        dropdown.addEventListener('change', async (event) => {
            const selectedMode = event.target.value;
            let currentTerritories = vanillaTerritories;
            if (selectedMode === '8player') {
                currentTerritories = eightPlayerTerritories;
            } else if (selectedMode === '10player') {
                currentTerritories = tenPlayerTerritories;
            }

            // Fetch new profile data for the selected mode
            userProfile = await fetchProfileData(username, selectedMode);

            // Update territory stats with new data
            updateTerritoryStats(userProfile, currentTerritories);
        });

        // Set initial stats for vanilla mode
        updateTerritoryStats(userProfile, currentTerritories);

        if (userProfile.defcon_username) {
            await loadRecentGame(userProfile.defcon_username);
        }

    } catch (error) {
        document.title = 'Profile Not Found - DEFCON Expanded';
    }
});

function updateTerritoryStats(userProfile, territories) {
    // Display best and worst territories
    const bestTerritoryElement = document.getElementById('best-territory-username');
    const worstTerritoryElement = document.getElementById('worst-territory-username');
    
    if (bestTerritoryElement) {
        bestTerritoryElement.textContent = `${userProfile.username}'s best territory is: ${userProfile.territoryStats.bestTerritory || 'N/A'}`;
    }

    if (worstTerritoryElement) {
        worstTerritoryElement.textContent = `${userProfile.username}'s worst territory is: ${userProfile.territoryStats.worstTerritory || 'N/A'}`;
    }

    // Add heatmap overlays for best and worst territories
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

    profileBannerOverlay.style.display = 'none';
    profilePictureOverlay.style.display = 'none';

    editButton.addEventListener('click', () => {
        isEditing = !isEditing;
        editButton.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
        editButtonMobile.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
        toggleEditMode();
    });

    function toggleEditMode() {
        profileBannerOverlay.style.display = isEditing ? 'flex' : 'none';
        profilePictureOverlay.style.display = isEditing ? 'flex' : 'none';
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

    imageToEdit.addEventListener('wheel', (e) => {
        e.preventDefault();
        const delta = e.deltaY > 0 ? -0.1 : 0.1; 
        const newZoom = Math.min(Math.max(zoomControl.value + delta * 100, zoomControl.min), zoomControl.max);
        zoomControl.value = newZoom;
        applyZoom();
    });

    function resetZoom() {
        scale = 1;
        zoomControl.value = 100;
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

    profilePictureContainer.addEventListener('click', (e) => {
        if (isEditing && e.target.closest('.edit-overlay')) {
            currentEditingElement = document.getElementById('profile-picture');
            isProfilePicture = true;
            imageUpload.click();
        }
    });

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

    zoomControl.max = 400;

    zoomControl.addEventListener('input', applyZoom);

    function applyTransform() {
        imageToEdit.style.transform = `translate(-50%, -50%) scale(${scale})`;
        imageToEdit.style.left = '50%';
        imageToEdit.style.top = '50%';
    }

    skipEditBtn.addEventListener('click', () => {
        imageEditorModal.style.display = 'none';
    });

    cancelEditBtn.addEventListener('click', () => {
        imageEditorModal.style.display = 'none';
    });

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

    let parsedPlayers = [];
    let teamScores = {};
    let highestScore = 0;

    if (demo.players) {
        try {
            parsedPlayers = JSON.parse(demo.players);
            parsedPlayers.forEach((player, index) => {
                const teamId = player.team || 0;
                if (!teamScores[teamId]) {
                    teamScores[teamId] = 0;
                }
                teamScores[teamId] += player.score;
                if (player.score > highestScore) {
                    highestScore = player.score;
                }
                // Add profileUrl back in for each player
                player.profileUrl = demo[`player${index + 1}_name_profileUrl`] || null;
            });
        } catch (e) {
            console.error('Error parsing players JSON:', e);
        }
    }

    // Sort teams by total score
    const sortedTeams = Object.entries(teamScores).sort((a, b) => b[1] - a[1]);
    let winningMessage = 'No winning team determined.';

    if (sortedTeams.length === 2) {
        const [winningTeamId, winningScore] = sortedTeams[0];
        const [secondTeamId, secondScore] = sortedTeams[1];
        const scoreDifference = winningScore - secondScore;
        const winningTeamName = teamColors[winningTeamId]?.name || `Team ${winningTeamId}`;
        const secondTeamName = teamColors[secondTeamId]?.name || `Team ${secondTeamId}`;
        const winningTeamColor = teamColors[winningTeamId]?.color || '#b8b8b8';
        const secondTeamColor = teamColors[secondTeamId]?.color || '#b8b8b8';

        winningMessage = `<span style="color: ${winningTeamColor}">${winningTeamName}</span> won against <span style="color: ${secondTeamColor}">${secondTeamName}</span> by ${scoreDifference} points.`;
    } else if (sortedTeams.length === 1) {
        const [winningTeamId, winningScore] = sortedTeams[0];
        const winningTeamName = teamColors[winningTeamId]?.name || `Team ${winningTeamId}`;
        const winningTeamColor = teamColors[winningTeamId]?.color || '#b8b8b8';

        winningMessage = `<span style="color: ${winningTeamColor}">${winningTeamName}</span> won with ${winningScore} points.`;
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
            <!-- Base map -->
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
            const teamNumber = player.team || 0;
            let colorName = teamColors[teamNumber]?.color || '#00bf00';

            colorName = colorName.replace('#', '');

            const overlayImage = `/images/${territoryImages[player.territory]}${colorName}.png`;

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
        // Sort players within each team by their individual score
        sortedTeams.forEach(([teamId, _]) => {
            const teamColor = teamColors[teamId]?.color || '#00bf00';
            parsedPlayers
                .filter(player => player.team === Number(teamId))
                .sort((a, b) => b.score - a.score)
                .forEach(player => {
                    const playerNameWithCrown = `${player.name}${player.score === highestScore ? ' 👑' : ''}`;
                    const playerIconLink = player.profileUrl
                        ? `<a href="${player.profileUrl}" target="_blank"><i class="fa-solid fa-square-up-right"></i></a>`
                        : '';

                    demoCardHtml += `
                        <tr>
                            <td style="color: ${teamColor}">
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

    demoCardHtml += `
            </table>
            <div class="demo-actions">
                <a href="/api/download/${demo.name}" class="download-btn-demo"><i class="fas fa-cloud-arrow-down"></i> Download</a>
                <button class="btn-report" onclick="showReportOptions(${demo.id}, event)">Report</button>
                <span class="downloads-count"><i class="fas fa-download"></i> ${demo.download_count || 0}</span>
            </div>
        </div>`;

    demoCard.innerHTML = demoCardHtml;

    return demoCard;
}

function formatDuration(duration) {
    if (!duration) return 'Unknown';
    const [hours, minutes] = duration.split(':').map(Number);
    if (hours === 0) {
        return `${minutes} min`;
    } else {
        return `${hours} hr ${minutes} min`;
    }
}

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
                recentGameContainer.innerHTML = '<h2 class="headerresources2" style="text-align: center; font-weight: 1; margin-bottom: 10px;">Most Recent Game</h2>' + demoCard.outerHTML;
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
        if (editButton) {
            editButton.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
        }
        if (editButtonMobile) {
            editButtonMobile.textContent = isEditing ? 'Save Changes' : 'Edit Profile';
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
        deleteBtn.textContent = 'X';
        deleteBtn.classList.add('delete-item-btn');
        deleteBtn.addEventListener('click', () => item.remove());
        item.appendChild(deleteBtn);
    }

    async function saveChanges() {
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

        try {
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
            alert('An error occurred while updating the profile.');
        }

        editableFields.forEach(field => {
            field.contentEditable = false;
            field.classList.remove('editing');
        });

        editableLists.forEach(list => {
            list.parentNode.querySelector('.add-item-btn').remove();
            list.querySelectorAll('.delete-item-btn').forEach(btn => btn.remove());
        });
    }
});