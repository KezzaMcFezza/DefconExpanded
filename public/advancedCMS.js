// Advanced CMS Features

let isAdmin = false; // This should be set based on the user's actual admin status

function initializeAdvancedCMS() {
    checkAdminStatus().then(adminStatus => {
        isAdmin = adminStatus;
        if (isAdmin) {
            createInitialLayout();
            makeCardsResizable();
            addLayoutControls();
        }
    });
}

async function checkAdminStatus() {
    try {
        const response = await fetch('/api/checkAuth');
        const data = await response.json();
        return data.isAdmin;
    } catch (error) {
        console.error('Error checking authentication:', error);
        return false;
    }
}

function createInitialLayout() {
    if (!isAdmin) return;

    const mainContent = document.querySelector('main');
    const layoutContainer = document.createElement('div');
    layoutContainer.id = 'layout-container';
    mainContent.appendChild(layoutContainer);

    // Create initial row
    addRow();
}

function addLayoutControls() {
    if (!isAdmin) return;

    const mainContent = document.querySelector('main');
    const controls = document.createElement('div');
    controls.id = 'layout-controls';
    controls.innerHTML = `
        <button onclick="addRow()">Add New Row</button>
        <button onclick="addColumnToLastRow()">Add Column to Last Row</button>
        <button onclick="saveLayout()">Save Layout</button>
    `;
    mainContent.insertBefore(controls, mainContent.firstChild);
}

async function saveLayout() {
    if (!isAdmin) return;

    const layoutContainer = document.getElementById('layout-container');
    const layout = [];

    layoutContainer.querySelectorAll('.grid-row').forEach((row, rowIndex) => {
        const rowData = {
            columns: []
        };

        row.querySelectorAll('.grid-column').forEach((column, columnIndex) => {
            const columnData = {
                cards: []
            };

            column.querySelectorAll('.card').forEach((card, cardIndex) => {
                const cardData = {
                    content: card.innerHTML,
                    style: {
                        width: card.style.width,
                        height: card.style.height
                    }
                };
                columnData.cards.push(cardData);
            });

            rowData.columns.push(columnData);
        });

        layout.push(rowData);
    });

    try {
        const response = await fetch('/api/updateLayout', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ 
                layout: layout,
                pageName: getPageName() // You'll need to implement this function to get the current page name
            }),
        });

        if (response.ok) {
            alert('Layout saved successfully');
        } else {
            const data = await response.json();
            alert(data.error || 'Failed to save layout');
        }
    } catch (error) {
        console.error('Error saving layout:', error);
        alert('An error occurred while saving the layout');
    }
}

async function loadLayout() {
    console.log('Loading layout');
    try {
        const response = await fetch('/api/getLayout');
        if (response.ok) {
            const layout = await response.json();
            console.log('Layout loaded:', layout);
            renderLayout(layout);
        } else {
            console.error('Failed to load layout:', response.statusText);
        }
    } catch (error) {
        console.error('Error loading layout:', error);
    }
}

function renderLayout(layout, isAdmin) {
    console.log('Rendering layout:', layout);
    
    const mainElement = document.querySelector('main');
    if (!mainElement) {
        console.error('Main element not found in the document');
        return;
    }

    // Clear existing content
    mainElement.innerHTML = '';

    if (!layout.rows || layout.rows.length === 0) {
        console.log('No rows in layout, creating a default row');
        layout.rows = [{ columns: [{ cards: [] }] }];
    }

    layout.rows.forEach((rowData, rowIndex) => {
        const row = document.createElement('div');
        row.className = 'grid-row';
        
        rowData.columns.forEach((columnData, columnIndex) => {
            const column = document.createElement('div');
            column.className = 'grid-column';
            
            columnData.cards.forEach((cardData, cardIndex) => {
                const card = document.createElement('div');
                card.className = 'card';
                
                const cardHeader = document.createElement('div');
                cardHeader.className = 'card-header';
                
                const cardTitle = document.createElement('h3');
                cardTitle.textContent = cardData.title || 'New Card';
                cardTitle.contentEditable = isAdmin ? 'true' : 'false';
                
                cardHeader.appendChild(cardTitle);
                
                if (isAdmin) {
                    const removeButton = document.createElement('button');
                    removeButton.textContent = 'Remove';
                    removeButton.className = 'remove-card-button';
                    removeButton.onclick = () => removeCard(card);
                    cardHeader.appendChild(removeButton);
                }
                
                card.appendChild(cardHeader);
                
                const cardContent = document.createElement('div');
                cardContent.className = 'card-content';
                cardContent.contentEditable = isAdmin ? 'true' : 'false';
                cardContent.innerHTML = cardData.content || 'Edit this content';
                
                card.appendChild(cardContent);
                
                if (isAdmin) {
                    const cardResizer = document.createElement('div');
                    cardResizer.className = 'card-resizer';
                    card.appendChild(cardResizer);
                }
                
                if (cardData.style) {
                    if (cardData.style.width) card.style.width = cardData.style.width;
                    if (cardData.style.height) card.style.height = cardData.style.height;
                }
                
                column.appendChild(card);
            });
            
            row.appendChild(column);
        });
        
        mainElement.appendChild(row);
    });

    console.log('Layout rendering complete');
}

