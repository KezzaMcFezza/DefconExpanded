
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

const NAVBAR_HTML = `
<div class="top-nav-inner">
  <a href="/" class="nav-brand">
    <img src="/images/Icon3expanded.png" alt="" class="nav-brand-icon">
    <span class="nav-brand-text">DefconExpanded</span>
  </a>
  <button type="button" class="nav-toggle" aria-label="Toggle menu" aria-expanded="false" aria-controls="nav-menu"><i class="fas fa-bars"></i></button>
  <nav class="nav-links" id="nav-menu" aria-label="Main navigation">
    <ul class="list-items">
      <li><a href="/" id="nav-homepage">Recordings</a></li>
      <li><a href="/about" id="nav-about">About</a></li>
      <li><a href="/dedcon-builds" id="nav-dedcon-builds">Dedcon</a></li>
      <li><a href="/leaderboard" id="nav-leaderboard">Leaderboard</a></li>
      <li><a href="/smurfchecker" id="nav-smurfchecker">Smurf Checker</a></li>
      <li><a href="/modlist" id="nav-modlist">Modlist</a></li>
    </ul>
    <div class="nav-dropdown-account">
      <div class="user-actions">
        <div class="logged-out-actions">
          <a href="/login" class="login-signup" target="_blank">Login</a>
          <a href="/signup" class="login-signup" target="_blank">Sign up</a>
        </div>
        <div class="logged-in-actions" style="display: none;">
          <div class="user-dropdown">
            <button class="dropbtn">
              <span class="logged-in-username">Logged-in User</span>
              <i class="fa fa-caret-down"></i>
            </button>
            <div class="dropdown-content-page">
              <a href="#" class="profile-link"><i class="fas fa-user"></i>Profile</a>
              <a href="/account-settings"><i class="fas fa-cog"></i>Settings</a>
            </div>
          </div>
          <button class="signout">Logout</button>
        </div>
      </div>
    </div>
  </nav>
  <div class="nav-icons">
    <a href="https://github.com/KezzaMcFezza/DefconExpanded" target="_blank" rel="noopener" class="nav-icon nav-icon-github" aria-label="GitHub"><i class="fab fa-github"></i></a>
    <a href="https://discord.gg/qADkVKY" target="_blank" rel="noopener" class="nav-icon nav-icon-discord" aria-label="Discord"><i class="fab fa-discord"></i></a>
  </div>
  <div class="account-container">
    <div class="user-actions">
      <div class="logged-out-actions">
        <a href="/login" class="login-signup" target="_blank">Login</a>
        <a href="/signup" class="login-signup" target="_blank">Sign up</a>
      </div>
      <div class="logged-in-actions" style="display: none;">
        <div class="user-dropdown">
          <button class="dropbtn">
            <span id="logged-in-username" class="logged-in-username">Logged-in User</span>
            <i class="fa fa-caret-down"></i>
          </button>
          <div class="dropdown-content-page">
            <a href="#" class="profile-link"><i class="fas fa-user"></i>Profile</a>
            <a href="/account-settings"><i class="fas fa-cog"></i>Settings</a>
          </div>
        </div>
        <button class="signout">Logout</button>
      </div>
    </div>
  </div>
</div>
`;

function injectNavbar() {
  const main = document.querySelector('main');
  if (!main || main.querySelector('.top-nav')) return;

  const header = document.createElement('header');
  header.className = 'top-nav';
  header.innerHTML = NAVBAR_HTML;
  main.insertBefore(header, main.firstChild);
}

function initNavbar() {
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', injectNavbar);
  } else {
    injectNavbar();
  }
}

initNavbar();

export { injectNavbar, NAVBAR_HTML };
