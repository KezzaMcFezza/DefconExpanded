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

    console.log('Sidebar:', sidebar);
    console.log('Title:', title);
    console.log('List Items:', listItems);

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
            updateDemoList(); // Refresh the demo list to show admin controls
            makeContentEditable(); // Make content editable for admin
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
            updateDemoList(); // Refresh the demo list to hide admin controls
            removeEditability(); // Remove edit buttons and editability when logging out
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
        card.insertBefore(saveButton, card.firstChild);

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
    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');

    if (editButton.textContent === 'Edit') {
        editButton.textContent = 'Cancel';
        saveButton.style.display = 'inline-block';
        elements.forEach(el => {
            el.contentEditable = 'true';
            el.classList.add('editable');
        });
    } else {
        editButton.textContent = 'Edit';
        saveButton.style.display = 'none';
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
        location.reload(); // Revert changes without saving
    }
}

function removeEditability() {
    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach(card => {
        const editButton = card.querySelector('.edit-button');
        const saveButton = card.querySelector('.save-button');
        if (editButton) editButton.remove();
        if (saveButton) saveButton.remove();

        const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });
    });
}

async function saveChanges(card, index) {
    if (!isAdmin) return;

    const elements = card.querySelectorAll('h1, h2, h3, p, a, li');
    const content = {};

    elements.forEach((el, i) => {
        content[`${el.tagName.toLowerCase()}_${i}`] = {
            outerHTML: el.outerHTML,
            text: el.innerText,
            href: el.tagName.toLowerCase() === 'a' ? el.getAttribute('href') : null
        };
    });

    try {
        const response = await fetch('/api/updateContent', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ cardIndex: index, content }),
        });

        if (response.ok) {
            alert('Content updated successfully');
            // Disable editing after saving
            elements.forEach(el => {
                el.contentEditable = 'false';
                el.classList.remove('editable');
            });
            const editButton = card.querySelector('.edit-button');
            const saveButton = card.querySelector('.save-button');
            editButton.textContent = 'Edit';
            saveButton.style.display = 'none';
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to update content');
        }
    } catch (error) {
        console.error('Error updating content:', error);
        alert('An error occurred while updating content');
    }
}

document.addEventListener('DOMContentLoaded', async function() {
    console.log('DOM fully loaded and ready to clap alien cheeks');

    try {
        const response = await fetch('/api/checkAuth');
        const data = await response.json();
        if (data.isAdmin) {
            isAdmin = true;
            showLogoutButton();
            makeContentEditable();
        } else {
            isAdmin = false;
            showLoginForm();
            removeEditability();
        }
    } catch (error) {
        console.error('Error checking authentication:', error);
        showLoginForm();
        removeEditability();
    }

    setActiveNavItem();

    setupMobileMenu();
    console.log('Mobile menu setup completed');

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
});

// Function to handle smooth scrolling
function smoothScroll(target) {
    const element = document.querySelector(target);
    if (element) {
        window.scrollTo({
            top: element.offsetTop,
            behavior: 'smooth'
        });
    }
}

// Set up smooth scrolling for anchor links
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
    console.log('DOM already complete, calling setupMobileMenu immediately and preparing to find the snake :)');
    setupMobileMenu();
    setActiveNavItem();
}