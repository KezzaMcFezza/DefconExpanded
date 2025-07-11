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
//Last Edited 09-07-2025

(function() {
    'use strict';

    const REPLAY_VIEWER_BASE_URL = 'https://defconexpanded.com';
    
    const isGroup6 = window.location.hostname === 'group6.info';
    const isSFCON = window.location.hostname === 'sfcon.demoszenen.de';
    
    function extractGroup6GameId(url) {
        const match = url.match(/\/dcrec\/download\/(\d+)/);
        return match ? match[1] : null;
    }
    
    function extractSFCONGamePath(url) {
        const match = url.match(/\/dcrec\/(.+\.zip)/);
        return match ? match[1] : null;
    }
    
    function createGroup6WatchButton(gameId) {
        const button = document.createElement('a');
        button.className = 'button dcreclink group6-watch-online-btn';
        button.href = `${REPLAY_VIEWER_BASE_URL}/replay-viewer/group6/${gameId}`;
        button.target = '_blank';
        button.innerHTML = 'Watch Online';
        button.title = `Watch game ${gameId} in browser`;
        button.style.marginLeft = '10px';
        
        return button;
    }
    
    function createSFCONWatchButton(gamePath) {
        const button = document.createElement('a');
        button.className = 'sfcon-watch-online-btn';
        button.href = `${REPLAY_VIEWER_BASE_URL}/replay-viewer/sfcon/${gamePath}`;
        button.target = '_blank';
        button.innerHTML = 'watch online';
        button.title = `Watch ${gamePath} in browser`;
        
        return button;
    }

    
    function addGroup6WatchButtons() {
        const downloadLinks = document.querySelectorAll('a.dcreclink[href*="/dcrec/download/"]');
        
        downloadLinks.forEach(downloadLink => {
            if (downloadLink.parentNode.querySelector('.group6-watch-online-btn')) {
                return;
            }
            
            const gameId = extractGroup6GameId(downloadLink.href);
            if (gameId) {
                const watchButton = createGroup6WatchButton(gameId);
                downloadLink.parentNode.insertBefore(watchButton, downloadLink.nextSibling);
            }
        });
    }
    
    function addSFCONWatchButtons() {
        const downloadLinks = document.querySelectorAll('a[href*="/dcrec/"][href$=".zip"]');
        
        downloadLinks.forEach(downloadLink => {
            if (downloadLink.parentNode.querySelector('.sfcon-watch-online-btn')) {
                return;
            }
            
            const gamePath = extractSFCONGamePath(downloadLink.href);
            if (gamePath) {
                const watchButton = createSFCONWatchButton(gamePath);
                downloadLink.parentNode.insertBefore(watchButton, downloadLink.nextSibling);
            }
        });
    }
    
    function addWatchOnlineButtons() {
        if (isGroup6) {
            addGroup6WatchButtons();
        } else if (isSFCON) {
            addSFCONWatchButtons();
        }
    }
    
    function observeForNewContent() {
        const observer = new MutationObserver(function(mutations) {
            mutations.forEach(function(mutation) {
                if (mutation.addedNodes.length > 0) {
                    const hasNewContent = Array.from(mutation.addedNodes).some(node => 
                        node.nodeType === Node.ELEMENT_NODE && 
                        (node.classList.contains('gameListItem') || 
                         node.querySelector('.gameListItem') ||
                         node.classList.contains('Game') ||
                         node.querySelector('.Game'))
                    );
                    
                    if (hasNewContent) {
                        setTimeout(addWatchOnlineButtons, 100);
                    }
                }
            });
        });
        
        observer.observe(document.body, {
            childList: true,
            subtree: true
        });
    }
    
    function init() {
        addWatchOnlineButtons();
        observeForNewContent();
        
        const siteName = isGroup6 ? 'Group 6' : isSFCON ? 'SFCON' : 'Unknown';
        console.log(`DEFCON Replay Viewer extension loaded for ${siteName}`);
    }
    
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

    setTimeout(addWatchOnlineButtons, 1000);
})(); 