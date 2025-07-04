// Settings page logic for auto-updating power mW <-> dBm and loading/saving settings

// Settings tracking for secondary header
let originalSettings = {};
let currentSettings = {};

// Conversion helpers
function mwToDbm(mw) {
  if (!mw || mw <= 0) return 0;
  return Math.round(10 * Math.log10(mw));
}

function dbmToMw(dbm) {
  return Math.round(Math.pow(10, (dbm / 10)));
}

// Settings change tracking functions
function captureCurrentSettings() {
  const wifiMode = document.getElementById('wifi-mode')?.value || '';
  const ssidSelect = document.getElementById('wifi-ssid-select');
  const ssidInput = document.getElementById('wifi-ssid-sta');
  
  let ssid = '';
  if (wifiMode === 'sta' && ssidSelect && ssidInput) {
    if (ssidInput.style.display === 'block') {
      ssid = ssidInput.value || '';
    } else {
      const selectedValue = ssidSelect.value;
      if (selectedValue && selectedValue !== '__custom__') {
        ssid = selectedValue;
      } else {
        ssid = ssidInput.value || '';
      }
    }
  }
  
  return {
    wifiMode: wifiMode,
    ssid: ssid,
    wifiPassword: wifiMode === 'sta' ? (document.getElementById('wifi-password-sta')?.value || '') : '',
    ssidAp: wifiMode === 'ap' ? (document.getElementById('wifi-ssid-ap')?.value || '') : '',
    wifiPasswordAp: wifiMode === 'ap' ? (document.getElementById('wifi-password-ap')?.value || '') : '',
    hostname: document.getElementById('hostname')?.value || '',
    callsign: document.getElementById('callsign')?.value || '',
    locator: document.getElementById('locator')?.value || '',
    powerDbm: document.getElementById('power-dbm')?.value || '',
    txPercent: document.getElementById('tx-percent')?.value || '',
    bandSelectionMode: document.getElementById('band-selection-mode')?.value || '',
    bands: (typeof collectBandConfiguration === 'function') ? collectBandConfiguration() : {}
  };
}

function settingsChanged() {
  currentSettings = captureCurrentSettings();
  
  // Compare with original settings
  const changed = JSON.stringify(originalSettings) !== JSON.stringify(currentSettings);
  
  // Update secondary header
  const statusText = document.getElementById('settings-status-text');
  const statusContainer = document.querySelector('.settings-status');
  const saveBtn = document.getElementById('save-settings-btn');
  
  if (statusText && statusContainer && saveBtn) {
    if (changed) {
      statusText.textContent = 'Settings modified';
      statusContainer.classList.add('modified');
      saveBtn.disabled = false;
    } else {
      statusText.textContent = 'Settings unchanged';
      statusContainer.classList.remove('modified');
      saveBtn.disabled = true;
    }
  }
}

function attachSettingsChangeListeners() {
  // Use event delegation for comprehensive coverage of all form elements
  document.addEventListener('input', (event) => {
    if (event.target.closest('#settings-form')) {
      settingsChanged();
    }
  });
  
  document.addEventListener('change', (event) => {
    if (event.target.closest('#settings-form')) {
      settingsChanged();
    }
  });
  
  // Monitor hour box clicks for band configuration
  document.addEventListener('click', (event) => {
    if (event.target.classList.contains('hour-box')) {
      setTimeout(settingsChanged, 10); // Small delay to let the class change take effect
    }
  });
  
  // Monitor power field synchronization (when one updates the other)
  const powerMwInput = document.getElementById('power-mw');
  const powerDbmInput = document.getElementById('power-dbm');
  
  if (powerMwInput) {
    powerMwInput.addEventListener('input', () => {
      setTimeout(settingsChanged, 10); // Small delay for conversion
    });
  }
  
  if (powerDbmInput) {
    powerDbmInput.addEventListener('input', () => {
      setTimeout(settingsChanged, 10); // Small delay for conversion
    });
  }
}

// Collapsible fieldsets functionality
function initializeCollapsibleFieldsets() {
  document.querySelectorAll('.collapsible-fieldset legend').forEach(legend => {
    legend.addEventListener('click', () => {
      const fieldset = legend.closest('.collapsible-fieldset');
      fieldset.classList.toggle('collapsed');
    });
  });
}

