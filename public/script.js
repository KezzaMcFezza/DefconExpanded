// Global variables
let soundVolume = 0.1;
window.isAdmin = false; 

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
            window.isAdmin = true;
            console.log('Logged in, isAdmin set to:', window.isAdmin);
            alert('Logged in successfully. The page will now refresh.');
            // Force a page refresh
            window.location.reload();
        } else {
            alert(data.error || 'Login failed');
        }
    } catch (error) {
        console.error('Login error:', error);
        alert('An error occurred during login');
    }
}

async function logout() {
    try {
        const response = await fetch('/api/logout', { method: 'POST' });
        if (response.ok) {
            window.isAdmin = false;
            console.log('Logged out, isAdmin set to:', window.isAdmin);
            alert('Logged out successfully');
            document.getElementById('logout-container').remove();
            showLoginForm();
            hideUploadForm();
            updateDemoList();
            makeContentEditable(); // This will now set everything to non-editable
            // Force update of resources if on resources page
            if (typeof updateResourceList === 'function') {
                updateResourceList();
            }
            location.reload();
        } else {
            alert('Logout failed');
        }
    } catch (error) {
        console.error('Logout error:', error);
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

function makeContentEditable() {
    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach((card, index) => {
        const elements = card.querySelectorAll('h1, h2, h3, h4, h5, h6, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'false';
            el.classList.remove('editable');
        });

        if (window.isAdmin) {
            // Only add edit buttons if user is admin
            if (!card.querySelector('.edit-button')) {
                const editButton = document.createElement('button');
                editButton.textContent = 'Edit';
                editButton.className = 'edit-button';
                editButton.onclick = () => toggleEditMode(card, index);
                card.insertBefore(editButton, card.firstChild);
            }
        } else {
            // Remove edit buttons if user is not admin
            const editButton = card.querySelector('.edit-button');
            if (editButton) editButton.remove();
        }
    });
}

function toggleEditMode(card, index) {
    if (!window.isAdmin) return;

    const editButton = card.querySelector('.edit-button');
    const isEnteringEditMode = editButton.textContent === 'Edit';

    if (isEnteringEditMode) {
        // Entering edit mode
        editButton.textContent = 'Exit Edit';
        
        const saveButton = document.createElement('button');
        saveButton.textContent = 'Save';
        saveButton.className = 'save-button';
        saveButton.onclick = () => saveChanges(card, index);
        card.insertBefore(saveButton, editButton.nextSibling);
        
        const cancelButton = document.createElement('button');
        cancelButton.textContent = 'Cancel';
        cancelButton.className = 'cancel-button';
        cancelButton.onclick = () => cancelEdit(card);
        card.insertBefore(cancelButton, saveButton.nextSibling);
        
        const formattingToolbar = createFormattingToolbar(card);
        formattingToolbar.style.display = 'block';
        card.insertBefore(formattingToolbar, cancelButton.nextSibling);

        const elements = card.querySelectorAll('h1, h2, h3, h4, h5, h6, p, a, li');
        elements.forEach(el => {
            el.contentEditable = 'true';
            el.classList.add('editable');
            el.addEventListener('keydown', handleEnterKey);
        });
    } else {
        // Exiting edit mode
        exitEditMode(card);
    }
}

function exitEditMode(card) {
    const editButton = card.querySelector('.edit-button');
    editButton.textContent = 'Edit';

    // Remove all editing elements
    const editingElements = card.querySelectorAll('.save-button, .cancel-button, .formatting-toolbar');
    editingElements.forEach(el => el.remove());

    const elements = card.querySelectorAll('h1, h2, h3, h4, h5, h6, p, a, li');
    elements.forEach(el => {
        el.contentEditable = 'false';
        el.classList.remove('editable');
        el.removeEventListener('keydown', handleEnterKey);
    });
}

function handleEnterKey(event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        
        // Create a line break element
        const br = document.createElement('br');
        
        // Insert the line break at the current cursor position
        const selection = window.getSelection();
        const range = selection.getRangeAt(0);
        range.deleteContents();
        range.insertNode(br);
        
        // Move the cursor after the line break
        range.setStartAfter(br);
        range.setEndAfter(br);
        selection.removeAllRanges();
        selection.addRange(range);
    }
}

function createFormattingToolbar(card) {
    console.log('Creating formatting toolbar');
    const toolbar = document.createElement('div');
    toolbar.className = 'formatting-toolbar';

    const headingSelect = document.createElement('select');
    headingSelect.innerHTML = `
        <option value="h1">Heading 1</option>
        <option value="h2" selected>Heading 2</option>
        <option value="h3">Heading 3</option>
        <option value="h4">Heading 4</option>
        <option value="h5">Heading 5</option>
        <option value="h6">Heading 6</option>
    `;

    const colorPicker = document.createElement('input');
    colorPicker.type = 'color';
    colorPicker.onchange = () => applyColor(card, colorPicker.value);

    const addParagraphButton = document.createElement('button');
    addParagraphButton.textContent = 'Add Paragraph';
    addParagraphButton.onclick = () => addParagraph(card);

    const addHeadingButton = document.createElement('button');
    addHeadingButton.textContent = 'Add Heading';
    addHeadingButton.onclick = () => addHeading(card, headingSelect.value);

    const removeParagraphButton = document.createElement('button');
    removeParagraphButton.textContent = 'Remove Paragraph';
    removeParagraphButton.onclick = () => removeParagraph(card);

    toolbar.appendChild(headingSelect);
    toolbar.appendChild(colorPicker);
    toolbar.appendChild(addParagraphButton);
    toolbar.appendChild(addHeadingButton);
    toolbar.appendChild(removeParagraphButton);

    console.log('Formatting toolbar created:', toolbar);
    return toolbar;
}

