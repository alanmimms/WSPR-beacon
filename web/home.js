// Home page logic for displaying status and statistics

let bandConfigs = {};

// Collapsible fieldsets functionality
function initializeCollapsibleFieldsets() {
  document.querySelectorAll('.collapsible-fieldset legend').forEach(legend => {
    legend.addEventListener('click', () => {
      const fieldset = legend.closest('.collapsible-fieldset');
      fieldset.classList.toggle('collapsed');
    });
  });
}

document.addEventListener('DOMContentLoaded', async () => {
  try {
    // Initialize collapsible fieldsets
    initializeCollapsibleFieldsets();
    
    await loadSettings(); // Load settings first to get band configs
    await loadStatus();
  } catch (error) {
    console.error('Error loading home page data:', error);
  }
});

async function loadStatus() {
  try {
    const response = await fetch('/api/status.json');
    if (!response.ok) throw new Error('Failed to load status');
    const status = await response.json();
    
    console.log('Loaded status:', status);
    
    // Update current status
    document.getElementById('current-callsign').textContent = status.callsign || '?';
    document.getElementById('current-locator').textContent = status.locator || '?';
    document.getElementById('current-power').textContent = `${status.powerDbm || '?'}dBm`;
    document.getElementById('current-band').textContent = status.currentBand || '?';
    
    // Format last reset time
    if (status.lastResetTime) {
      const resetDate = new Date(status.lastResetTime);
      document.getElementById('last-reset').textContent = resetDate.toLocaleString();
    }
    
    // Update statistics
    if (status.statistics) {
      document.getElementById('total-transmissions').textContent = status.statistics.totalTransmissions || 0;
      document.getElementById('total-minutes').textContent = status.statistics.totalMinutes || 0;
      
      // Update band statistics
      updateBandStats(status.statistics.byBand || {}, bandConfigs);
    }
    
  } catch (error) {
    console.error('Error loading status:', error);
    // Show error state
    document.querySelectorAll('.status-item span, .stat-value span').forEach(el => {
      el.textContent = 'Error';
    });
  }
}

async function loadSettings() {
  try {
    const response = await fetch('/api/settings');
    if (!response.ok) throw new Error('Failed to load settings');
    const settings = await response.json();
    
    // Update footer
    const callsignSpan = document.getElementById('callsign-span');
    const hostnameSpan = document.getElementById('hostname-span');
    if (callsignSpan) callsignSpan.textContent = settings.callsign || '?';
    if (hostnameSpan) hostnameSpan.textContent = settings.hostname || '?';
    
    // Store band configs for use in updateBandStats
    if (settings.bands) {
      bandConfigs = settings.bands;
      updateScheduleOverview(settings.bands);
    }
    
  } catch (error) {
    console.error('Error loading settings:', error);
  }
}

function updateBandStats(bandStats, bandConfigs) {
  const container = document.getElementById('band-stats');
  container.innerHTML = '';
  
  const bands = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m'];
  
  bands.forEach(band => {
    const stats = bandStats[band] || { transmissions: 0, minutes: 0 };
    const config = bandConfigs && bandConfigs[band] ? bandConfigs[band] : { enabled: false, schedule: [] };
    
    const bandDiv = document.createElement('div');
    bandDiv.className = 'band-stat';
    
    // Create hour indicators in two rows of 12
    const firstRow = Array.from({length: 12}, (_, hour) => {
      const isActive = config.schedule && config.schedule.includes(hour);
      const hourStr = hour.toString().padStart(2, '0');
      return `<div class="hour-indicator ${isActive ? 'active' : ''}">${hourStr}</div>`;
    }).join('');
    
    const secondRow = Array.from({length: 12}, (_, hour) => {
      const actualHour = hour + 12;
      const isActive = config.schedule && config.schedule.includes(actualHour);
      const hourStr = actualHour.toString().padStart(2, '0');
      return `<div class="hour-indicator ${isActive ? 'active' : ''}">${hourStr}</div>`;
    }).join('');
    
    bandDiv.innerHTML = `
      <div class="band-header">
        <div class="band-name">${band}</div>
        <div class="band-numbers">
          <div>${stats.transmissions || 0} Tx</div>
          <div>${stats.minutes || 0} mins</div>
        </div>
      </div>
      <div class="band-hours">
        Schedule:
        <div class="hour-indicators">
          <div class="hour-row">${firstRow}</div>
          <div class="hour-row">${secondRow}</div>
        </div>
      </div>
    `;
    
    container.appendChild(bandDiv);
  });
}

function updateScheduleOverview(bands) {
  const container = document.getElementById('schedule-overview');
  container.innerHTML = '';
  
  // Create 24-hour timeline
  const timeline = document.createElement('div');
  timeline.className = 'schedule-timeline';
  
  // Hour labels
  const hourLabels = document.createElement('div');
  hourLabels.className = 'hour-labels';
  for (let hour = 0; hour < 24; hour++) {
    const label = document.createElement('div');
    label.className = 'hour-label';
    label.textContent = hour.toString().padStart(2, '0');
    hourLabels.appendChild(label);
  }
  timeline.appendChild(hourLabels);
  
  // Band schedules
  Object.entries(bands).forEach(([bandName, bandConfig]) => {
    if (!bandConfig.enabled) return;
    
    const bandRow = document.createElement('div');
    bandRow.className = 'schedule-band-row';
    
    const bandLabel = document.createElement('div');
    bandLabel.className = 'schedule-band-label';
    bandLabel.textContent = bandName;
    bandRow.appendChild(bandLabel);
    
    const scheduleBar = document.createElement('div');
    scheduleBar.className = 'schedule-bar';
    
    for (let hour = 0; hour < 24; hour++) {
      const hourSlot = document.createElement('div');
      hourSlot.className = 'schedule-hour';
      
      if (bandConfig.schedule && bandConfig.schedule.includes(hour)) {
        hourSlot.classList.add('active');
      }
      
      scheduleBar.appendChild(hourSlot);
    }
    
    bandRow.appendChild(scheduleBar);
    timeline.appendChild(bandRow);
  });
  
  container.appendChild(timeline);
}