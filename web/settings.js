// Settings page logic for auto-updating power mW <-> dBm and loading/saving settings
console.log('SETTINGS.JS LOADED');

// Password visibility toggle functionality
function togglePasswordVisibility(inputId) {
  const input = document.getElementById(inputId);
  const button = input.nextElementSibling;
  
  if (input.type === 'password') {
    input.type = 'text';
    button.textContent = '👁️'; // open eye (password visible)
    button.title = 'Hide password';
  } else {
    input.type = 'password';
    button.textContent = '🙈'; // closed eye (password hidden)
    button.title = 'Show password';
  }
}

// Settings tracking for secondary header
let settingsModified = false;

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
  settingsModified = true;
  
  // Update secondary header
  const statusText = document.getElementById('settings-status-text');
  const statusContainer = document.querySelector('.settings-status');
  const saveBtn = document.getElementById('save-settings-btn');
  
  if (statusText && statusContainer && saveBtn) {
    statusText.textContent = 'Settings modified';
    statusContainer.classList.add('modified');
    saveBtn.disabled = false;
  }
}

// Convenience function for marking settings as changed
function markSettingsChanged() {
  settingsChanged();
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

// Function to trigger WiFi scan manually (for button click simulation only)
function triggerWifiScan() {
  const scanBtn = document.getElementById('scan-networks-btn');
  if (scanBtn && !scanBtn.disabled) {
    console.log('Triggering manual WiFi scan');
    scanBtn.click();
  }
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
      
      // WiFi scan is now manual only - user must click Scan button
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

// Time synchronization functionality
function initializeTimeSyncButton() {
  const syncTimeBtn = document.getElementById('sync-time-btn');
  if (!syncTimeBtn) return;
  
  syncTimeBtn.addEventListener('click', async () => {
    const originalText = syncTimeBtn.textContent;
    syncTimeBtn.disabled = true;
    syncTimeBtn.textContent = 'Syncing...';
    
    try {
      // Get current UTC time from browser
      const browserTime = Math.floor(Date.now() / 1000); // Unix timestamp in seconds
      
      const response = await fetch('/api/time/sync', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          time: browserTime
        })
      });
      
      if (response.ok) {
        syncTimeBtn.textContent = 'Time Synced!';
        setTimeout(() => {
          syncTimeBtn.textContent = originalText;
          syncTimeBtn.disabled = false;
        }, 2000);
      } else {
        throw new Error(`HTTP ${response.status}`);
      }
    } catch (error) {
      console.error('Time sync failed:', error);
      syncTimeBtn.textContent = 'Sync Failed';
      setTimeout(() => {
        syncTimeBtn.textContent = originalText;
        syncTimeBtn.disabled = false;
      }, 2000);
    }
  });
}

