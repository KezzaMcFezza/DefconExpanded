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
//Last Edited 05-06-2025

class SmurfChecker {
    constructor() {
        this.searchInput = document.getElementById('player-search-input');
        this.searchBtn = document.getElementById('search-btn');
        this.resultsContainer = document.getElementById('results-container');
        this.resultsTitle = document.getElementById('results-title');
        this.resultsContent = document.getElementById('results-content');
        this.reportForm = document.getElementById('report-form');
        
        this.bindEvents();
    }

    bindEvents() {
        if (this.searchBtn) {
            this.searchBtn.addEventListener('click', () => this.performSearch());
        }
        
        if (this.searchInput) {
            this.searchInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    this.performSearch();
                }
            });
        }

        if (this.reportForm) {
            this.reportForm.addEventListener('submit', (e) => this.handleReportSubmission(e));
        }
    }

    async performSearch() {
        const playerName = this.searchInput?.value?.trim();
        if (!playerName) {
            this.showError('Please enter a player name to search');
            return;
        }

        this.showLoading();
        
        try {
            const response = await fetch(`/api/smurf-check?playerName=${encodeURIComponent(playerName)}`);
            const data = await response.json();
            
            if (!response.ok) {
                throw new Error(data.error || 'Search failed');
            }
            
            this.displayResults(data, playerName);
        } catch (error) {
            console.error('Search error:', error);
            this.showError(`Search failed: ${error.message}`);
        }
    }

    showLoading() {
        if (!this.resultsContainer) return;
        
        this.resultsContainer.style.display = 'block';
        this.resultsTitle.textContent = 'Searching...';
        this.resultsContent.innerHTML = `
            <div class="loading">
                <i class="fas fa-spinner"></i>
                Analyzing player data...
            </div>
        `;
    }

    showError(message) {
        if (!this.resultsContainer) return;
        
        this.resultsContainer.style.display = 'block';
        this.resultsTitle.textContent = 'Search Error';
        this.resultsContent.innerHTML = `
            <div class="error-message">
                <i class="fas fa-exclamation-triangle"></i>
                ${message}
            </div>
        `;
    }

    displayResults(data, searchedPlayerName) {
        if (!this.resultsContainer) return;
        
        this.resultsContainer.style.display = 'block';
        this.resultsTitle.textContent = `Results for "${searchedPlayerName}"`;
        
        
        if (!data.results || data.results.length === 0) {
            this.resultsContent.innerHTML = `
                <div class="no-infractions">
                    No players found matching "${searchedPlayerName}"
                </div>
            `;
            return;
        }
     
        const player = data.results[0];
        
        if (!player.alternateNames || player.alternateNames.length === 0) {
            this.resultsContent.innerHTML = `
                <div class="no-infractions">
                    <i class="fas fa-check-circle"></i>
                    No information found for "${player.name}". Clean record!
                </div>
            `;
            return;
        }

        const totalInfractions = player.infractionCount || player.alternateNames.length;
        const alternateNamesWithCounts = player.alternateNamesWithCounts || [];
        const filteredAlternateNames = alternateNamesWithCounts.filter(item => item.name !== player.name);
        const infractionClass = this.getInfractionClass(totalInfractions);
        
        this.resultsContent.innerHTML = `
            <div class="player-info">
                <div class="info-card">
                    <h4>Player Name</h4>
                    <p>${player.name}</p>
                </div>
                <div class="info-card">
                    <h4>Total Hits</h4>
                    <p class="infraction-count ${infractionClass}">${totalInfractions}</p>
                </div>
            </div>
            
            ${filteredAlternateNames.length > 0 ? `
                <div class="alternate-names-section">
                    <h4>Alternate Names</h4>
                    <div class="alternate-names-list-container">
                        ${filteredAlternateNames.map((item, index) => 
                            `<div class="alternate-name-item">
                                <div class="alternate-name-header">
                                    <span class="alternate-name-title">${item.name}</span>
                                    <span class="alternate-name-count">Occurred ${item.count} time${item.count === 1 ? '' : 's'}</span>
                                </div>
                            </div>`
                        ).join('')}
                    </div>
                </div>
            ` : `
                <div class="alternate-names-section">
                    <h4>Alternate Names</h4>
                    <p style="color: #b8b8b8; font-style: italic;">No alternate names detected</p>
                </div>
            `}
            
            <div class="infraction-demos-section">
                <h4>Evidence</h4>
                <p style="color: #b8b8b8; margin-bottom: 1rem; font-size: 0.95rem;">
                    Games where alternate names were detected...
                </p>
                <div id="smurf-demos-container" class="smurf-demos-container">
                    <!-- Demo cards will be inserted here -->
                </div>
            </div>
        `;
        
        
        this.displayInfractionGames(player.infractionDemos || []);
    }

    getInfractionClass(count) {
        if (count === 0) return 'low';
        if (count <= 2) return 'low';
        if (count <= 5) return 'medium';
        return 'high';
    }

    displayInfractionGames(infractionDemos) {
        const gamesContainer = document.getElementById('smurf-demos-container');
        if (!gamesContainer) return;

        gamesContainer.innerHTML = '';

        if (!infractionDemos || infractionDemos.length === 0) {
            gamesContainer.innerHTML = `
                <div class="no-infractions">
                    <i class="fas fa-info-circle"></i>
                    No infraction games available for display
                </div>
            `;
            return;
        }

        const gamesHtml = infractionDemos.map(demo => {
            const gameDate = new Date(demo.date);
            const formattedDate = gameDate.toLocaleDateString('en-US', {
                year: 'numeric',
                month: 'short',
                day: 'numeric'
            });
            const formattedTime = gameDate.toLocaleTimeString('en-US', {
                hour: '2-digit',
                minute: '2-digit'
            });

            const alternateName = demo.reason ? demo.reason.replace('Alternate name detected: ', '') : 'Unknown';

            return `
                <div class="infraction-game-item">
                    <div class="game-header">
                        <span class="game-type">${demo.game_type || 'Unknown Game Type'}</span>
                        <span class="game-date">${formattedDate} at ${formattedTime}</span>
                    </div>
                    <div class="game-details">
                        <div class="game-reason">
                            Alternate name: <strong>${alternateName}</strong>
                        </div>
                        <div class="game-actions">
                            <button class="download-demo-btn" onclick="window.open('/api/download/${encodeURIComponent(demo.name)}', '_blank')" title="Download demo file to verify evidence">
                                <i class="fas fa-download"></i>
                                Download
                            </button>
                        </div>
                    </div>
                </div>
            `;
        }).join('');

        gamesContainer.innerHTML = gamesHtml;
    }

    async handleReportSubmission(e) {
        e.preventDefault();
        
        const playerNameInput = document.getElementById('report-player-name');
        const descriptionInput = document.getElementById('report-description');
        const submitBtn = document.getElementById('submit-report-btn');
        
        if (!playerNameInput || !descriptionInput || !submitBtn) return;

        const playerName = playerNameInput.value.trim();
        const description = descriptionInput.value.trim();

        if (!playerName || !description) {
            this.showReportError('Please fill in all fields.');
            return;
        }

        const isLoggedIn = await this.checkUserAuthentication();
        if (!isLoggedIn) {
            this.showReportError('You must be logged in to submit a report. Please <a href="/login" target="_blank">login</a> or <a href="/signup" target="_blank">sign up</a> first.');
            return;
        }

        submitBtn.disabled = true;
        submitBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin" style="margin-right: 0.5rem;"></i>Submitting...';

        try {
            const response = await fetch('/api/smurf-report', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                credentials: 'include',
                body: JSON.stringify({
                    playerName: playerName,
                    description: description
                })
            });

            const data = await response.json();

            if (response.ok) {
                this.showReportSuccess(data.message);
                playerNameInput.value = '';
                descriptionInput.value = '';
            } else {
                this.showReportError(data.error || 'Failed to submit report');
            }
        } catch (error) {
            console.error('Report submission error:', error);
            this.showReportError('Network error. Please try again later.');
        } finally {
            
            submitBtn.disabled = false;
            submitBtn.innerHTML = '<i class="fas fa-paper-plane" style="margin-right: 0.5rem;"></i>Submit Report';
        }
    }

    async checkUserAuthentication() {
        try {
            const response = await fetch('/api/checkAuth', {
                method: 'GET',
                credentials: 'include'
            });
            
            if (response.ok) {
                const data = await response.json();
                return data.isLoggedIn;
            }
            return false;
        } catch (error) {
            console.error('Auth check error:', error);
            return false;
        }
    }

    showReportSuccess(message) {
        this.showReportNotification(message, 'success');
    }

    showReportError(message) {
        this.showReportNotification(message, 'error');
    }

    showReportNotification(message, type) {
        
        const existingNotification = document.querySelector('.report-notification');
        if (existingNotification) {
            existingNotification.remove();
        }

        const notification = document.createElement('div');
        notification.className = `report-notification ${type}`;
        notification.innerHTML = `
            <i class="fas ${type === 'success' ? 'fa-check-circle' : 'fa-exclamation-triangle'}"></i>
            ${message}
        `;

        const submitBtn = document.getElementById('submit-report-btn');
        if (submitBtn && submitBtn.parentNode) {
            submitBtn.parentNode.insertBefore(notification, submitBtn.nextSibling);
        }

        setTimeout(() => {
            if (notification && notification.parentNode) {
                notification.remove();
            }
        }, 5000);
    }
}

export function initializeSmurfChecker() {
    new SmurfChecker();
} 