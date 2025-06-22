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

import { formatBytes } from '../main/main.js';

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

export { createModCard };