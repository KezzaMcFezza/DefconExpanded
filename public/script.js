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
            if (item.classList.contains('dropdown')) {
                item.querySelector('.dropdown-content').classList.add('show');
            }
        }
    });
}

const discordEvent = {
    title: "Community Game Night",
    date: "Sunday, Aug 18th",
    time: "3PM PM EST"
  };
  
  function updateDiscordWidget() {
    fetch('/api/discord-widget')
      .then(response => response.json())
      .then(data => {
        const widgetContainer = document.getElementById('discord-widget');
        if (!widgetContainer) return;
  
        let html = `
          <div class="discord-widget">
            <div class="discord-header">
              <img src="/images/discord-logo.png" alt="Discord" class="discord-logo">
              <span class="discord-title"></span>
              <span class="discord-online-count">${data.presence_count} Online</span>
            </div>
            <div class="discord-event">
              <h3 class="event-title">${discordEvent.title}</h3>
              <p class="event-date">${discordEvent.date}</p>
              <p class="event-time">${discordEvent.time}</p>
            </div>
            <a href="${data.instant_invite}" target="_blank" rel="noopener noreferrer" class="discord-join-button">
              Join Discord
            </a>
          </div>
        `;
  
        widgetContainer.innerHTML = html;
      })
      .catch(error => console.error('Error updating Discord widget:', error));
  }
  
  updateDiscordWidget();
  setInterval(updateDiscordWidget, 5 * 60 * 1000);

function setupMobileMenu() {
    console.log('Setting up mobile menu');
    const sidebar = document.getElementById('sidebar');
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');
    const dropdowns = sidebar.querySelectorAll('.dropdown');

    if (title && listItems) {
        title.addEventListener('click', function(e) {
            e.preventDefault();
            console.log('Title clicked');
            listItems.classList.toggle('show');
            title.classList.toggle('menu-open'); 
            console.log('Menu toggled, show class:', listItems.classList.contains('show'));
        });

        listItems.addEventListener('click', function(e) {
            if (e.target.tagName === 'A' && !e.target.parentElement.classList.contains('dropdown')) {
                console.log('Link clicked, closing menu');
                listItems.classList.remove('show');
                title.classList.remove('menu-open'); 
            }
        });

        dropdowns.forEach(dropdown => {
            const dropdownLink = dropdown.querySelector('a');
            const dropdownContent = dropdown.querySelector('.dropdown-content');
            dropdownLink.addEventListener('click', function(e) {
                e.preventDefault();
                dropdownContent.classList.toggle('show');
                dropdowns.forEach(otherDropdown => {
                    if (otherDropdown !== dropdown) {
                        otherDropdown.querySelector('.dropdown-content').classList.remove('show');
                    }
                });
            });
        });
    } else {
        console.error('Could not find title or list items elements');
    }
}

function togglePatchNotes(button) {
    const details = button.closest('.patchnote-content').querySelector('.patchnote-details');
    if (details.style.display === 'none') {
      details.style.display = 'block';
      button.textContent = 'Hide patch notes';
    } else {
      details.style.display = 'none';
      button.textContent = 'View patch notes';
    }
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
            alert('Logged out successfully. The page will now refresh.');
            
            window.location.reload();
        } else {
            alert('Logout failed');
        }
    } catch (error) {
        console.error('Logout error:', error);
        alert('An error occurred during logout');
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
            if (!card.querySelector('.edit-button')) {
                const editButton = document.createElement('button');
                editButton.textContent = 'Edit';
                editButton.className = 'edit-button';
                editButton.onclick = () => toggleEditMode(card, index);
                card.insertBefore(editButton, card.firstChild);
            }
        } else {
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
        exitEditMode(card);
    }
}

function exitEditMode(card) {
    const editButton = card.querySelector('.edit-button');
    editButton.textContent = 'Edit';

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
        
        const br = document.createElement('br');
        
        const selection = window.getSelection();
        const range = selection.getRangeAt(0);
        range.deleteContents();
        range.insertNode(br);
        
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
    
    const lastTextElement = Array.from(card.children).reverse().find(el => 
        ['P', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6'].includes(el.tagName) || (el.textContent && el.textContent.trim() !== '')
    );

    if (lastTextElement) {
        lastTextElement.after(newHeading);
    } else {
        card.appendChild(newHeading);
    }

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
    
    const lastTextElement = Array.from(card.children).reverse().find(el => 
        ['P', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6'].includes(el.tagName) || (el.textContent && el.textContent.trim() !== '')
    );

    if (lastTextElement) {
        lastTextElement.after(newParagraph);
    } else {
        card.appendChild(newParagraph);
    }

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
    const links = element.querySelectorAll('a');
    links.forEach((link, index) => {
        if (index > 0 && link.href === links[index - 1].href && link.textContent === links[index - 1].textContent) {
            link.remove();
        }
    });

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

    const editingElements = card.querySelectorAll('.edit-button, .save-button, .cancel-button, .formatting-toolbar');
    editingElements.forEach(el => el.remove());

    const cardClone = card.cloneNode(true);

    const paragraphs = cardClone.querySelectorAll('p');
    paragraphs.forEach(paragraph => {
        const links = paragraph.querySelectorAll('a');
        if (links.length > 1) {
            const firstLink = links[0];
            paragraph.innerHTML = paragraph.innerHTML.replace(/<a\b[^>]*>(.*?)<\/a>/g, '$1');
            paragraph.innerHTML = paragraph.innerHTML.replace(firstLink.textContent, firstLink.outerHTML);
        }
    });

    const contentElements = cardClone.querySelectorAll('h1, h2, h3, h4, h5, h6, p, li');
    contentElements.forEach((el, i) => {
        content[`element_${i}`] = {
            outerHTML: el.outerHTML,
            isHTML: true
        };
    });

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
        exitEditMode(card);
        makeContentEditable();
    }
}

function cancelEdit(card) {
    exitEditMode(card);
    location.reload(); 
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
            addDemoToList(data.demoName); 
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
            hideUploadForm();
        }
        makeContentEditable(); 
    } catch (error) {
        console.error('Error checking authentication:', error);
        window.isAdmin = false;
        hideUploadForm();
        makeContentEditable();
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

    const bugReportForm = document.getElementById('bug-report-form');
    if (bugReportForm) {
        bugReportForm.addEventListener('submit', submitBugReport);
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

async function submitBugReport(event) {
    event.preventDefault();

    const bugTitle = document.querySelector('input[name="bug-title"]').value;
    const bugDescription = document.querySelector('textarea[name="bug-description"]').value;

    try {
        const response = await fetch('/api/report-bug', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ bugTitle, bugDescription }),
        });

        if (response.ok) {
            const data = await response.json();
            alert(data.message);
            event.target.reset(); 
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to submit bug report');
        }
    } catch (error) {
        console.error('Error submitting bug report:', error);
        alert('An error occurred while submitting the bug report');
    }
}