const tutorialVideo = {
    id: 'J5nbZtPTP4Q',
    title: 'DEFCON Expanded Installation Tutorial',
    description: 'Learn how to install DEFCON Expanded in this step-by-step guide.'
};

const gameplayVideos = [
    {
        id: 'lwVrOLSw97U',
        title: '16 Player CPU Simulation',
        description: '16 Bots battle it out in our new 16 player build.'
    },
    {
        id: 'RDYcrpD8k9Y',
        title: '8 Player 4v4: Full Game',
        description: 'A full recording from the spectators point of view with voices.'
    },
    {
        id: 'jcDG5Z23Zn0',
        title: '8 Player 4v4: 16x Speed',
        description: 'This is our 8 player 4v4 game but sped up for quick viewing.'
    },
    {
        id: 'GEWYloL6kr0',
        title: '8 Player CPU Simulation',
        description: '8 CPUs fight to the death on our 8 player build.'
    },
    {
        id: 'mBUYr95Hqjk',
        title: '7 Player Diplomacy: Full Game',
        description: 'A full recording from the spectators point of view with voices.'
    },

];

function createTutorialVideo() {
    const tutorialContainer = document.getElementById('tutorialVideo');
    tutorialContainer.innerHTML = `
        <div class="video-item">
            <img src="https://img.youtube.com/vi/${tutorialVideo.id}/0.jpg" alt="${tutorialVideo.title}" class="video-thumbnail">
            <div class="video-info">
                <h3 class="video-title">${tutorialVideo.title}</h3>
                <p class="video-description">${tutorialVideo.description}</p>
            </div>
        </div>
    `;
    tutorialContainer.querySelector('.video-item').addEventListener('click', () => openVideoModal(tutorialVideo.id));
}

function createGameplayVideoGrid() {
    const videoGrid = document.getElementById('videoGrid');
    gameplayVideos.forEach(video => {
        const videoItem = document.createElement('div');
        videoItem.className = 'video-item';
        videoItem.innerHTML = `
            <img src="https://img.youtube.com/vi/${video.id}/0.jpg" alt="${video.title}" class="video-thumbnail">
            <div class="video-info">
                <h3 class="video-title">${video.title}</h3>
                <p class="video-description">${video.description}</p>
            </div>
        `;
        videoItem.addEventListener('click', () => openVideoModal(video.id));
        videoGrid.appendChild(videoItem);
    });
}

function openVideoModal(videoId) {
    const modal = document.getElementById('videoModal');
    const videoPlayer = document.getElementById('videoPlayer');
    videoPlayer.innerHTML = `<iframe width="100%" height="100%" src="https://www.youtube.com/embed/${videoId}" frameborder="0" allowfullscreen></iframe>`;
    modal.style.display = 'block';
}

function closeVideoModal() {
    const modal = document.getElementById('videoModal');
    const videoPlayer = document.getElementById('videoPlayer');
    modal.style.display = 'none';
    videoPlayer.innerHTML = '';
}

document.addEventListener('DOMContentLoaded', () => {
    createTutorialVideo();
    createGameplayVideoGrid();
    
    const closeBtn = document.querySelector('.close');
    closeBtn.addEventListener('click', closeVideoModal);

    window.addEventListener('click', (event) => {
        const modal = document.getElementById('videoModal');
        if (event.target === modal) {
            closeVideoModal();
        }
    });
});