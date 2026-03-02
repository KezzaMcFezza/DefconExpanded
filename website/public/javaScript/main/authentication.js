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

function isUserLoggedIn() {
  return !!localStorage.getItem('token');
}

function updateUIForLoginState(isLoggedIn, loggedInUsername) {
  const loggedOutActions = document.querySelectorAll('.logged-out-actions');
  const loggedInActions = document.querySelectorAll('.logged-in-actions');
  const loggedInUsernameSpans = document.querySelectorAll('.logged-in-username');

  if (isLoggedIn) {
    loggedOutActions.forEach(action => action.style.display = 'none');
    loggedInActions.forEach(action => action.style.display = 'flex');
    loggedInUsernameSpans.forEach(span => { span.textContent = loggedInUsername; });
  } else {
    loggedOutActions.forEach(action => action.style.display = 'flex');
    loggedInActions.forEach(action => action.style.display = 'none');
  }
}

function setupProfileLinks() {
  const profileLinks = document.querySelectorAll('.profile-link');

  profileLinks.forEach(profileLink => {
    fetch('/api/current-user')
      .then(response => {
        if (!response.ok) {
        }
        return response.json();
      })
      .then(data => {
        if (data.user && data.user.username) {
          profileLink.href = `/profile/${data.user.username}`;
        } else {
          profileLink.href = '#';
        }
      })
      .catch(error => {
        console.error(error);
      });

    profileLink.addEventListener('click', function (e) {
      if (!profileLink.href || profileLink.href === '#') {
        e.preventDefault();
      } else {
        window.location.href = profileLink.href;
      }
    });
  });

  const loggedInUsernameSpans = document.querySelectorAll('.logged-in-username');
  if (loggedInUsernameSpans.length > 0) {
    fetch('/api/current-user')
      .then(response => response.json())
      .then(data => {
        if (data.user && data.user.username) {
          loggedInUsernameSpans.forEach(span => { span.textContent = data.user.username; });
        }
      });
  }
}

function setupProfileDropdown() {
  const userDropdowns = document.querySelectorAll('.user-dropdown');

  userDropdowns.forEach(function (userDropdown) {
    const dropbtn = userDropdown.querySelector('.dropbtn');
    const dropdownContent = userDropdown.querySelector('.dropdown-content-page');
    if (!dropbtn || !dropdownContent) return;

    dropbtn.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      userDropdown.classList.toggle('open');
    });

    document.addEventListener('click', function (e) {
      if (!userDropdown.contains(e.target)) {
        userDropdown.classList.remove('open');
      }
    });
  });
}

function setupSignout() {
  const signoutButtons = document.querySelectorAll('.signout');

  if (signoutButtons.length > 0) {
    signoutButtons.forEach(signoutButton => {
      signoutButton.addEventListener('click', async function (event) {
        event.preventDefault();
        const confirmLogout = await window.confirm("Are you sure you want to log out?");
        if (confirmLogout) {
          fetch('/api/logout', { method: 'POST' })
            .then(response => response.json())
            .then(async data => {
              if (data.message === 'Logged out successfully') {
                localStorage.removeItem('token');
                updateUIForLoginState(false);
                await window.alert("You have been successfully logged out.");
                window.location.reload();
              }
            });
        }
      });
    });
  }
}

async function checkAuthStatus() {
  try {
    const response = await fetch('/api/checkAuth');
    const data = await response.json();
    
    if (data.isLoggedIn) {
      localStorage.setItem('token', data.token);
    }
    
    updateUIForLoginState(data.isLoggedIn, data.username);
    return data.isLoggedIn;
  } catch (error) {
    console.error('Error checking auth status:', error);
    return false;
  }
}

async function initializeAuthentication() {
  await checkAuthStatus();
  setupProfileLinks();
  setupProfileDropdown();
  setupSignout();
}

export {
  isUserLoggedIn,
  updateUIForLoginState,
  setupProfileLinks,
  setupSignout,
  checkAuthStatus,
  initializeAuthentication
};