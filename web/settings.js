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
      if (s.wifiSsid) document.getElementById('wifi-ssid').value = s.wifiSsid;
      if (s.wifiPassword) document.getElementById('wifi-password').value = s.wifiPassword;

      if (s.callsign) {
	document.getElementById('callsign').value = s.callsign;
	document.getElementById('footer span#callsign-span').textContent = s.callsign;
      }

      if (s.hostname) {
	document.getElementById('hostname').value = s.hostname;
	document.getElementById('footer span#hostname-span').textContent = s.hostname;
      }

      if (s.locator) document.getElementById('locator').value = s.locator;
      if (typeof s.powerDbm === 'number') {
        powerDbmInput.value = s.powerDbm;
        powerMwInput.value = dbmToMw(s.powerDbm);
      }
      if (typeof s.powerMw === 'number') {
        powerMwInput.value = s.powerMw;
        powerDbmInput.value = mwToDbm(s.powerMw);
      }
      if (typeof s.txPercent === 'number') {
        document.getElementById('tx-percent').value = s.txPercent;
      }
    } catch {
      // Optionally show error
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
        powerDbm: parseInt(powerDbmInput.value, 10),
        txPercent: parseInt(document.getElementById('tx-percent').value, 10),
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
});