function addHeading(card, headingLevel) {
    const newHeading = document.createElement(headingLevel);
    newHeading.textContent = `New ${headingLevel.toUpperCase()}`;
    newHeading.contentEditable = 'true';
    newHeading.classList.add('editable');
    
    // Find the last paragraph or text content in the card
    const lastTextElement = Array.from(card.children).reverse().find(el => 
        ['P', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6'].includes(el.tagName) || (el.textContent && el.textContent.trim() !== '')
    );

    if (lastTextElement) {
        lastTextElement.after(newHeading);
    } else {
        card.appendChild(newHeading);
    }

    // Focus on the new heading
    newHeading.focus();
}

function applyColor(card, color) {
    document.execCommand('foreColor', false, color);
}

function addParagraph(card) {
    const newParagraph = document.createElement('p');
    newParagraph.textContent = 'New paragraph';
    newParagraph.contentEditable = 'true';
    newParagraph.classList.add('editable');
    
    // Find the last paragraph or text content in the card
    const lastTextElement = Array.from(card.children).reverse().find(el => 
        ['P', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6'].includes(el.tagName) || (el.textContent && el.textContent.trim() !== '')
    );

    if (lastTextElement) {
        lastTextElement.after(newParagraph);
    } else {
        card.appendChild(newParagraph);
    }

    // Focus on the new paragraph
    newParagraph.focus();
}

function removeParagraph(card) {
    const paragraphs = card.querySelectorAll('p, h1, h2, h3, h4, h5, h6');
    if (paragraphs.length > 1) {
        const lastParagraph = paragraphs[paragraphs.length - 1];
        lastParagraph.remove();
    } else {
        alert('Cannot remove the last text element.');
    }
}

function cleanupContent(element) {
    // Remove any duplicate adjacent links
    const links = element.querySelectorAll('a');
    links.forEach((link, index) => {
        if (index > 0 && link.href === links[index - 1].href && link.textContent === links[index - 1].textContent) {
            link.remove();
        }
    });

    // Remove any empty elements
    const allElements = element.querySelectorAll('*');
    allElements.forEach(el => {
        if (el.innerHTML.trim() === '' && !['BR', 'HR'].includes(el.tagName)) {
            el.remove();
        }
    });

    return element;
}

async function saveChanges(card, index) {
    if (!window.isAdmin) return;

    const content = {};
    const pageName = getPageName();

    // Temporarily remove editing elements
    const editingElements = card.querySelectorAll('.edit-button, .save-button, .cancel-button, .formatting-toolbar');
    editingElements.forEach(el => el.remove());

    // Clone the card to avoid modifying the original content
    const cardClone = card.cloneNode(true);

    // Clean up links
    const paragraphs = cardClone.querySelectorAll('p');
    paragraphs.forEach(paragraph => {
        const links = paragraph.querySelectorAll('a');
        if (links.length > 1) {
            // Keep only the first link and remove others
            const firstLink = links[0];
            paragraph.innerHTML = paragraph.innerHTML.replace(/<a\b[^>]*>(.*?)<\/a>/g, '$1');
            paragraph.innerHTML = paragraph.innerHTML.replace(firstLink.textContent, firstLink.outerHTML);
        }
    });

    // Collect all content, excluding editing elements and media blocks
    const contentElements = cardClone.querySelectorAll('h1, h2, h3, h4, h5, h6, p, li');
    contentElements.forEach((el, i) => {
        content[`element_${i}`] = {
            outerHTML: el.outerHTML,
            isHTML: true
        };
    });

    // Preserve media block if it exists
    const mediaBlock = cardClone.querySelector('.media-block');
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
            // Update the original card with the cleaned-up content
            card.innerHTML = cardClone.innerHTML;
            makeContentEditable();
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to update content');
        }
    } catch (error) {
        console.error('Error updating content:', error);
        alert('An error occurred while updating content');
    } finally {
        // Restore edit mode
        exitEditMode(card);
        makeContentEditable();
    }
}

function cancelEdit(card) {
    exitEditMode(card);
    location.reload(); // Revert changes without saving
}

function removeEditability() {
    const cards = document.querySelectorAll('.card, .cardcredits');
    cards.forEach(card => {
        exitEditMode(card);
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
        adminUpload.style.display = window.isAdmin ? 'block' : 'none';
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
        window.isAdmin = data.isAdmin;
        if (window.isAdmin) {
            showLogoutButton();
            showUploadForm();
        } else {
            showLoginForm();
            hideUploadForm();
        }
        makeContentEditable(); // This will now handle both admin and non-admin cases
    } catch (error) {
        console.error('Error checking authentication:', error);
        window.isAdmin = false;
        showLoginForm();
        hideUploadForm();
        makeContentEditable(); // Ensure content is not editable in case of error
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