function initializeAdvancedCMS() {
    checkAdminStatus().then(adminStatus => {
        isAdmin = adminStatus;
        if (isAdmin) {
            addLayoutControls();
            loadLayout(); // Load existing layout if available
            makeCardsResizable();
        }
    });
}

function addRow() {
    if (!isAdmin) return;
  
    let layoutContainer = document.getElementById('layout-container');
    if (!layoutContainer) {
      // If layout container doesn't exist, create it
      const mainContent = document.querySelector('main');
      layoutContainer = document.createElement('div');
      layoutContainer.id = 'layout-container';
      mainContent.appendChild(layoutContainer);
    }
  
    const row = document.createElement('div');
    row.className = 'grid-row';
    layoutContainer.appendChild(row);
  
    // Add initial column to the new row
    addColumnToRow(row);
  
    // Add row controls
    const rowControls = document.createElement('div');
    rowControls.className = 'row-controls';
    rowControls.innerHTML = `
      <button onclick="addColumnToRow(this.parentElement.previousElementSibling)">Add Column</button>
      <button onclick="removeRow(this.parentElement.previousElementSibling)">Remove Row</button>
    `;
    layoutContainer.appendChild(rowControls);
  }

function addColumnToLastRow() {
    if (!isAdmin) return;

    const rows = document.querySelectorAll('.grid-row');
    if (rows.length > 0) {
        const lastRow = rows[rows.length - 1];
        addColumnToRow(lastRow);
    } else {
        alert('No rows available. Please add a row first.');
    }
}

function addNewCard(mainContent) {
    const newCard = document.createElement('div');
    newCard.className = 'card';
    newCard.innerHTML = '<h2>New Card Title</h2><p>New card content goes here.</p>';
    mainContent.appendChild(newCard);
    makeContentEditable();
}

function createCard() {
    if (!isAdmin) return null;

    const card = document.createElement('div');
    card.className = 'card';
    card.innerHTML = `
        <div class="card-header">
            <h3>New Card</h3>
            <button onclick="removeCard(this.parentElement.parentElement)">Remove</button>
        </div>
        <div class="card-content" contenteditable="true">
            <p>Edit this content</p>
        </div>
    `;
    return card;
}

function addImage(card) {
    const imageUrl = prompt('Enter the URL of the image:');
    if (imageUrl) {
        const img = document.createElement('img');
        img.src = imageUrl;
        img.alt = 'Inserted image';
        img.style.maxWidth = '100%';
        
        const selection = window.getSelection();
        if (selection.rangeCount > 0) {
            const range = selection.getRangeAt(0);
            range.insertNode(img);
        } else {
            card.appendChild(img);
        }
    }
}

function addColumnToRow(row) {
    if (!isAdmin) return;

    const column = document.createElement('div');
    column.className = 'grid-column';
    column.innerHTML = '<div class="column-controls"><button onclick="addCardToColumn(this.parentElement.parentElement)">Add Card</button></div>';
    row.appendChild(column);
}

function removeRow(row) {
    if (!isAdmin) return;

    if (confirm('Are you sure you want to remove this row and all its content?')) {
        const layoutContainer = document.getElementById('layout-container');
        layoutContainer.removeChild(row.nextElementSibling); // Remove row controls
        layoutContainer.removeChild(row); // Remove the row itself
    }
}

function addCardToColumn(column) {
    if (!isAdmin) return;

    const card = createCard();
    column.appendChild(card);
    makeCardResizable(card);
}

function createCard() {
    if (!isAdmin) return null;

    const card = document.createElement('div');
    card.className = 'card';
    card.innerHTML = `
        <div class="card-header">
            <h3>New Card</h3>
            <button onclick="removeCard(this.parentElement.parentElement)">Remove</button>
        </div>
        <div class="card-content" contenteditable="true">
            <p>Edit this content</p>
        </div>
    `;
    return card;
}

function removeCard(card) {
    if (!isAdmin) return;

    card.parentElement.removeChild(card);
}

// Initialize advanced CMS features when the DOM is loaded
document.addEventListener('DOMContentLoaded', initializeAdvancedCMS);