// Auto-link power fields
document.addEventListener('DOMContentLoaded', () => {
  console.log('DOMContentLoaded - initializing');
  
  // Initialize collapsible fieldsets
  initializeCollapsibleFieldsets();
  
  // Initialize WiFi mode handling
  initializeWifiModeHandling();
  
  // Initialize time sync button
  initializeTimeSyncButton();
  
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
      document.getElementById('callsign').value = (s.call || '').toUpperCase();
      document.getElementById('locator').value = (s.loc || '').toUpperCase();

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
      
      // Load timezone settings
      if (typeof s.autoTimezone === 'boolean') {
        document.getElementById('auto-timezone').checked = s.autoTimezone;
      }
      if (s.timezone) {
        document.getElementById('timezone-select').value = s.timezone;
      }
      
      // Update timezone UI visibility
      updateTimezoneUI();
      
      // Load band configurations
      if (s.bands) {
        loadBandConfiguration(s.bands);
      }
      
      // Reset settings status after loading (with a small delay to ensure everything is ready)
      setTimeout(() => {
        // Make sure UI shows settings as unchanged after initial load
        settingsModified = false;
        const statusText = document.getElementById('settings-status-text');
        const statusContainer = document.querySelector('.settings-status');
        const saveBtn = document.getElementById('save-settings-btn');
        if (statusText && statusContainer && saveBtn) {
          statusText.textContent = 'Settings unchanged';
          statusContainer.classList.remove('modified');
          saveBtn.disabled = true;
        }
        
        // WiFi scan is now manual only - removed auto-scan on page load
      }, 100);
    } catch (error) {
      console.error('Error loading settings:', error);
    }
  }
  loadSettings();
  
  // Initialize settings change tracking
  attachSettingsChangeListeners();
  
  // Add input validation for callsign, locator, and hostname
  const callsignInput = document.getElementById('callsign');
  const locatorInput = document.getElementById('locator');
  const hostnameInput = document.getElementById('hostname');
  
  // Callsign validation - uppercase letters and numbers only
  if (callsignInput) {
    callsignInput.addEventListener('input', (e) => {
      // Convert to uppercase and remove invalid characters
      e.target.value = e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, '');
    });
  }
  
  // Grid locator validation - uppercase letters and numbers only
  if (locatorInput) {
    locatorInput.addEventListener('input', (e) => {
      // Convert to uppercase and remove invalid characters
      e.target.value = e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, '');
      
      // Optionally enforce 6-character limit for standard grid squares
      if (e.target.value.length > 6) {
        e.target.value = e.target.value.slice(0, 6);
      }
    });
  }
  
  // Hostname validation - alphanumeric and hyphens only
  if (hostnameInput) {
    hostnameInput.addEventListener('input', (e) => {
      // Remove invalid characters (keep letters, numbers, hyphens)
      e.target.value = e.target.value.replace(/[^a-zA-Z0-9-]/g, '');
    });
  }
  
  // Save settings function
  async function saveSettings() {
    console.log('saveSettings() called');
    const statusMessage = document.getElementById('status-message');
    const saveBtn = document.getElementById('save-settings-btn');
    
    saveBtn.disabled = true;
    saveBtn.textContent = 'Saving...';

    let data;
    let wifiMode;
    
    try {
      wifiMode = document.getElementById('wifi-mode').value;
      const powerDbmInput = document.getElementById('power-dbm');
      
      console.log('Collecting band configuration...');
      const bands = collectBandConfiguration();
      console.log('Band configuration collected:', bands);
      
      data = {
        wifiMode: wifiMode,
        host: document.getElementById('hostname').value,
        call: document.getElementById('callsign').value.toUpperCase(),
        loc: document.getElementById('locator').value.toUpperCase(),
        pwr: parseInt(powerDbmInput.value, 10) || 0,
        txPct: parseInt(document.getElementById('tx-percent').value, 10) || 0,
        bandMode: document.getElementById('band-selection-mode').value,
        autoTimezone: document.getElementById('auto-timezone').checked,
        timezone: document.getElementById('timezone-select').value,
        bands: bands
      };
      
      console.log('Data object created:', data);
    } catch (error) {
      console.error('ERROR preparing save data:', error);
      saveBtn.disabled = false;
      saveBtn.textContent = 'Save Settings';
      if (statusMessage) statusMessage.textContent = 'Error preparing settings data';
      return;
    }
    
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
      console.log('Sending POST to /api/settings with data:', JSON.stringify(data, null, 2));
      
      const resp = await fetch('/api/settings', {
        method: 'POST',
        body: JSON.stringify(data),
        headers: { 'Content-Type': 'application/json' }
      });
      
      console.log('Response status:', resp.status);
      
      if (resp.ok) {
        console.log('Save successful!');
        if (statusMessage) statusMessage.textContent = 'Settings saved!';
        setTimeout(() => { if (statusMessage) statusMessage.textContent = ''; }, 3200);
        
        // Reset modified flag and update UI
        settingsModified = false;
        const statusText = document.getElementById('settings-status-text');
        const statusContainer = document.querySelector('.settings-status');
        if (statusText && statusContainer) {
          statusText.textContent = 'Settings unchanged';
          statusContainer.classList.remove('modified');
        }
        
        // Restore button immediately on success
        saveBtn.disabled = false;
        saveBtn.textContent = 'Save Settings';
      } else {
        const errorText = await resp.text();
        console.error('Save failed:', resp.status, errorText);
        if (statusMessage) statusMessage.textContent = 'Failed to save settings.';
        
        // Restore button on error
        saveBtn.disabled = false;
        saveBtn.textContent = 'Save Settings';
      }
    } catch (error) {
      console.error('Network error:', error);
      if (statusMessage) statusMessage.textContent = 'Network error saving settings.';
      
      // Always restore button on error
      saveBtn.disabled = false;
      saveBtn.textContent = 'Save Settings';
    }
  }

  // Handle save button in secondary header
  const saveSettingsBtn = document.getElementById('save-settings-btn');
  if (saveSettingsBtn) {
    console.log('Save button found, adding listener');
    saveSettingsBtn.addEventListener('click', async (event) => {
      console.log('Save button clicked!');
      try {
        await saveSettings();
      } catch (error) {
        console.error('Button click error:', error);
      }
    });
  } else {
    console.error('ERROR: Save button NOT found!');
  }

  // Intercept form submit to use the new save function
  const form = document.getElementById('settings-form');
  if (form) {
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      await saveSettings();
    });
  }

  // Day/Night hour calculation functions
  function getTimezoneOffset() {
    const autoTimezone = document.getElementById('auto-timezone');
    const timezoneSelect = document.getElementById('timezone-select');
    
    if (autoTimezone && autoTimezone.checked) {
      // Auto-detect from locator
      const locator = document.getElementById('locator').value || 'AA00aa';
      return estimateTimezoneFromLocator(locator);
    } else if (timezoneSelect) {
      // Use manual timezone
      const timezoneValue = timezoneSelect.value;
      if (timezoneValue.startsWith('UTC')) {
        const offset = timezoneValue.slice(3);
        return offset === '' ? 0 : parseInt(offset);
      }
    }
    return 0; // Default to UTC
  }
  
  function estimateTimezoneFromLocator(locator) {
    if (locator.length < 4) return 0;
    
    // Extract longitude from maidenhead grid
    const lonField = (locator.charCodeAt(0) - 65) * 20 + (parseInt(locator[2]) * 2) - 180;
    const lonSquare = (locator.charCodeAt(1) - 65) * 10 + parseInt(locator[3]) - 90;
    
    // Rough longitude (center of grid square)
    const longitude = lonField + lonSquare / 10.0 + 1.0;
    
    // Estimate timezone (15 degrees per hour)
    let offset = Math.round(longitude / 15.0);
    
    // Clamp to reasonable range
    if (offset < -12) offset = -12;
    if (offset > 12) offset = 12;
    
    return offset;
  }
  
  function getDayHours() {
    const timezoneOffset = getTimezoneOffset();
    const dayHours = [];
    
    // Day hours are 6 AM to 6 PM in local time
    for (let localHour = 6; localHour < 18; localHour++) {
      let utcHour = localHour - timezoneOffset;
      
      // Handle wrap-around
      if (utcHour < 0) utcHour += 24;
      if (utcHour >= 24) utcHour -= 24;
      
      dayHours.push(utcHour);
    }
    
    return dayHours;
  }
  
  function getNightHours() {
    const timezoneOffset = getTimezoneOffset();
    const nightHours = [];
    
    // Night hours are 6 PM to 6 AM in local time
    for (let localHour = 18; localHour < 30; localHour++) { // 18-29 to handle wrap-around
      let actualLocalHour = localHour >= 24 ? localHour - 24 : localHour;
      let utcHour = actualLocalHour - timezoneOffset;
      
      // Handle wrap-around
      if (utcHour < 0) utcHour += 24;
      if (utcHour >= 24) utcHour -= 24;
      
      nightHours.push(utcHour);
    }
    
    return nightHours;
  }
  
  function updateTimezoneUI() {
    const autoTimezone = document.getElementById('auto-timezone');
    const manualTimezoneRow = document.getElementById('manual-timezone-row');
    const timezoneDisplay = document.getElementById('current-timezone-display');
    
    if (autoTimezone && manualTimezoneRow) {
      manualTimezoneRow.style.display = autoTimezone.checked ? 'none' : 'block';
    }
    
    // Update timezone display
    if (timezoneDisplay) {
      const offset = getTimezoneOffset();
      const locator = document.getElementById('locator').value || 'AA00aa';
      
      if (autoTimezone && autoTimezone.checked) {
        timezoneDisplay.textContent = `UTC${offset >= 0 ? '+' : ''}${offset} (auto-detected from ${locator})`;
      } else {
        const manualValue = document.getElementById('timezone-select').value;
        timezoneDisplay.textContent = `${manualValue} (manual)`;
      }
    }
  }
  
  function updateScheduleCheckboxes(bandName) {
    const scheduleDiv = document.querySelector(`#schedule-${bandName}`);
    if (!scheduleDiv) return;
    
    const allHourBoxes = scheduleDiv.querySelectorAll('.hour-box');
    const selectedHours = Array.from(allHourBoxes).filter(box => box.classList.contains('selected'));
    
    // Update All checkbox
    const selectAllCheckbox = document.querySelector(`.select-all-checkbox[data-band="${bandName}"]`);
    if (selectAllCheckbox) {
      selectAllCheckbox.checked = selectedHours.length === allHourBoxes.length;
    }
    
    // Update Day checkbox
    const dayHoursCheckbox = document.querySelector(`.day-hours-checkbox[data-band="${bandName}"]`);
    if (dayHoursCheckbox) {
      const dayHours = getDayHours();
      const selectedDayHours = dayHours.filter(hour => {
        const hourBox = scheduleDiv.querySelector(`[data-hour="${hour}"]`);
        return hourBox && hourBox.classList.contains('selected');
      });
      dayHoursCheckbox.checked = selectedDayHours.length === dayHours.length;
    }
    
    // Update Night checkbox
    const nightHoursCheckbox = document.querySelector(`.night-hours-checkbox[data-band="${bandName}"]`);
    if (nightHoursCheckbox) {
      const nightHours = getNightHours();
      const selectedNightHours = nightHours.filter(hour => {
        const hourBox = scheduleDiv.querySelector(`[data-hour="${hour}"]`);
        return hourBox && hourBox.classList.contains('selected');
      });
      nightHoursCheckbox.checked = selectedNightHours.length === nightHours.length;
    }
  }

  // Band configuration functions
  function loadBandConfiguration(bands) {
    const container = document.getElementById('band-config');
    if (!container) {
      console.error('ERROR: band-config container not found!');
      return;
    }
    container.innerHTML = '';
    
    const bandOrder = ['160m', '80m', '60m', '40m', '30m', '20m', '17m', '15m', '12m', '10m', '6m', '2m'];
    
    bandOrder.forEach(bandName => {
      const rawBandConfig = bands[bandName] || {
        en: false,
        freq: getDefaultFrequency(bandName),
        sched: 0
      };
      
      // Convert schedule from array format to bitmap format if needed
      let schedule = rawBandConfig.sched;
      if (Array.isArray(schedule)) {
        // Convert array [0,1,2,3...] to bitmap
        let bitmap = 0;
        schedule.forEach(hour => {
          if (hour >= 0 && hour <= 23) {
            bitmap |= (1 << hour);
          }
        });
        schedule = bitmap;
      }
      
      const bandConfig = {
        en: rawBandConfig.en,
        freq: rawBandConfig.freq,
        sched: schedule
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
            <div class="schedule-header">
              <label>Active Hours (UTC):</label>
              <div class="schedule-controls">
                <label class="schedule-toggle" title="Select all 24 hours for ${bandName}">
                  <input type="checkbox" class="schedule-checkbox select-all-checkbox" data-band="${bandName}">
                  <span class="schedule-label">All</span>
                </label>
                <label class="schedule-toggle" title="Toggle day hours (6AM-6PM local time) for ${bandName}">
                  <input type="checkbox" class="schedule-checkbox day-hours-checkbox" data-band="${bandName}">
                  <span class="schedule-label">☀️ Day</span>
                </label>
                <label class="schedule-toggle" title="Toggle night hours (6PM-6AM local time) for ${bandName}">
                  <input type="checkbox" class="schedule-checkbox night-hours-checkbox" data-band="${bandName}">
                  <span class="schedule-label">🌙 Night</span>
                </label>
              </div>
            </div>
            <div class="schedule-hours" id="schedule-${bandName}">
              <div class="hour-group">
                ${Array.from({length: 12}, (_, hour) => {
                  const dayHours = getDayHours();
                  const nightHours = getNightHours();
                  const isDayHour = dayHours.includes(hour);
                  const isNightHour = nightHours.includes(hour);
                  const hourClass = isDayHour ? 'day-hour' : (isNightHour ? 'night-hour' : '');
                  return `
                    <div class="hour-box ${(bandConfig.sched & (1 << hour)) ? 'selected' : ''} ${hourClass}" 
                         data-hour="${hour}" 
                         title="Enable transmission during hour ${hour.toString().padStart(2, '0')}:00-${hour.toString().padStart(2, '0')}:59 UTC">
                      ${hour.toString().padStart(2, '0')}
                    </div>
                  `;
                }).join('')}
              </div>
              <div class="hour-group">
                ${Array.from({length: 12}, (_, i) => {
                  const hour = i + 12;
                  const dayHours = getDayHours();
                  const nightHours = getNightHours();
                  const isDayHour = dayHours.includes(hour);
                  const isNightHour = nightHours.includes(hour);
                  const hourClass = isDayHour ? 'day-hour' : (isNightHour ? 'night-hour' : '');
                  return `
                    <div class="hour-box ${(bandConfig.sched & (1 << hour)) ? 'selected' : ''} ${hourClass}" 
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
          updateScheduleCheckboxes(bandName);
          markSettingsChanged();
        });
      });
      
      // Add Select All checkbox functionality
      const selectAllCheckbox = bandDiv.querySelector('.select-all-checkbox');
      if (selectAllCheckbox) {
        selectAllCheckbox.addEventListener('change', (e) => {
          const scheduleDiv = bandDiv.querySelector(`#schedule-${bandName}`);
          const allHourBoxes = scheduleDiv.querySelectorAll('.hour-box');
          
          if (e.target.checked) {
            allHourBoxes.forEach(box => box.classList.add('selected'));
          } else {
            allHourBoxes.forEach(box => box.classList.remove('selected'));
          }
          
          updateScheduleCheckboxes(bandName);
          markSettingsChanged();
        });
      }
      
      // Add Day Hours checkbox functionality
      const dayHoursCheckbox = bandDiv.querySelector('.day-hours-checkbox');
      if (dayHoursCheckbox) {
        dayHoursCheckbox.addEventListener('change', (e) => {
          const scheduleDiv = bandDiv.querySelector(`#schedule-${bandName}`);
          const dayHours = getDayHours();
          
          dayHours.forEach(hour => {
            const hourBox = scheduleDiv.querySelector(`[data-hour="${hour}"]`);
            if (hourBox) {
              if (e.target.checked) {
                hourBox.classList.add('selected');
              } else {
                hourBox.classList.remove('selected');
              }
            }
          });
          
          updateScheduleCheckboxes(bandName);
          markSettingsChanged();
        });
      }
      
      // Add Night Hours checkbox functionality
      const nightHoursCheckbox = bandDiv.querySelector('.night-hours-checkbox');
      if (nightHoursCheckbox) {
        nightHoursCheckbox.addEventListener('change', (e) => {
          const scheduleDiv = bandDiv.querySelector(`#schedule-${bandName}`);
          const nightHours = getNightHours();
          
          nightHours.forEach(hour => {
            const hourBox = scheduleDiv.querySelector(`[data-hour="${hour}"]`);
            if (hourBox) {
              if (e.target.checked) {
                hourBox.classList.add('selected');
              } else {
                hourBox.classList.remove('selected');
              }
            }
          });
          
          updateScheduleCheckboxes(bandName);
          markSettingsChanged();
        });
      }
      
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
    const bandOrder = ['160m', '80m', '60m', '40m', '30m', '20m', '17m', '15m', '12m', '10m', '6m', '2m'];
    
    let missingCount = 0;
    
    bandOrder.forEach(bandName => {
      try {
        const enabledEl = document.getElementById(`band-${bandName}-enabled`);
        const freqEl = document.getElementById(`band-${bandName}-freq`);
        
        if (!enabledEl || !freqEl) {
          missingCount++;
          // Use defaults for missing bands
          bands[bandName] = {
            en: false,
            freq: getDefaultFrequency(bandName),
            sched: 0
          };
          return;
        }
        
        const enabled = enabledEl.checked;
        const frequency = parseInt(freqEl.value) || getDefaultFrequency(bandName);
        
        const selectedHours = document.querySelectorAll(`#schedule-${bandName} .hour-box.selected`);
        let schedule = 0;
        selectedHours.forEach(box => {
          const hour = parseInt(box.dataset.hour);
          if (!isNaN(hour) && hour >= 0 && hour <= 23) {
            schedule |= (1 << hour);
          }
        });
        
        bands[bandName] = {
          en: enabled,
          freq: frequency,
          sched: schedule
        };
      } catch (error) {
        console.error('Band config error:', bandName, error);
        // Provide safe defaults
        bands[bandName] = {
          en: false,
          freq: getDefaultFrequency(bandName),
          sched: 0
        };
      }
    });
    
    if (missingCount > 0) {
      console.warn('Missing band elements:', missingCount);
    }
    
    return bands;
  }
  
  function getDefaultFrequency(bandName) {
    const defaults = {
      '160m': 1836600,
      '80m': 3568600,
      '60m': 5287200,
      '40m': 7038600,
      '30m': 10138700,
      '20m': 14095600,
      '17m': 18104600,
      '15m': 21094600,
      '12m': 24924600,
      '10m': 28124600,
      '6m': 50293000,
      '2m': 144488500
    };
    return defaults[bandName] || 14095600;
  }
  
  // Add timezone event listeners
  const autoTimezoneCheckbox = document.getElementById('auto-timezone');
  if (autoTimezoneCheckbox) {
    autoTimezoneCheckbox.addEventListener('change', () => {
      updateTimezoneUI();
      markSettingsChanged();
    });
  }
  
  const timezoneSelect = document.getElementById('timezone-select');
  if (timezoneSelect) {
    timezoneSelect.addEventListener('change', () => {
      markSettingsChanged();
    });
  }
  
  // Initialize timezone UI
  updateTimezoneUI();
});
