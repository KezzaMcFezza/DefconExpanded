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

import { isUserLoggedIn } from './authentication.js';

function showReportOptions(demoId, event) {
  event.preventDefault();
  event.stopPropagation();

  const options = [
    'Broken download',
    'Incorrect information',
    'Demo file corrupted',
    'Missing player data',
    'Wrong game type'
  ];

  const dropdown = document.createElement('div');
  dropdown.className = 'report-dropdown';

  options.forEach(option => {
    const button = document.createElement('button');
    button.textContent = option;
    button.onclick = () => confirmDemoReport(demoId, option);
    dropdown.appendChild(button);
  });

  const existingDropdown = document.querySelector('.report-dropdown');
  if (existingDropdown) {
    existingDropdown.remove();
  }

  const reportButton = event.target;
  const demoCard = reportButton.closest('.demo-card');
  if (demoCard) {
    const demoActions = reportButton.closest('.demo-actions');
    demoActions.style.position = 'relative';
    demoActions.appendChild(dropdown);

    document.addEventListener('click', closeDemoDropdown);
  }
}

function closeDemoDropdown(event) {
  if (!event.target.matches('.btn-report')) {
    const dropdowns = document.getElementsByClassName('report-dropdown');
    for (let i = 0; i < dropdowns.length; i++) {
      dropdowns[i].remove();
    }
    document.removeEventListener('click', closeDemoDropdown);
  }
}

async function confirmDemoReport(demoId, reportType) {
  const confirmed = await window.confirm(`Are you sure you want to report this demo for ${reportType}?`);
  if (confirmed) {
    submitDemoReport(demoId, reportType);
  }
}

async function submitDemoReport(demoId, reportType) {
  try {
    const response = await fetch('/api/report-demo', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ demoId, reportType }),
    });

    if (response.ok) {
      window.alert('Demo report successfully sent to the team!');
    } else {
      throw new Error('You need to be logged in to report demos.');
    }
  } catch (error) {
    window.alert('You need to be logged in to report demos.');
  }
}

function showModReportOptions(modId, event) {
  event.preventDefault();
  event.stopPropagation();

  const options = [
    'Broken download',
    'Incorrect information',
    'Malicious content',
    'Hard install process',
    'Does not work as expected'
  ];

  const dropdown = document.createElement('div');
  dropdown.className = 'report-dropdown';

  options.forEach(option => {
    const button = document.createElement('button');
    button.textContent = option;
    button.onclick = () => confirmModReport(modId, option);
    dropdown.appendChild(button);
  });

  const existingDropdown = document.querySelector('.report-dropdown');
  if (existingDropdown) {
    existingDropdown.remove();
  }

  const reportButton = event.target;
  const modItem = reportButton.closest('.mod-info');
  modItem.style.position = 'relative';
  modItem.appendChild(dropdown);

  document.addEventListener('click', closeModDropdown);
}

function closeModDropdown(event) {
  if (!event.target.matches('.btn-report')) {
    const dropdowns = document.getElementsByClassName('report-dropdown');
    for (let i = 0; i < dropdowns.length; i++) {
      dropdowns[i].remove();
    }
    document.removeEventListener('click', closeModDropdown);
  }
}

async function confirmModReport(modId, reportType) {
  const confirmed = await window.confirm(`Are you sure you want to report this mod for ${reportType}?`);
  if (confirmed) {
    submitModReport(modId, reportType);
  }
}

async function submitModReport(modId, reportType) {
  try {
    const response = await fetch('/api/report-mod', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ modId, reportType }),
    });

    if (response.ok) {
      window.alert('Mod report successfully sent to the team!');
    } else {
      throw new Error('You need to be logged in to report mods.');
    }
  } catch (error) {
    window.alert('You need to be logged in to report mods.');
  }
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
      window.alert(data.message);
      event.target.reset();
    } else {
      const data = await response.json();
      window.alert(data.error || 'Failed to submit bug report');
    }
  } catch (error) {
    window.alert('An error occurred while submitting the bug report');
  }
}

function initializeReportHandlers() {
  const bugReportForm = document.getElementById('bug-report-form');
  if (bugReportForm) {
    bugReportForm.addEventListener('submit', submitBugReport);
  }

  document.addEventListener('click', function(event) {
    if (event.target.classList.contains('btn-report')) {
      const demoCard = event.target.closest('.demo-card');
      const modItem = event.target.closest('.mod-item');
      
      if (demoCard) {
        showReportOptions(demoCard.dataset.demoId, event);
      } else if (modItem) {
        showModReportOptions(modItem.dataset.modId, event);
      }
    }
  });
}

export {
  initializeReportHandlers,
  showReportOptions,
  closeDemoDropdown,
  confirmDemoReport,
  submitDemoReport,
  submitBugReport,
  showModReportOptions,
  closeModDropdown,
  confirmModReport,
  submitModReport
};