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


const UI = (() => {
    function setupMobileMenu() {
        const sidebar = document.getElementById('sidebar');
        if (!sidebar) return;
        
        const title = sidebar.querySelector('.title');
        const listItems = sidebar.querySelector('.list-items');
        const dropdowns = sidebar.querySelectorAll('.dropdown');

        if (title && listItems) {
            title.addEventListener('click', function(e) {
                e.preventDefault();
                listItems.classList.toggle('show');
                title.classList.toggle('menu-open');
            });

            listItems.addEventListener('click', function(e) {
                if (e.target.tagName === 'A' && !e.target.parentElement.classList.contains('dropdown')) {
                    listItems.classList.remove('show');
                    title.classList.remove('menu-open');
                }
            });

            dropdowns.forEach(dropdown => {
                const dropdownLink = dropdown.querySelector('a');
                const dropdownContent = dropdown.querySelector('.dropdown-content');
                if (dropdownLink && dropdownContent) {
                    dropdownLink.addEventListener('click', function(e) {
                        e.preventDefault();
                        dropdownContent.classList.toggle('show');
                        dropdowns.forEach(otherDropdown => {
                            if (otherDropdown !== dropdown) {
                                const otherContent = otherDropdown.querySelector('.dropdown-content');
                                if (otherContent) otherContent.classList.remove('show');
                            }
                        });
                    });
                }
            });
        } else {
            console.error('Could not find title or list items elements');
        }
    }

    function setupCustomDialog() {
        const dialogHTML = `
        <div id="custom-dialog" class="custom-dialog">
          <div class="dialog-content">
            <label id="dialog-message"></label>
            <div class="dialog-buttons">
              <button id="dialog-cancel" class="dialog-button">Cancel</button>
              <button id="dialog-confirm" class="dialog-button">OK</button>
            </div>
          </div>
        </div>
        `;

        document.body.insertAdjacentHTML('beforeend', dialogHTML);

        const styles = `
        <style>
          .custom-dialog {
            display: none;
            position: fixed;
            z-index: 1000;
            left: 0;
            top: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0,0,0,0.4);
          }
          .dialog-content {
            background-color: #101010;
            margin: 15% auto;
            padding: 20px;
            width: 350px;
            border-radius: 10px;
            text-align: center;
          }
          .dialog-message {
            color: #1a1a1a;
          }
          #dialog-cancel {
            background-color: #830000;
            color: #ffffff;
          }
          #dialog-confirm {
            background-color: #0067b8;
            color: #ffffff;
          }
          .dialog-button {
            font-size: 0.9rem;
            font-weight: bold;
            padding: 5px 20px;
            margin: 20px 10px 0px 10px;
            border-radius: 10px;
            cursor: pointer;
            border: none;
          }
        </style>
        `;

        document.head.insertAdjacentHTML('beforeend', styles);

        window.confirm = function(message) {
            return new Promise((resolve) => {
                const dialog = document.getElementById('custom-dialog');
                const messageElement = document.getElementById('dialog-message');
                const confirmButton = document.getElementById('dialog-confirm');
                const cancelButton = document.getElementById('dialog-cancel');

                if (!dialog || !messageElement || !confirmButton || !cancelButton) {
                    console.error('Dialog elements not found');
                    resolve(false);
                    return;
                }

                messageElement.textContent = message;
                dialog.style.display = 'block';
                cancelButton.style.display = 'inline-block';

                const closeDialog = (result) => {
                    dialog.style.display = 'none';
                    resolve(result);
                };

                confirmButton.onclick = () => closeDialog(true);
                cancelButton.onclick = () => closeDialog(false);
            });
        };

        window.alert = function(message) {
            return new Promise((resolve) => {
                const dialog = document.getElementById('custom-dialog');
                const messageElement = document.getElementById('dialog-message');
                const confirmButton = document.getElementById('dialog-confirm');
                const cancelButton = document.getElementById('dialog-cancel');

                if (!dialog || !messageElement || !confirmButton || !cancelButton) {
                    console.error('Dialog elements not found');
                    resolve();
                    return;
                }

                messageElement.textContent = message;
                dialog.style.display = 'block';
                cancelButton.style.display = 'none';

                const closeDialog = () => {
                    dialog.style.display = 'none';
                    resolve();
                };

                confirmButton.onclick = closeDialog;
            });
        };
    }

    function showAlert(message) {
        return window.alert(message);
    }

    function showConfirm(message) {
        return window.confirm(message);
    }

    function setActiveNavItem() {
        const path = window.location.pathname;
        const navItems = document.querySelectorAll('#sidebar .list-items a');
        
        navItems.forEach(item => {
            const href = item.getAttribute('href');
            if (href && path.includes(href) && href !== '/') {
                item.classList.add('active');
            } else {
                item.classList.remove('active');
            }
        });
    }

    function scrollToTop() {
        const pageContainer = document.getElementById('page-container');
        if (!pageContainer) return;
        
        pageContainer.classList.add('scrolling');

        setTimeout(() => {
            window.scrollTo({
                top: 0,
                behavior: 'instant'
            });

            pageContainer.classList.remove('scrolling');
        }, 500);
    }

    function showElement(id) {
        const element = document.getElementById(id);
        if (element) element.style.display = 'block';
    }

    function hideElement(id) {
        const element = document.getElementById(id);
        if (element) element.style.display = 'none';
    }

    function toggleSpectators(button) {
        const list = button.nextElementSibling;
        if (list) {
            const isVisible = list.style.display === 'block';
            list.style.display = isVisible ? 'none' : 'block';
            button.innerHTML = isVisible ? 
                '<i class="fas fa-eye"></i> Show Spectators' : 
                '<i class="fas fa-eye-slash"></i> Hide Spectators';
        }
    }

    return {
        setupMobileMenu,
        setupCustomDialog,
        showAlert,
        showConfirm,
        setActiveNavItem,
        scrollToTop,
        showElement,
        hideElement,
        toggleSpectators
    };
})();

export default UI;