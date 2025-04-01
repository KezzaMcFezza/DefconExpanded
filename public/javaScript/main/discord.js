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

let discordWidget = null;

function getNextSunday() {
  const today = new Date();
  const nextSunday = new Date(today);
  const daysUntilSunday = 7 - today.getDay();

  if (today.getDay() === 0) {
    const estHour = today.getUTCHours() - 5;
    if (estHour < 15) {
      return today;
    }
  }

  nextSunday.setDate(today.getDate() + (daysUntilSunday % 7));
  return nextSunday;
}

function formatDateForEvent(date) {
  const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  const day = date.getDate();
  const month = months[date.getMonth()];

  function getOrdinalSuffix(n) {
    const s = ['th', 'st', 'nd', 'rd'];
    const v = n % 100;
    return n + (s[(v - 20) % 10] || s[v] || s[0]);
  }

  return `${month} ${getOrdinalSuffix(day)}`;
}

function getDiscordEvent() {
  const nextSunday = getNextSunday();
  return {
    title: "Community Game Night",
    date: `Sunday, ${formatDateForEvent(nextSunday)}`,
    time: "3PM EST"
  };
}

function updateDiscordWidget() {
  fetch('/api/discord-widget')
    .then(response => response.json())
    .then(data => {
      const widgetContainer = document.getElementById('discord-widget');
      if (!widgetContainer) return;

      const discordEvent = getDiscordEvent();

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
                <div class="event-time-div">
                    <p class="event-time">${discordEvent.time}</p>
                    <a href="https:
                </div>
            </div>
        </div>
      `;

      widgetContainer.innerHTML = html;
    });
}

function initializeDiscordWidget() {
  updateDiscordWidget();
  setInterval(updateDiscordWidget, 5 * 60 * 1000); 
}

export {
  initializeDiscordWidget,
  updateDiscordWidget,
  getDiscordEvent,
  formatDateForEvent,
  getNextSunday
};