// WiFi mode management
function initializeWifiModeHandling() {
  const wifiModeSelect = document.getElementById('wifi-mode');
  const staSettings = document.getElementById('sta-mode-settings');
  const apSettings = document.getElementById('ap-mode-settings');
  const scanBtn = document.getElementById('scan-networks-btn');
  const ssidSelect = document.getElementById('wifi-ssid-select');
  const ssidInput = document.getElementById('wifi-ssid-sta');
  
  function updateWifiModeUI() {
    const mode = wifiModeSelect.value;
    if (mode === 'sta') {
      staSettings.style.display = 'block';
      apSettings.style.display = 'none';
      
      // Ensure SSID combo box is properly initialized
      const ssidSelect = document.getElementById('wifi-ssid-select');
      const ssidInput = document.getElementById('wifi-ssid-sta');
      
      // If no existing value, make sure dropdown is visible
      if (!ssidInput.value) {
        ssidSelect.style.display = 'block';
        ssidInput.style.display = 'none';
        ssidSelect.value = '';
      }
    } else {
      staSettings.style.display = 'none';
      apSettings.style.display = 'block';
    }
  }
  
  // Handle mode changes
  wifiModeSelect.addEventListener('change', () => {
    updateWifiModeUI();
    // Trigger settings change detection
    if (typeof settingsChanged === 'function') {
      settingsChanged();
    }
  });
  
  // Handle scan button
  scanBtn.addEventListener('click', async () => {
    console.log('WiFi scan button clicked');
    scanBtn.disabled = true;
    scanBtn.textContent = 'Scanning...';
    
    try {
      console.log('Fetching /api/wifi/scan...');
      const response = await fetch('/api/wifi/scan');
      console.log('Scan response status:', response.status);
      
      if (response.ok) {
        const networks = await response.json();
        console.log('Received networks:', networks);
        populateNetworkList(networks);
      } else {
        console.warn('WiFi scan failed:', response.status, response.statusText);
        // Add some mock networks for testing
        const fallbackNetworks = [
          { ssid: 'TestNetwork', rssi: -60, encryption: 'WPA2' },
          { ssid: 'MyWiFi', rssi: -70, encryption: 'WPA2' }
        ];
        console.log('Using fallback networks:', fallbackNetworks);
        populateNetworkList(fallbackNetworks);
      }
    } catch (error) {
      console.warn('WiFi scan error:', error);
      // Add fallback networks in case of network error
      const fallbackNetworks = [
        { ssid: 'FallbackNetwork', rssi: -65, encryption: 'WPA2' }
      ];
      populateNetworkList(fallbackNetworks);
    } finally {
      scanBtn.disabled = false;
      scanBtn.textContent = 'Scan';
    }
  });
  
  // Handle SSID selection changes
  ssidSelect.addEventListener('change', () => {
    const selectedValue = ssidSelect.value;
    if (selectedValue === '__custom__') {
      // Show text input for custom SSID
      ssidSelect.style.display = 'none';
      ssidInput.style.display = 'block';
      ssidInput.value = '';  // Clear any previous value
      ssidInput.focus();
    } else {
      // Use selected network SSID
      ssidInput.value = selectedValue;
      // Keep dropdown visible and input hidden
      ssidSelect.style.display = 'block';
      ssidInput.style.display = 'none';
    }
    // Trigger settings change detection
    if (typeof settingsChanged === 'function') {
      settingsChanged();
    }
  });
  
  // Add button to switch back to dropdown from custom input
  const backToDropdownBtn = document.createElement('button');
  backToDropdownBtn.type = 'button';
  backToDropdownBtn.textContent = '↺';
  backToDropdownBtn.className = 'back-to-dropdown-btn';
  backToDropdownBtn.title = 'Switch back to network list';
  backToDropdownBtn.style.display = 'none';
  
  // Insert the button after the text input
  ssidInput.parentNode.insertBefore(backToDropdownBtn, scanBtn);
  
  // Handle click on back button
  backToDropdownBtn.addEventListener('click', () => {
    // Always switch back to dropdown, preserving current input value
    const currentValue = ssidInput.value.trim();
    ssidInput.style.display = 'none';
    backToDropdownBtn.style.display = 'none';
    ssidSelect.style.display = 'block';
    
    // Try to find current value in dropdown options
    let foundMatch = false;
    for (let option of ssidSelect.options) {
      if (option.value === currentValue) {
        ssidSelect.value = currentValue;
        foundMatch = true;
        break;
      }
    }
    
    // If no match found, add current value as a temporary option
    if (currentValue && !foundMatch) {
      const tempOption = document.createElement('option');
      tempOption.value = currentValue;
      tempOption.textContent = `${currentValue} (entered)`;
      ssidSelect.insertBefore(tempOption, ssidSelect.children[1]);
      ssidSelect.value = currentValue;
    } else if (!currentValue) {
      ssidSelect.value = '';  // Reset to default
    }
    
    // Trigger settings change detection
    if (typeof settingsChanged === 'function') {
      settingsChanged();
    }
  });
  
  // Show back button when in custom input mode
  function updateCustomInputUI() {
    if (ssidInput.style.display === 'block') {
      backToDropdownBtn.style.display = 'inline-block';
    } else {
      backToDropdownBtn.style.display = 'none';
    }
  }
  
  // Update UI when switching to custom mode
  const originalChangeHandler = ssidSelect.onchange;
  ssidSelect.addEventListener('change', updateCustomInputUI);
  
  // Initialize mode display
  updateWifiModeUI();
  updateCustomInputUI();
}

