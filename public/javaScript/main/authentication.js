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

window.isAdmin = false;

window.initializeUserRole = async function () {
  try {
    const response = await fetch('/api/current-user');
    const data = await response.json();
    if (data.user) {
      window.userRole = data.user.role;
    }
  } catch (error) {
    console.error('Error setting user role:', error);
  }
};

function isUserLoggedIn() {
  return !!localStorage.getItem('token');
}

function updateUIForLoginState(isLoggedIn, loggedInUsername) {
  const loggedOutActions = document.querySelectorAll('.logged-out-actions');
  const loggedInActions = document.querySelectorAll('.logged-in-actions');
  const loggedInUsernameSpan = document.querySelectorAll('#logged-in-username');

  if (isLoggedIn) {
    loggedOutActions.forEach(action => action.style.display = 'none');
    loggedInActions.forEach(action => action.style.display = 'flex');
    loggedInUsernameSpan.forEach(span => span.textContent = loggedInUsername);
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

  const loggedInUsername = document.getElementById('logged-in-username');
  if (loggedInUsername) {
    fetch('/api/current-user')
      .then(response => response.json())
      .then(data => {
        if (data.user && data.user.username) {
          loggedInUsername.textContent = data.user.username;
        }
      });
  }
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
  await window.initializeUserRole();
  await checkAuthStatus();
  setupProfileLinks();
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