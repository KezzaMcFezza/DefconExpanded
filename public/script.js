// Global variables
let soundVolume = 0.1;
let isAdmin = false;

function setActiveNavItem() {
    const currentPath = window.location.pathname;
    const navItems = document.querySelectorAll('#sidebar .list-items li');
    
    navItems.forEach(item => {
        const link = item.querySelector('a');
        item.classList.remove('active');
        if (currentPath === link.getAttribute('href') || currentPath.startsWith(link.getAttribute('href') + '/')) {
            item.classList.add('active');
        }
    });
}

function setupMobileMenu() {
    console.log('Setting up mobile menu');
    const sidebar = document.getElementById('sidebar');
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');

    if (title && listItems) {
        title.addEventListener('click', function(e) {
            e.preventDefault();
            console.log('Title clicked');
            listItems.classList.toggle('show');
            console.log('Menu toggled, show class:', listItems.classList.contains('show'));
        });

        listItems.addEventListener('click', function(e) {
            if (e.target.tagName === 'A') {
                console.log('Link clicked, closing menu');
                listItems.classList.remove('show');
            }
        });
    } else {
        console.error('Could not find title or list items elements');
    }
}

function showLoginForm() {
    const sidebar = document.getElementById('sidebar');
    if (!sidebar) {
        console.error('Sidebar not found');
        return;
    }

    if (document.getElementById('login-container')) {
        console.log('Login form already exists');
        return;
    }

    const loginContainer = document.createElement('div');
    loginContainer.id = 'login-container';
    loginContainer.innerHTML = `
        <form id="login-form">
            <input type="text" id="username" placeholder="Username" required>
            <input type="password" id="password" placeholder="Password" required>
            <button type="submit">Login</button>
        </form>
    `;
    
    const listItems = sidebar.querySelector('.list-items');
    const icons = sidebar.querySelector('.icons');
    
    if (listItems && icons) {
        sidebar.insertBefore(loginContainer, icons);
    } else {
        console.error('Could not find proper position for login form');
        sidebar.appendChild(loginContainer);
    }

    document.getElementById('login-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        await login(username, password);
    });
}

async function login(username, password) {
    try {
        const response = await fetch('/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ username, password }),
        });
        const data = await response.json();
        if (response.ok) {
            isAdmin = true;
            alert('Logged in successfully');
            document.getElementById('login-container').remove();
            showLogoutButton();
            showUploadForm();
            updateDemoList();
            makeContentEditable();
        } else {
            alert(data.error || 'Login failed');
        }
    } catch (error) {
        console.error('Login error:', error);
        alert('An error occurred during login');
    }
}

function showLogoutButton() {
    const sidebar = document.getElementById('sidebar');
    if (!sidebar) {
        console.error('Sidebar not found');
        return;
    }

    const logoutContainer = document.createElement('div');
    logoutContainer.id = 'logout-container';
    const logoutButton = document.createElement('button');
    logoutButton.textContent = 'Logout';
    logoutButton.onclick = logout;
    logoutContainer.appendChild(logoutButton);

    const icons = sidebar.querySelector('.icons');
    
    if (icons) {
        sidebar.insertBefore(logoutContainer, icons);
    } else {
        console.error('Could not find proper position for logout button');
        sidebar.appendChild(logoutContainer);
    }
}

async function logout() {
    try {
        const response = await fetch('/api/logout', { method: 'POST' });
        if (response.ok) {
            isAdmin = false;
            alert('Logged out successfully');
            document.getElementById('logout-container').remove();
            showLoginForm();
            hideUploadForm();
            updateDemoList();
            removeEditability();
        } else {
            alert('Logout failed');
        }
    } catch (error) {
        console.error('Logout error:', error);
    }
}

function makeContentEditable() {
    if (!isAdmin) return;

    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach((card, index) => {
        // Remove existing buttons to avoid duplication
        const existingButtons = card.querySelectorAll('.edit-button, .save-button, .cancel-button');
        existingButtons.forEach(button => button.remove());

        const editButton = document.createElement('button');
        editButton.textContent = 'Edit';
        editButton.className = 'edit-button';
        editButton.onclick = () => toggleEditMode(card, index);
        card.insertBefore(editButton, card.firstChild);

        const saveButton = document.createElement('button');
        saveButton.textContent = 'Save';
        saveButton.className = 'save-button';
        saveButton.style.display = 'none';
        saveButton.onclick = () => saveChanges(card, index);
        card.insertBefore(saveButton, editButton.nextSibling);

        const cancelButton = document.createElement('button');
        cancelButton.textContent = 'Cancel';
        cancelButton.className = 'cancel-button';
        cancelButton.style.display = 'none';
        cancelButton.onclick = () => cancelEdit(card);
        card.insertBefore(cancelButton, saveButton.nextSibling);

        const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
    });
}

function toggleEditMode(card, index) {
    if (!isAdmin) return;

    const editButton = card.querySelector('.edit-button');
    const saveButton = card.querySelector('.save-button');
    const cancelButton = card.querySelector('.cancel-button');
    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');

    if (editButton.style.display !== 'none') {
        // Entering edit mode
        editButton.style.display = 'none';
        saveButton.style.display = 'inline-block';
        cancelButton.style.display = 'inline-block';
        elements.forEach(el => {
            el.contentEditable = 'true';
            el.classList.add('editable');
        });
    }
}

