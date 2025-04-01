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
//Last Edited 01-04-2025

import { isUserLoggedIn } from './authentication.js';
import { formatBytes } from './main.js';

async function loadModsByType(type, searchTerm = '', sortBy = 'latest') {
  try {
    let url = `/api/mods?type=${encodeURIComponent(type)}`;
    if (searchTerm) {
      url += `&search=${encodeURIComponent(searchTerm)}`;
    }
    url += `&sort=${encodeURIComponent(sortBy)}`;

    const response = await fetch(url);
    const mods = await response.json();
    displayMods(mods);
  } catch (error) {
    console.error('Error loading mods:', error);
  }
}

function displayMods(mods) {
  const modList = document.getElementById('mod-list');
  if (!modList) return;
  
  modList.innerHTML = '';

  const uniqueMods = Array.from(new Set(mods.map(mod => mod.id)))
    .map(id => mods.find(mod => mod.id === id));

  uniqueMods.forEach(mod => {
    const modItem = document.createElement('div');
    modItem.className = 'mod-item';
    modItem.dataset.modId = mod.id;

    const headerImagePath = mod.preview_image_path
      ? '/' + mod.preview_image_path.split('/').slice(-2).join('/')
      : '/modpreviews/icon3.png';

    modItem.innerHTML = `
      <div class="mod-header">
        <div class="mod-header-background" style="background-image: url('${headerImagePath}')"></div>
        <div class="mod-header-overlay"></div>
        <div class="mod-title-container">
          <h3 class="mod-title">${mod.name}</h3>
          <h3 class="mod-author">By (${mod.creator})</h3>
          <span class="file-size"><i class="fas fa-file-archive"></i> ${formatBytes(mod.size)}</span>
          <p class="mod-subtitle">${mod.description}</p>
          <div class="mod-interaction">
            <button class="like-button ${mod.isLiked ? 'liked' : ''}">
              <i class="fas fa-thumbs-up"></i>
              <span class="like-count">${mod.likes_count || 0}</span>
            </button>
            <button class="favorite-button ${mod.isFavorited ? 'favorited' : ''}">
              <i class="fas fa-star"></i>
              <span class="favorite-count">${mod.favorites_count || 0}</span>
            </button>
          </div>
        </div>
      </div>
      <div class="mod-info">
        <div class="mod-meta">
          <span class="downloads"><i class="fas fa-download"></i> ${mod.download_count || 0}</span>
          <span class="release-date"><i class="fas fa-calendar-alt"></i> ${new Date(mod.release_date).toLocaleDateString()}</span>
          <span class="compatibility">${mod.compatibility || 'Unknown'}</span>
          <button class="btn-report">Report</button>
          <a href="/api/download-mod/${mod.id}" class="download-btn"><i class="fas fa-cloud-arrow-down"></i> Download</a>
        </div>
      </div>
    `;

    modList.appendChild(modItem);

    const likeButton = modItem.querySelector('.like-button');
    const favoriteButton = modItem.querySelector('.favorite-button');

    likeButton.addEventListener('click', async function () {
      const response = await likeMod(mod.id);
      if (response.ok) {
        this.classList.toggle('liked');
        mod.isLiked = !mod.isLiked;
        const likeCount = this.querySelector('.like-count');
        let count = parseInt(likeCount.textContent);
        likeCount.textContent = mod.isLiked ? count + 1 : Math.max(0, count - 1);
      }
    });

    favoriteButton.addEventListener('click', async function () {
      const response = await favoriteMod(mod.id);
      if (response.ok) {
        this.classList.toggle('favorited');
        mod.isFavorited = !mod.isFavorited;
        const favoriteCount = this.querySelector('.favorite-count');
        let count = parseInt(favoriteCount.textContent);
        favoriteCount.textContent = mod.isFavorited ? count + 1 : Math.max(0, count - 1);
      }
    });
  });
}

async function likeMod(modId) {
  if (!isUserLoggedIn()) {
    await window.alert('You need to be logged in to like mods.');
    return { ok: false };
  }
  
  try {
    const response = await fetch(`/api/mods/${modId}/like`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${localStorage.getItem('token')}`
      }
    });
    
    if (!response.ok) {
      if (response.status === 401) {
        await window.alert('You need to be logged in to like mods.');
      } else {
        const errorData = await response.json();
        await window.alert(errorData.error || 'An error occurred while liking the mod.');
      }
      return { ok: false };
    }
    
    return { ok: true };
  } catch (error) {
    await window.alert('An error occurred while liking the mod.');
    return { ok: false };
  }
}

async function favoriteMod(modId) {
  if (!isUserLoggedIn()) {
    await window.alert('You need to be logged in to favorite mods.');
    return { ok: false };
  }
  
  try {
    const response = await fetch(`/api/mods/${modId}/favorite`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${localStorage.getItem('token')}`
      }
    });
    
    if (!response.ok) {
      if (response.status === 401) {
        await window.alert('You need to be logged in to favorite mods.');
      } else {
        const errorData = await response.json();
        await window.alert(errorData.error || 'An error occurred while favoriting the mod.');
      }
      return { ok: false };
    }
    
    return { ok: true };
  } catch (error) {
    await window.alert('An error occurred while favoriting the mod.');
    return { ok: false };
  }
}

async function getUserLikes() {
  try {
    const response = await fetch('/api/user/likes');
    const data = await response.json();
    return data.likes || [];
  } catch (error) {
    console.error('Error getting user likes:', error);
    return [];
  }
}

async function getUserFavorites() {
  try {
    const response = await fetch('/api/user/favorites');
    const data = await response.json();
    return data.favorites || [];
  } catch (error) {
    console.error('Error getting user favorites:', error);
    return [];
  }
}

function setupModFilters(modType) {
  const searchInput = document.getElementById('mod-search');
  const searchButton = document.getElementById('search-button');
  const sortSelect = document.getElementById('sort-select');

  if (!searchInput || !searchButton || !sortSelect) return;

  searchButton.addEventListener('click', () => {
    const searchTerm = searchInput.value.trim();
    loadModsByType(modType, searchTerm, sortSelect.value);
  });

  sortSelect.addEventListener('change', () => {
    loadModsByType(modType, searchInput.value.trim(), sortSelect.value);
  });

  
  sortSelect.value = 'latest';
  loadModsByType(modType, '', 'latest');
}

function initializeMods() {
  const modListContainer = document.getElementById('mod-list');
  if (modListContainer) {
    const modType = modListContainer.dataset.modType;
    if (modType) {
      setupModFilters(modType);
    }
  }

  const searchInput = document.getElementById('mod-search');
  const searchButton = document.getElementById('search-button');
  const sortSelect = document.getElementById('sort-select');

  if (modListContainer && searchButton && searchInput) {
    searchButton.addEventListener('click', () => {
      const searchTerm = searchInput.value.trim();
      const sortBy = sortSelect ? sortSelect.value : '';
      loadModsByType(modListContainer.dataset.modType, searchTerm, sortBy);
    });

    if (sortSelect) {
      sortSelect.addEventListener('change', () => {
        const searchTerm = searchInput ? searchInput.value.trim() : '';
        loadModsByType(modListContainer.dataset.modType, searchTerm, sortSelect.value);
      });
    }
  }
}

export {
  loadModsByType,
  displayMods,
  likeMod,
  favoriteMod,
  getUserLikes,
  getUserFavorites,
  setupModFilters,
  initializeMods
};