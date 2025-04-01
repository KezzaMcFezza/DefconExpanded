// Discord integration functionality
let discordWidget = null;

// Get the next Sunday date for the community game event
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

// Format date for displaying in the event widget
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

// Get Discord event information
function getDiscordEvent() {
  const nextSunday = getNextSunday();
  return {
    title: "Community Game Night",
    date: `Sunday, ${formatDateForEvent(nextSunday)}`,
    time: "3PM EST"
  };
}

// Update Discord widget with current information
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
                    <a href="https://discord.gg/rexZfJ5dMn" target="_blank" rel="noopener noreferrer" class="discord-join-button">Join Discord</a>
                </div>
            </div>
        </div>
      `;

      widgetContainer.innerHTML = html;
    });
}

// Initialize Discord widget and set refresh interval
function initializeDiscordWidget() {
  updateDiscordWidget();
  setInterval(updateDiscordWidget, 5 * 60 * 1000); // Refresh every 5 minutes
}

export {
  initializeDiscordWidget,
  updateDiscordWidget,
  getDiscordEvent,
  formatDateForEvent,
  getNextSunday
};