function cancelEdit(card) {
    const editButton = card.querySelector('.edit-button');
    const saveButton = card.querySelector('.save-button');
    const cancelButton = card.querySelector('.cancel-button');
    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');

    editButton.style.display = 'inline-block';
    saveButton.style.display = 'none';
    cancelButton.style.display = 'none';
    elements.forEach(el => {
        el.contentEditable = 'false';
        el.classList.remove('editable');
    });

    location.reload(); // Revert changes without saving
}

async function saveChanges(card, index) {
    if (!isAdmin) return;

    const content = {};
    const pageName = getPageName();

    // Collect all content, including nested elements
    card.childNodes.forEach((node, i) => {
        if (node.nodeType === Node.ELEMENT_NODE && !node.classList.contains('media-block') && !['button', 'script'].includes(node.tagName.toLowerCase())) {
            content[`element_${i}`] = {
                outerHTML: node.outerHTML,
                isHTML: true
            };
        }
    });

    // Preserve media block
    const mediaBlock = card.querySelector('.media-block');
    if (mediaBlock) {
        content['media_block'] = {
            outerHTML: mediaBlock.outerHTML
        };
    }

    console.log('Saving changes for page:', pageName, 'card index:', index);
    console.log('Content being saved:', content);

    try {
        const response = await fetch('/api/updateContent', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ cardIndex: index, content, pageName }),
        });

        if (response.ok) {
            alert('Content updated successfully');
            cancelEdit(card); // Reset the edit mode
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to update content');
        }
    } catch (error) {
        console.error('Error updating content:', error);
        alert('An error occurred while updating content');
    }
}

function removeEditability() {
    const cards = document.querySelectorAll('.card');
    cards.forEach(card => {
        const buttons = card.querySelectorAll('.edit-button, .save-button, .cancel-button');
        buttons.forEach(button => button.remove());

        const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
    });
}

function getPageName() {
    const path = window.location.pathname;
    const pages = ['index.html', 'about.html', 'resources.html', 'news.html', 'laikasdefcon.html', '404.html', 'media.html', 'matchroom.html'];
    let pageName = pages.find(page => path.endsWith(page));
    
    if (!pageName) {
        pageName = path === '/' ? 'index.html' : path.split('/').pop() + '.html';
    }
    
    return pageName;
}

function showUploadForm() {
    const adminUpload = document.getElementById('admin-upload');
    if (adminUpload) {
        adminUpload.style.display = isAdmin ? 'block' : 'none';
    }
}

function hideUploadForm() {
    const adminUpload = document.getElementById('admin-upload');
    if (adminUpload) {
        adminUpload.style.display = 'none';
    }
}

async function uploadDemo(event) {
    event.preventDefault();

    const form = event.target;
    const formData = new FormData(form);

    try {
        const response = await fetch('/api/upload', {
            method: 'POST',
            body: formData,
        });

        if (response.ok) {
            const data = await response.json();
            alert('Demo uploaded successfully');
            form.reset();
            addDemoToList(data.demoName); // Add the new demo to the list
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to upload demo');
        }
    } catch (error) {
        console.error('Error uploading demo:', error);
        alert('An error occurred while uploading demo');
    }
}

function addDemoToList(demoName) {
    const demoContainer = document.getElementById('demo-container');
    if (demoContainer) {
        const demoCard = createDemoCard({ name: demoName, date: new Date() });
        demoContainer.insertBefore(demoCard, demoContainer.firstChild);
    }
}

document.addEventListener('DOMContentLoaded', async function() {
    console.log('DOM fully loaded and parsed');

    try {
        const response = await fetch('/api/checkAuth');
        const data = await response.json();
        if (data.isAdmin) {
            isAdmin = true;
            showLogoutButton();
            showUploadForm();
            makeContentEditable();
        } else {
            isAdmin = false;
            showLoginForm();
            hideUploadForm();
            removeEditability();
        }
    } catch (error) {
        console.error('Error checking authentication:', error);
        showLoginForm();
        hideUploadForm();
        removeEditability();
    }

    setActiveNavItem();
    setupMobileMenu();

    const searchButton2 = document.getElementById('search-button2');
    const searchInput2 = document.getElementById('search-input2');

    if (searchButton2 && searchInput2) {
        searchButton2.addEventListener('click', performGameSearch);
        searchInput2.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                performGameSearch();
            }
        });
    }

    const uploadForm = document.getElementById('upload-form');
    if (uploadForm) {
        uploadForm.addEventListener('submit', uploadDemo);
    }
});

function smoothScroll(target) {
    const element = document.querySelector(target);
    if (element) {
        window.scrollTo({
            top: element.offsetTop,
            behavior: 'smooth'
        });
    }
}

document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function(e) {
        e.preventDefault();
        smoothScroll(this.getAttribute('href'));
    });
});

window.smoothScroll = smoothScroll;
window.setupMobileMenu = setupMobileMenu;
window.setActiveNavItem = setActiveNavItem;

window.addEventListener('load', function() {
    console.log('Window loaded, calling setupMobileMenu');
    setupMobileMenu();
    setActiveNavItem();
});

window.addEventListener('resize', function() {
    if (window.innerWidth <= 768) {
        console.log('Window resized to mobile view, calling setupMobileMenu');
        setupMobileMenu();
    }
});

if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('DOM already complete, calling setupMobileMenu immediately');
    setupMobileMenu();
    setActiveNavItem();
}

function performGameSearch() {
    console.log('Game search functionality not implemented yet.');
}

function updateDemoList() {
    console.log('Game demos are currently non-functioning and are just placeholders.');
}