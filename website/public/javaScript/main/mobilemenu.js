
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
//Last Edited 02-03-2026

function setActiveNavItem() {
    const currentPath = window.location.pathname;
    const navItems = document.querySelectorAll('#sidebar .list-items li, .top-nav .list-items li');
  
    navItems.forEach(item => {
      const link = item.querySelector('a');
      if (!link) return;
      item.classList.remove('active');
      const href = link.getAttribute('href');
      if (currentPath === href || (href !== '/' && currentPath.startsWith(href + '/'))) {
        item.classList.add('active');
        if (item.classList.contains('dropdown')) {
          const content = item.querySelector('.dropdown-content');
          if (content) content.classList.add('show');
        }
      }
    });
  }
  
  function setupMobileMenu() {
    const sidebar = document.getElementById('sidebar');
    const topNav = document.querySelector('.top-nav');
    const listItems = topNav ? topNav.querySelector('.nav-links .list-items') : (sidebar ? sidebar.querySelector('.list-items') : null);
    const toggleBtn = topNav ? topNav.querySelector('.nav-toggle') : null;
    const title = sidebar ? sidebar.querySelector('.title') : null;
    const dropdowns = (sidebar || topNav) ? (sidebar || topNav).querySelectorAll('.dropdown') : [];

    if (topNav && listItems && toggleBtn) {
      toggleBtn.addEventListener('click', function (e) {
        e.preventDefault();
        e.stopPropagation();
        const isOpen = listItems.classList.toggle('show');
        toggleBtn.setAttribute('aria-expanded', isOpen);
      });
      const navLinks = topNav.querySelector('.nav-links');
      function closeMobileMenu() {
        listItems.classList.remove('show');
        toggleBtn.setAttribute('aria-expanded', 'false');
      }
      listItems.addEventListener('click', function (e) {
        if (e.target.closest('a')) closeMobileMenu();
      });
      if (navLinks) {
        navLinks.addEventListener('click', function (e) {
          if (e.target.closest('.nav-dropdown-account a, .nav-dropdown-account .signout')) closeMobileMenu();
        });
      }
      document.addEventListener('click', function (e) {
        if (listItems.classList.contains('show') && !topNav.contains(e.target)) {
          listItems.classList.remove('show');
          toggleBtn.setAttribute('aria-expanded', 'false');
        }
      });
    } else if (sidebar && title && listItems) {
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
    }

    dropdowns.forEach(dropdown => {
      const dropdownLink = dropdown.querySelector('a');
      const dropdownContent = dropdown.querySelector('.dropdown-content');
      if (dropdownLink && dropdownContent) {
        dropdownLink.addEventListener('click', function (e) {
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
    const pages = ['index.html', 'about.html', 'news.html', 'laikasdefcon.html', '404.html', 'media.html', 'matchroom.html'];
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