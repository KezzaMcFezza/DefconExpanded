// Global variables
let allResources = [];

function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function updateResourceList() {
    console.log('Updating resource list');
    // First, check admin status
    fetch('/api/checkAuth')
        .then(response => response.json())
        .then(data => {
            window.isAdmin = data.isAdmin;
            console.log('Admin status set to:', window.isAdmin);
            // Now fetch resources
            return fetch('/api/resources');
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(resources => {
            console.log('Received resources:', resources);
            allResources = resources;
            displayResources(allResources);
        })
        .catch(error => {
            console.error('Error fetching resources:', error);
            alert('Failed to fetch resource data. Please try again later.');
        });
}

function displayResources(resources) {
    console.log('Displaying resources, isAdmin:', window.isAdmin);
    const recentBuildsSection = document.querySelector('.download-section-recent');
    const versionHistorySection = document.querySelector('.download-section');

    if (!recentBuildsSection || !versionHistorySection) {
        console.error('Required sections not found in the HTML');
        return;
    }

    // Clear existing content
    recentBuildsSection.innerHTML = '<h2 class="headerresources2">Recent Builds</h2>';
    versionHistorySection.innerHTML = '<h2 class="headerresources2">Version History</h2>';

    // Group resources by player count
    const groupedResources = groupResourcesByPlayerCount(resources);

    // Display recent builds
    for (const [playerCount, builds] of Object.entries(groupedResources)) {
        const latestBuild = builds[0]; // Assuming builds are sorted by date
        const recentBuildHtml = createBuildTable(playerCount, [latestBuild], true);
        recentBuildsSection.innerHTML += recentBuildHtml;
    }

    // Display version history
    for (const [playerCount, builds] of Object.entries(groupedResources)) {
        const versionHistoryHtml = createBuildTable(playerCount, builds, false);
        versionHistorySection.innerHTML += versionHistoryHtml;
    }
}

function groupResourcesByPlayerCount(resources) {
    const grouped = {
        '8 PLAYER BUILD': [],
        '10 PLAYER BUILD': [],
        '16 PLAYER BUILD': []
    };

    resources.forEach(resource => {
        if (resource.name.includes('8 Players')) {
            grouped['8 PLAYER BUILD'].push(resource);
        } else if (resource.name.includes('10 Players')) {
            grouped['10 PLAYER BUILD'].push(resource);
        } else if (resource.name.includes('16 Players')) {
            grouped['16 PLAYER BUILD'].push(resource);
        }
    });

    return grouped;
}

function createBuildTable(playerCount, builds, isRecentBuild) {
    console.log('Creating build table, isAdmin:', window.isAdmin);
    let html = `
        <h2 class="card-title-download">${playerCount}</h2>
        <div class="tutorial-download-container">
            <table class="version-history-table">
                <thead>
                    <tr>
                        <th>Version</th>
                        <th>Release Date</th>
                        <th>Download Link</th>
                        ${window.isAdmin ? '<th>Actions</th>' : ''}
                    </tr>
                </thead>
                <tbody>
    `;

    builds.forEach((build, index) => {
        const versionMatch = build.name.match(/v(\d+\.\d+\.\d+)/);
        const version = versionMatch ? versionMatch[1] : 'Unknown';
        const releaseDate = new Date(build.date).toLocaleDateString();
        const isLatest = index === 0;

        html += `
            <tr>
                <td class="td2">${version}</td>
                <td>${isLatest && isRecentBuild ? `Latest Version - ${releaseDate}` : releaseDate}</td>
                <td><a href="/api/download-resource/${encodeURIComponent(build.name)}" class="btn-download foo">Download</a></td>
                ${window.isAdmin ? `<td><button onclick="deleteResource(${build.id})" class="btn-delete foo">Delete</button></td>` : ''}
            </tr>
        `;
    });

    html += `
                </tbody>
            </table>
        </div>
    `;

    return html;
}

async function uploadResource(event) {
    event.preventDefault();

    const form = event.target;
    const formData = new FormData(form);

    try {
        const response = await fetch('/api/upload-resource', {
            method: 'POST',
            body: formData,
        });

        if (response.ok) {
            const data = await response.json();
            alert('Resource uploaded successfully');
            form.reset();
            updateResourceList();
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to upload resource');
        }
    } catch (error) {
        console.error('Error uploading resource:', error);
        alert('An error occurred while uploading resource');
    }
}

async function deleteResource(resourceId) {
    if (!window.isAdmin) {
        alert('You must be an admin to delete resources');
        return;
    }
    if (confirm('Are you sure you want to delete this resource?')) {
        try {
            const response = await fetch(`/api/resource/${resourceId}`, {
                method: 'DELETE',
            });
            if (response.ok) {
                alert('Resource deleted successfully');
                updateResourceList();
            } else {
                const data = await response.json();
                alert(data.error || 'Failed to delete resource');
            }
        } catch (error) {
            console.error('Error deleting resource:', error);
            alert('An error occurred while deleting resource');
        }
    }
}

document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM content loaded in resources.js');
    updateResourceList();

    const uploadForm = document.getElementById('upload-resource-form');
    if (uploadForm) {
        uploadForm.addEventListener('submit', uploadResource);
    }

    // Show/hide admin upload form based on admin status
    fetch('/api/checkAuth')
        .then(response => response.json())
        .then(data => {
            window.isAdmin = data.isAdmin;
            console.log('Admin status checked in DOMContentLoaded:', window.isAdmin);
            const adminUpload = document.getElementById('admin-upload');
            if (adminUpload) {
                adminUpload.style.display = window.isAdmin ? 'block' : 'none';
            }
            // Force a re-render of the resources after setting admin status
            displayResources(allResources);
        });
});

// Export functions that might be used elsewhere
window.updateResourceList = updateResourceList;
window.deleteResource = deleteResource;