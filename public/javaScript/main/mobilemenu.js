
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
  
  function setupMobileMenu() {
    const sidebar = document.getElementById('sidebar');
    if (!sidebar) return;
    
    const title = sidebar.querySelector('.title');
    const listItems = sidebar.querySelector('.list-items');
    const dropdowns = sidebar.querySelectorAll('.dropdown');
  
    if (title && listItems) {
      title.addEventListener('click', function (e) {
        e.preventDefault();
        listItems.classList.toggle('show');
        title.classList.toggle('menu-open');
      });
  
      listItems.addEventListener('click', function (e) {
        if (e.target.tagName === 'A' && !e.target.parentElement.classList.contains('dropdown')) {
          listItems.classList.remove('show');
          title.classList.remove('menu-open');
        }
      });
  
      dropdowns.forEach(dropdown => {
        const dropdownLink = dropdown.querySelector('a');
        const dropdownContent = dropdown.querySelector('.dropdown-content');
        dropdownLink.addEventListener('click', function (e) {
          e.preventDefault();
          dropdownContent.classList.toggle('show');
          dropdowns.forEach(otherDropdown => {
            if (otherDropdown !== dropdown) {
              otherDropdown.querySelector('.dropdown-content').classList.remove('show');
            }
          });
        });
      });
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
  
  function getPageName() {
    const path = window.location.pathname;
    const pages = ['index.html', 'about.html', 'resources.html', 'news.html', 'laikasdefcon.html', '404.html', 'media.html', 'matchroom.html'];
    let pageName = pages.find(page => path.endsWith(page));
  
    if (!pageName) {
      pageName = path === '/' ? 'index.html' : path.split('/').pop() + '.html';
    }
  
    return pageName;
  }
  
  function setupDropdownSections() {
    const sections = document.querySelectorAll('.installation-section');
  
    sections.forEach(section => {
      const header = section.querySelector('.dropdown-header');
      const content = section.querySelector('.dropdown-content');
      const arrow = section.querySelector('.arrow');
  
      if (header && content && arrow) {
        header.addEventListener('click', () => {
          arrow.classList.toggle('up');
          content.classList.toggle('show');
  
          sections.forEach(otherSection => {
            if (otherSection !== section) {
              otherSection.querySelector('.arrow')?.classList.remove('up');
              otherSection.querySelector('.dropdown-content')?.classList.remove('show');
            }
          });
        });
      }
    });
  }
  
  function smoothScroll(target) {
    const element = document.querySelector(target);
    if (element) {
      window.scrollTo({
        top: element.offsetTop,
        behavior: 'smooth'
      });
    }
  }
  
  function initializeNavigation() {
    setupMobileMenu();
    setActiveNavItem();
    setupDropdownSections();
    
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
      anchor.addEventListener('click', function (e) {
        e.preventDefault();
        smoothScroll(this.getAttribute('href'));
      });
    });
  }
  
  export {
    setActiveNavItem,
    setupMobileMenu,
    togglePatchNotes,
    getPageName,
    setupDropdownSections,
    smoothScroll,
    initializeNavigation
  };