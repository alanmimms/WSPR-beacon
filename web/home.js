// Home page logic for displaying status and statistics

let bandConfigs = {};
let liveUpdateInterval = null;
let previousStatus = {};

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
    
    // Start the UTC clock
    startUTCClock();
    
    await loadSettings(); // Load settings first to get band configs
    await loadStatus();
    startLiveUpdates(); // Start real-time status updates
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
    
    // Update statistics
    if (status.statistics) {
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
    if (callsignSpan) callsignSpan.textContent = settings.call || '?';
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
  
  const bands = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m', '6m', '2m'];
  
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
        Schedule (UTC):
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

function startUTCClock() {
  let serverTimeOffset = 0; // Offset between server time and local time in milliseconds
  let lastSyncTime = Date.now();
  
  function updateClock() {
    const localTime = Date.now();
    const adjustedTime = new Date(localTime + serverTimeOffset);
    
    // Format with line break between date and time
    const isoString = adjustedTime.toISOString();
    const datePart = isoString.substring(0, 10); // YYYY-MM-DD
    const timePart = isoString.substring(11, 19) + ' UTC'; // HH:MM:SS UTC
    
    const clockElement = document.getElementById('current-time');
    if (clockElement) {
      clockElement.innerHTML = `<div style="text-align: right;">${datePart}<br>${timePart}</div>`;
    }
  }
  
  async function syncWithServer() {
    try {
      const syncStart = Date.now();
      const response = await fetch('/api/time');
      const syncEnd = Date.now();
      
      if (response.ok) {
        const data = await response.json();
        const networkDelay = (syncEnd - syncStart) / 2; // Estimate one-way delay
        const serverTime = data.unixTime * 1000; // Convert to milliseconds
        const localTime = syncEnd - networkDelay; // Adjust for network delay
        
        serverTimeOffset = serverTime - localTime;
        lastSyncTime = Date.now();
        
        console.log(`Clock synced with server. Offset: ${serverTimeOffset}ms, Synced: ${data.synced}`);
      } else {
        console.warn('Failed to sync time with server');
      }
    } catch (error) {
      console.error('Error syncing time with server:', error);
    }
  }
  
  function checkSync() {
    const timeSinceSync = Date.now() - lastSyncTime;
    // Sync every 7 minutes (7 * 60 * 1000 = 420000 ms)
    if (timeSinceSync >= 420000) {
      syncWithServer();
    }
  }
  
  // Initial sync with server
  syncWithServer();
  
  // Update clock every second
  setInterval(updateClock, 1000);
  
  // Check if sync is needed every 30 seconds
  setInterval(checkSync, 30000);
}

// Live status updates
function startLiveUpdates() {
  // Update every 1 second for fast demo
  liveUpdateInterval = setInterval(updateLiveStatus, 1000);
  console.log('Live status updates started');
}

function stopLiveUpdates() {
  if (liveUpdateInterval) {
    clearInterval(liveUpdateInterval);
    liveUpdateInterval = null;
    console.log('Live status updates stopped');
  }
}

async function updateLiveStatus() {
  try {
    // Try live-status first, fallback to status.json for host-mock
    let response = await fetch('/api/live-status');
    if (!response.ok) {
      response = await fetch('/api/status.json');
    }
    if (!response.ok) throw new Error('Failed to fetch status');
    const status = await response.json();
    
    // Provide defaults for host-mock (which doesn't have live status fields)
    const liveStatus = {
      networkState: status.networkState || 'READY',
      transmissionState: status.transmissionState || 'IDLE',
      isTransmitting: status.isTransmitting || false,
      transmissionInProgress: status.transmissionInProgress || false,
      nextTransmissionIn: status.nextTransmissionIn || 120, // 2 minutes default
      transmissionCount: status.statistics?.totalTransmissions || 0,
      ...status
    };
    
    // Update transmission state and visual indicators  
    updateTransmissionState(liveStatus);
    
    // Update transmission count in statistics section only
    updateElement('total-transmissions', liveStatus.transmissionCount);
    
    // Store current status for next comparison
    previousStatus = liveStatus;
    
  } catch (error) {
    console.error('Error updating live status:', error);
    // No need for fallback values since status box is removed
  }
}

function updateTransmissionState(status) {
  // Transmission state is now handled by the header manager
  // No need for visual indicators since the header shows the state
  // The header manager will update the page title
}

function updateCountdown(secondsRemaining) {
  // Countdown is now handled by the footer via header manager
  // No need for separate countdown element
}

function updateElement(elementId, newValue, highlightChange = true) {
  const element = document.getElementById(elementId);
  if (!element) return;
  
  const currentValue = element.textContent;
  if (currentValue !== newValue.toString()) {
    element.textContent = newValue;
    
    if (highlightChange) {
      // Briefly highlight the changed value
      element.classList.add('changed');
      setTimeout(() => {
        element.classList.remove('changed');
      }, 1000);
    }
  }
}

// Enhanced time sync for accelerated time
function startUTCClock() {
  let serverTimeOffset = 0;
  let lastSyncTime = Date.now();
  let isAccelerated = false;
  let accelerationFactor = 1;
  
  function updateClock() {
    let adjustedTime;
    
    if (isAccelerated) {
      // For accelerated time, fetch from server more frequently
      fetchServerTime().then(serverTime => {
        if (serverTime) {
          adjustedTime = new Date(serverTime * 1000);
          displayTime(adjustedTime);
        }
      });
    } else {
      // Normal time calculation
      const localTime = Date.now();
      adjustedTime = new Date(localTime + serverTimeOffset);
      displayTime(adjustedTime);
    }
  }
  
  function displayTime(time) {
    const isoString = time.toISOString();
    const datePart = isoString.substring(0, 10);
    const timePart = isoString.substring(11, 19) + ' UTC';
    
    const clockElement = document.getElementById('current-time');
    if (clockElement) {
      clockElement.innerHTML = `<div style="text-align: right;">${datePart}<br>${timePart}</div>`;
    }
  }
  
  async function fetchServerTime() {
    try {
      const response = await fetch('/api/time');
      if (response.ok) {
        const data = await response.json();
        isAccelerated = data.accelerated || false;
        accelerationFactor = data.acceleration || 1;
        return data.unixTime;
      }
    } catch (error) {
      console.error('Error fetching server time:', error);
    }
    return null;
  }
  
  async function syncWithServer() {
    try {
      const syncStart = Date.now();
      const response = await fetch('/api/time');
      const syncEnd = Date.now();
      
      if (response.ok) {
        const data = await response.json();
        isAccelerated = data.accelerated || false;
        accelerationFactor = data.acceleration || 1;
        
        if (!isAccelerated) {
          const networkDelay = (syncEnd - syncStart) / 2;
          const serverTime = data.unixTime * 1000;
          const localTime = syncEnd - networkDelay;
          serverTimeOffset = serverTime - localTime;
        }
        
        lastSyncTime = Date.now();
        console.log(`Clock synced. Accelerated: ${isAccelerated}, Factor: ${accelerationFactor}x`);
      }
    } catch (error) {
      console.error('Error syncing time with server:', error);
    }
  }
  
  // Initial sync
  syncWithServer();
  
  // Update clock more frequently for accelerated time
  const clockInterval = isAccelerated ? 100 : 1000;
  setInterval(updateClock, clockInterval);
  
  // Sync less frequently for accelerated time since it changes rapidly
  const syncInterval = isAccelerated ? 10000 : 30000;
  setInterval(syncWithServer, syncInterval);
}