function populateNetworkList(networks) {
  const ssidSelect = document.getElementById('wifi-ssid-select');
  
  // Clear existing options (keep the default and custom options)
  while (ssidSelect.children.length > 2) {
    ssidSelect.removeChild(ssidSelect.lastChild);
  }
  
  // Sort networks by signal strength (descending)
  networks.sort((a, b) => (b.rssi || -100) - (a.rssi || -100));
  
  networks.forEach(network => {
    if (network.ssid && network.ssid.trim()) {
      const option = document.createElement('option');
      option.value = network.ssid;
      
      // Add signal strength indicator if available
      if (network.rssi !== undefined) {
        const signalQuality = getSignalQuality(network.rssi);
        const lockIcon = getEncryptionIcon(network.encryption);
        option.textContent = `${network.ssid} (${signalQuality}) ${lockIcon}`;
      } else {
        option.textContent = network.ssid;
      }
      
      ssidSelect.appendChild(option);
    }
  });
  
  console.log(`Populated ${networks.length} networks in dropdown`);
}

function getSignalQuality(rssi) {
  if (rssi > -50) return 'Excellent';
  if (rssi > -60) return 'Good';
  if (rssi > -70) return 'Fair';
  return 'Poor';
}

function getEncryptionIcon(encryption) {
  if (!encryption || encryption === 'Open') {
    return '🔓'; // Open network
  }
  return '🔒'; // Secured network
}

