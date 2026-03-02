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

function setupProfileLinks(isLoggedIn, username) {
  const profileLinks = document.querySelectorAll('.profile-link');
  profileLinks.forEach(profileLink => {
    profileLink.href = isLoggedIn && username ? `/profile/${username}` : '#';
    profileLink.addEventListener('click', function (e) {
      if (!profileLink.href || profileLink.href === '#') {
        e.preventDefault();
      } else {
        window.location.href = profileLink.href;
      }
    });
  });

  const loggedInUsernameSpans = document.querySelectorAll('.logged-in-username');
  if (loggedInUsernameSpans.length > 0 && isLoggedIn && username) {
    loggedInUsernameSpans.forEach(span => { span.textContent = username; });
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
      if (data.token) localStorage.setItem('token', data.token);
    }

    updateUIForLoginState(data.isLoggedIn, data.username);
    return { isLoggedIn: !!data.isLoggedIn, username: data.username || null };
  } catch (error) {
    console.error('Error checking auth status:', error);
    return { isLoggedIn: false, username: null };
  }
}

let authStatePromise = null;

function getAuthState() {
  if (!authStatePromise) authStatePromise = checkAuthStatus();
  return authStatePromise;
}

async function initializeAuthentication() {
  const auth = await getAuthState();
  setupProfileLinks(auth.isLoggedIn, auth.username);
  setupProfileDropdown();
  setupSignout();
}

export {
  isUserLoggedIn,
  updateUIForLoginState,
  setupProfileLinks,
  setupSignout,
  checkAuthStatus,
  getAuthState,
  initializeAuthentication
};