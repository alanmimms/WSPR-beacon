// Settings page logic for auto-updating power mW <-> dBm and loading/saving settings

// Conversion helpers
function mwToDbm(mw) {
  if (!mw || mw <= 0) return 0;
  return Math.round(10 * Math.log10(mw));
}

function dbmToMw(dbm) {
  return Math.round(Math.pow(10, (dbm / 10)));
}

// Auto-link power fields
document.addEventListener('DOMContentLoaded', () => {
  const powerMwInput = document.getElementById('power-mw');
  const powerDbmInput = document.getElementById('power-dbm');

  // Flags to avoid infinite update loop
  let updatingMw = false, updatingDbm = false;

  powerMwInput.addEventListener('input', () => {
    if (updatingMw) return;
    updatingDbm = true;
    const mw = parseFloat(powerMwInput.value);
    if (!isNaN(mw) && mw > 0) {
      powerDbmInput.value = mwToDbm(mw);
    } else {
      powerDbmInput.value = '';
    }
    updatingDbm = false;
  });

  powerDbmInput.addEventListener('input', () => {
    if (updatingDbm) return;
    updatingMw = true;
    const dbm = parseFloat(powerDbmInput.value);
    if (!isNaN(dbm)) {
      powerMwInput.value = dbmToMw(dbm);
    } else {
      powerMwInput.value = '';
    }
    updatingMw = false;
  });

  // Load existing settings via /api/settings
  async function loadSettings() {
    try {
      const r = await fetch('/api/settings');
      if (!r.ok) throw new Error('Failed to load settings');
      const s = await r.json();
      console.log('Loaded settings:', s);
      
      // Load all settings, including empty strings
      document.getElementById('wifi-ssid').value = s.wifiSsid || '';
      document.getElementById('wifi-password').value = s.wifiPassword || '';
      document.getElementById('hostname').value = s.hostname || '';
      document.getElementById('callsign').value = s.callsign || '';
      document.getElementById('locator').value = s.locator || '';

      // Update footer elements (fix CSS selectors)
      const callsignSpan = document.getElementById('callsign-span');
      if (callsignSpan) callsignSpan.textContent = s.callsign || '?';
      
      const hostnameSpan = document.getElementById('hostname-span');
      if (hostnameSpan) hostnameSpan.textContent = s.hostname || '?';

      // Load power settings (prefer powerDbm over powerMw)
      if (typeof s.powerDbm === 'number') {
        powerDbmInput.value = s.powerDbm;
        powerMwInput.value = dbmToMw(s.powerDbm);
      } else if (typeof s.powerMw === 'number') {
        powerMwInput.value = s.powerMw;
        powerDbmInput.value = mwToDbm(s.powerMw);
      }
      
      // Load transmit percentage
      if (typeof s.txPercent === 'number') {
        document.getElementById('tx-percent').value = s.txPercent;
      }
      
      // Load band configurations
      if (s.bands) {
        loadBandConfiguration(s.bands);
      }
    } catch (error) {
      console.error('Error loading settings:', error);
    }
  }
  loadSettings();

  // Intercept form submit to send JSON (not URL-encoded!) to backend
  const form = document.getElementById('settings-form');
  if (form) {
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const statusMessage = document.getElementById('status-message');
      const submitBtn = document.getElementById('submit-btn');
      submitBtn.disabled = true;
      submitBtn.textContent = 'Saving...';

      const data = {
        wifiSsid: document.getElementById('wifi-ssid').value,
        wifiPassword: document.getElementById('wifi-password').value,
        hostname: document.getElementById('hostname').value,
        callsign: document.getElementById('callsign').value,
        locator: document.getElementById('locator').value,
        powerDbm: parseInt(powerDbmInput.value, 10) || 0,
        txPercent: parseInt(document.getElementById('tx-percent').value, 10) || 0,
        bands: collectBandConfiguration()
      };
      // Send as JSON!
      try {
        const resp = await fetch('/api/settings', {
          method: 'POST',
          body: JSON.stringify(data),
          headers: { 'Content-Type': 'application/json' }
        });
        if (resp.ok) {
          if (statusMessage) statusMessage.textContent = 'Settings saved!';
          setTimeout(() => { statusMessage.textContent = ''; }, 3200);
        } else {
          if (statusMessage) statusMessage.textContent = 'Failed to save settings.';
        }
      } catch {
        if (statusMessage) statusMessage.textContent = 'Error saving settings.';
      } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = 'Save Settings';
      }
    });
  }

  // Band configuration functions
  function loadBandConfiguration(bands) {
    const container = document.getElementById('band-config');
    container.innerHTML = '';
    
    const bandOrder = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m'];
    
    bandOrder.forEach(bandName => {
      const bandConfig = bands[bandName] || {
        enabled: false,
        frequency: getDefaultFrequency(bandName),
        schedule: []
      };
      
      const bandDiv = document.createElement('div');
      bandDiv.className = 'band-config-item';
      bandDiv.innerHTML = `
        <div class="band-header">
          <label class="band-enable">
            <input type="checkbox" id="band-${bandName}-enabled" ${bandConfig.enabled ? 'checked' : ''}>
            <strong>${bandName}</strong>
          </label>
          <div class="band-frequency">
            <label for="band-${bandName}-freq">Frequency (Hz):</label>
            <input type="number" id="band-${bandName}-freq" value="${bandConfig.frequency}" min="1000000" max="30000000" step="1">
          </div>
        </div>
        <div class="band-schedule">
          <label>Active Hours (UTC):</label>
          <div class="schedule-hours" id="schedule-${bandName}">
            ${Array.from({length: 24}, (_, hour) => `
              <label class="hour-checkbox">
                <input type="checkbox" value="${hour}" ${bandConfig.schedule.includes(hour) ? 'checked' : ''}>
                ${hour.toString().padStart(2, '0')}
              </label>
            `).join('')}
          </div>
        </div>
      `;
      
      container.appendChild(bandDiv);
    });
  }
  
  function collectBandConfiguration() {
    const bands = {};
    const bandOrder = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m'];
    
    bandOrder.forEach(bandName => {
      const enabled = document.getElementById(`band-${bandName}-enabled`).checked;
      const frequency = parseInt(document.getElementById(`band-${bandName}-freq`).value) || getDefaultFrequency(bandName);
      
      const scheduleCheckboxes = document.querySelectorAll(`#schedule-${bandName} input[type="checkbox"]:checked`);
      const schedule = Array.from(scheduleCheckboxes).map(cb => parseInt(cb.value));
      
      bands[bandName] = {
        enabled,
        frequency,
        schedule
      };
    });
    
    return bands;
  }
  
  function getDefaultFrequency(bandName) {
    const defaults = {
      '160m': 1838100,
      '80m': 3570100,
      '40m': 7040100,
      '30m': 10140200,
      '20m': 14097100,
      '17m': 18106100,
      '15m': 21096100,
      '12m': 24926100,
      '10m': 28126100
    };
    return defaults[bandName] || 14097100;
  }
});