// Auto-link power fields
document.addEventListener('DOMContentLoaded', () => {
  // Initialize collapsible fieldsets
  initializeCollapsibleFieldsets();
  
  // Initialize WiFi mode handling
  initializeWifiModeHandling();
  
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
      
      // Load WiFi mode and settings
      const wifiMode = s.wifiMode || 'sta';
      console.log('Loading WiFi mode:', wifiMode);
      document.getElementById('wifi-mode').value = wifiMode;
      
      // Load appropriate WiFi settings based on mode
      if (wifiMode === 'sta') {
        const ssid = s.ssid || '';
        console.log('Loading STA SSID:', ssid);
        const ssidSelect = document.getElementById('wifi-ssid-select');
        const ssidInput = document.getElementById('wifi-ssid-sta');
        
        // Always set the hidden input value
        ssidInput.value = ssid;
        console.log('SSID elements found:', !!ssidSelect, !!ssidInput);
        
        if (ssid) {
          // Try to find the SSID in the dropdown options
          let foundInDropdown = false;
          for (let option of ssidSelect.options) {
            if (option.value === ssid) {
              ssidSelect.value = ssid;
              foundInDropdown = true;
              break;
            }
          }
          
          if (foundInDropdown) {
            // SSID found in dropdown, show dropdown mode
            ssidSelect.style.display = 'block';
            ssidInput.style.display = 'none';
            console.log('SSID found in dropdown, showing dropdown');
          } else {
            // SSID not in dropdown, add it as a temporary option and select it
            const tempOption = document.createElement('option');
            tempOption.value = ssid;
            tempOption.textContent = `${ssid} (current)`;
            ssidSelect.insertBefore(tempOption, ssidSelect.children[1]); // Insert after first option
            ssidSelect.value = ssid;
            ssidSelect.style.display = 'block';
            ssidInput.style.display = 'none';
            console.log('SSID not in dropdown, added as current option');
          }
        } else {
          // No SSID configured, show dropdown with default prompt
          ssidSelect.style.display = 'block';
          ssidInput.style.display = 'none';
          ssidSelect.value = '';
          console.log('No SSID configured, showing dropdown');
        }
        
        document.getElementById('wifi-password-sta').value = s.pwd || '';
      } else {
        document.getElementById('wifi-ssid-ap').value = s.ssidAp || '';
        document.getElementById('wifi-password-ap').value = s.pwdAp || '';
      }
      
      // Trigger UI update to show correct mode sections
      const modeChangeEvent = new Event('change');
      document.getElementById('wifi-mode').dispatchEvent(modeChangeEvent);
      
      document.getElementById('hostname').value = s.host || '';
      document.getElementById('callsign').value = s.call || '';
      document.getElementById('locator').value = s.loc || '';

      // Update footer elements (fix CSS selectors)
      const callsignSpan = document.getElementById('callsign-span');
      if (callsignSpan) callsignSpan.textContent = s.call || '?';
      
      const hostnameSpan = document.getElementById('hostname-span');
      if (hostnameSpan) hostnameSpan.textContent = s.host || '?';

      // Load power settings (prefer pwr over pwrMw)
      if (typeof s.pwr === 'number') {
        powerDbmInput.value = s.pwr;
        powerMwInput.value = dbmToMw(s.pwr);
      } else if (typeof s.pwrMw === 'number') {
        powerMwInput.value = s.pwrMw;
        powerDbmInput.value = mwToDbm(s.pwrMw);
      }
      
      // Load transmit percentage
      if (typeof s.txPct === 'number') {
        document.getElementById('tx-percent').value = s.txPct;
      }
      
      // Load band selection mode
      if (s.bandMode) {
        document.getElementById('band-selection-mode').value = s.bandMode;
      }
      
      // Load band configurations
      if (s.bands) {
        loadBandConfiguration(s.bands);
      }
      
      // Capture original settings after loading (with a small delay to ensure everything is ready)
      setTimeout(() => {
        originalSettings = captureCurrentSettings();
        settingsChanged(); // Update secondary header state
      }, 100);
    } catch (error) {
      console.error('Error loading settings:', error);
    }
  }
  loadSettings();
  
  // Initialize settings change tracking
  attachSettingsChangeListeners();
  
  // Save settings function
  async function saveSettings() {
    const statusMessage = document.getElementById('status-message');
    const saveBtn = document.getElementById('save-settings-btn');
    
    saveBtn.disabled = true;
    saveBtn.textContent = 'Saving...';

    const wifiMode = document.getElementById('wifi-mode').value;
    const powerDbmInput = document.getElementById('power-dbm');
    const data = {
      wifiMode: wifiMode,
      host: document.getElementById('hostname').value,
      call: document.getElementById('callsign').value,
      loc: document.getElementById('locator').value,
      pwr: parseInt(powerDbmInput.value, 10) || 0,
      txPct: parseInt(document.getElementById('tx-percent').value, 10) || 0,
      bandMode: document.getElementById('band-selection-mode').value,
      bands: collectBandConfiguration()
    };
    
    // Add WiFi settings based on mode
    if (wifiMode === 'sta') {
      const ssidSelect = document.getElementById('wifi-ssid-select');
      const ssidInput = document.getElementById('wifi-ssid-sta');
      
      let ssid = '';
      if (ssidInput.style.display === 'block') {
        ssid = ssidInput.value;
      } else {
        const selectedValue = ssidSelect.value;
        if (selectedValue && selectedValue !== '__custom__') {
          ssid = selectedValue;
        } else {
          ssid = ssidInput.value;
        }
      }
      
      data.ssid = ssid;
      data.pwd = document.getElementById('wifi-password-sta').value;
    } else {
      data.ssidAp = document.getElementById('wifi-ssid-ap').value;
      data.pwdAp = document.getElementById('wifi-password-ap').value;
    }
    
    try {
      const resp = await fetch('/api/settings', {
        method: 'POST',
        body: JSON.stringify(data),
        headers: { 'Content-Type': 'application/json' }
      });
      if (resp.ok) {
        if (statusMessage) statusMessage.textContent = 'Settings saved!';
        setTimeout(() => { if (statusMessage) statusMessage.textContent = ''; }, 3200);
        
        // Update original settings and reset status
        originalSettings = captureCurrentSettings();
        settingsChanged();
      } else {
        if (statusMessage) statusMessage.textContent = 'Failed to save settings.';
      }
    } catch {
      if (statusMessage) statusMessage.textContent = 'Error saving settings.';
    } finally {
      saveBtn.disabled = false;
      saveBtn.textContent = 'Save Settings';
    }
  }

  // Handle save button in secondary header
  const saveSettingsBtn = document.getElementById('save-settings-btn');
  if (saveSettingsBtn) {
    saveSettingsBtn.addEventListener('click', async () => {
      await saveSettings();
    });
  }

  // Intercept form submit to use the new save function
  const form = document.getElementById('settings-form');
  if (form) {
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      await saveSettings();
    });
  }

  // Band configuration functions
  function loadBandConfiguration(bands) {
    const container = document.getElementById('band-config');
    container.innerHTML = '';
    
    const bandOrder = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m'];
    
    bandOrder.forEach(bandName => {
      const bandConfig = bands[bandName] || {
        en: false,
        freq: getDefaultFrequency(bandName),
        sched: []
      };
      
      const bandDiv = document.createElement('div');
      bandDiv.className = 'band-config-item';
      bandDiv.innerHTML = `
        <div class="band-header">
          <label class="band-enable">
            <input type="checkbox" id="band-${bandName}-enabled" ${bandConfig.en ? 'checked' : ''} title="Enable transmissions on the ${bandName} band">
            <strong>${bandName}</strong>
            <button type="button" class="band-collapse-toggle" title="Expand/collapse band configuration">▼</button>
          </label>
          <div class="band-frequency">
            <label for="band-${bandName}-freq">Frequency (Hz):</label>
            <input type="number" id="band-${bandName}-freq" value="${bandConfig.freq}" min="1000000" max="30000000" step="1" title="Transmit frequency in Hz for ${bandName} band (override WSPR default if needed)">
          </div>
        </div>
        <div class="band-details">
          <div class="band-schedule">
            <label>Active Hours (UTC):</label>
            <div class="schedule-hours" id="schedule-${bandName}">
              <div class="hour-group">
                ${Array.from({length: 12}, (_, hour) => `
                  <div class="hour-box ${bandConfig.sched.includes(hour) ? 'selected' : ''}" 
                       data-hour="${hour}" 
                       title="Enable transmission during hour ${hour.toString().padStart(2, '0')}:00-${hour.toString().padStart(2, '0')}:59 UTC">
                    ${hour.toString().padStart(2, '0')}
                  </div>
                `).join('')}
              </div>
              <div class="hour-group">
                ${Array.from({length: 12}, (_, i) => {
                  const hour = i + 12;
                  return `
                    <div class="hour-box ${bandConfig.sched.includes(hour) ? 'selected' : ''}" 
                         data-hour="${hour}" 
                         title="Enable transmission during hour ${hour.toString().padStart(2, '0')}:00-${hour.toString().padStart(2, '0')}:59 UTC">
                      ${hour.toString().padStart(2, '0')}
                    </div>
                  `;
                }).join('')}
              </div>
            </div>
          </div>
        </div>
      `;
      
      container.appendChild(bandDiv);
      
      // Add click handlers for hour boxes
      const hourBoxes = bandDiv.querySelectorAll('.hour-box');
      hourBoxes.forEach(box => {
        box.addEventListener('click', function() {
          this.classList.toggle('selected');
        });
      });
      
      // Add collapse toggle functionality
      const collapseToggle = bandDiv.querySelector('.band-collapse-toggle');
      if (collapseToggle) {
        collapseToggle.addEventListener('click', (e) => {
          e.preventDefault();
          e.stopPropagation();
          bandDiv.classList.toggle('collapsed');
        });
      }
    });
  }
  
  function collectBandConfiguration() {
    const bands = {};
    const bandOrder = ['160m', '80m', '40m', '30m', '20m', '17m', '15m', '12m', '10m'];
    
    bandOrder.forEach(bandName => {
      const enabled = document.getElementById(`band-${bandName}-enabled`).checked;
      const frequency = parseInt(document.getElementById(`band-${bandName}-freq`).value) || getDefaultFrequency(bandName);
      
      const selectedHours = document.querySelectorAll(`#schedule-${bandName} .hour-box.selected`);
      const schedule = Array.from(selectedHours).map(box => parseInt(box.dataset.hour));
      
      bands[bandName] = {
        en: enabled,
        freq: frequency,
        sched: schedule
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
