// Persistent Header and Footer Management
class HeaderManager {
  constructor() {
    this.elements = {
      callsign: document.getElementById('header-callsign'),
      locator: document.getElementById('header-locator'),
      power: document.getElementById('header-power'),
      state: document.getElementById('header-state'),
      frequency: document.getElementById('header-frequency'),
      time: document.getElementById('header-time'),
      ntpSync: document.getElementById('header-ntp-sync')
    };
    
    this.footerElements = {
      hostname: document.getElementById('footer-hostname'),
      uptime: document.getElementById('footer-uptime'),
      wifi: document.getElementById('footer-wifi'),
      rssi: document.getElementById('footer-rssi'),
      nextTx: document.getElementById('footer-next-tx'),
      txCount: document.getElementById('footer-tx-count')
    };
    
    this.currentState = 'idle';
    this.lastStatus = null;
    this.lastNtpSync = null;
  }

  // Map FSM states to UI requirements from README
  mapBeaconState(networkState, transmissionState) {
    // README requirements: active, idle, error, config
    // Plus additional states: booting, connecting, pending
    
    switch (networkState) {
      case 'BOOTING':
        return 'booting';
      case 'AP_MODE':
        return 'config';
      case 'STA_CONNECTING':
        return 'connecting';
      case 'ERROR':
        return 'error';
      case 'READY':
        switch (transmissionState) {
          case 'TRANSMITTING':
            return 'transmitting';
          case 'TX_PENDING':
            return 'pending';
          case 'IDLE':
          default:
            return 'idle';
        }
      default:
        return 'idle';
    }
  }

  // Get frequency display for active transmissions
  getFrequencyDisplay(band, frequency) {
    if (!band || !frequency) return '';
    return ` @ ${frequency}`;
  }

  // Get frequency for band (fallback for host-mock)
  getFrequencyForBand(band) {
    const frequencies = {
      '160m': '1.836600 MHz',
      '80m': '3.568600 MHz',
      '60m': '5.287200 MHz',
      '40m': '7.038600 MHz',
      '30m': '10.138700 MHz',
      '20m': '14.095600 MHz',
      '17m': '18.104600 MHz',
      '15m': '21.094600 MHz',
      '12m': '24.924600 MHz',
      '10m': '28.124600 MHz',
      '6m': '50.293000 MHz',
      '2m': '144.488500 MHz'
    };
    return frequencies[band] || 'Unknown';
  }

  // Convert dBm to watts for display
  convertDbmToWatts(dbm) {
    const watts = Math.pow(10, (dbm - 30) / 10);
    if (watts < 0.001) {
      return `${(watts * 1000000).toFixed(1)}µW`;
    } else if (watts < 1) {
      return `${(watts * 1000).toFixed(1)}mW`;
    } else {
      return `${watts.toFixed(2)}W`;
    }
  }

  // Format uptime from seconds
  formatUptime(uptimeSeconds) {
    if (!uptimeSeconds || uptimeSeconds < 0) return '0d 0h 0m';
    
    const days = Math.floor(uptimeSeconds / (60 * 60 * 24));
    const hours = Math.floor((uptimeSeconds % (60 * 60 * 24)) / (60 * 60));
    const minutes = Math.floor((uptimeSeconds % (60 * 60)) / 60);
    
    return `${days}d ${hours}h ${minutes}m`;
  }

  // Calculate uptime from reset time (fallback)
  calculateUptime(lastResetTime) {
    if (!lastResetTime) return '0d 0h 0m';
    
    const resetTime = new Date(lastResetTime);
    const now = new Date();
    const uptimeMs = now.getTime() - resetTime.getTime();
    
    const days = Math.floor(uptimeMs / (1000 * 60 * 60 * 24));
    const hours = Math.floor((uptimeMs % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
    const minutes = Math.floor((uptimeMs % (1000 * 60 * 60)) / (1000 * 60));
    
    return `${days}d ${hours}h ${minutes}m`;
  }

  // Format WiFi connection status
  formatWifiStatus(networkState, ssid, wifiMode, clientCount) {
    if (wifiMode === 'ap' || networkState === 'AP_MODE') {
      const clients = clientCount !== undefined ? clientCount : 0;
      return `AP: ${clients} client${clients === 1 ? '' : 's'}`;
    } else if (networkState === 'READY' && ssid) {
      return `Connected: ${ssid}`;
    } else if (networkState === 'STA_CONNECTING') {
      return 'Connecting...';
    } else {
      return 'Disconnected';
    }
  }

  // Format RSSI signal strength with quality indicator
  formatRssi(rssi, wifiMode) {
    // In AP mode, RSSI doesn't apply - show empty
    if (wifiMode === 'ap') {
      return { text: '-', cssClass: '' };
    }
    
    if (rssi === undefined || rssi === null || rssi === 0) {
      return { text: '-', cssClass: '' };
    }
    
    // Determine signal quality and color class based on WiFi standards
    let quality, cssClass;
    if (rssi > -50) {
      quality = 'Excellent';
      cssClass = 'rssi-excellent';
    } else if (rssi > -60) {
      quality = 'Good';
      cssClass = 'rssi-good';
    } else if (rssi > -70) {
      quality = 'Fair';
      cssClass = 'rssi-fair';
    } else {
      quality = 'Poor';
      cssClass = 'rssi-poor';
    }
    
    return {
      text: `${rssi}dBm (${quality})`,
      cssClass: cssClass
    };
  }

  // Update the persistent header
  updateHeader(status) {
    // Store latest status for uptime updates
    this.lastStatus = status;
    
    // Update callsign
    if (status.call && this.elements.callsign) {
      this.elements.callsign.textContent = status.call;
    }

    // Update locator (grid square)
    if (status.loc && this.elements.locator) {
      this.elements.locator.textContent = status.loc;
    }

    // Update power (convert dBm to watts)
    if (status.pwr !== undefined && this.elements.power) {
      const watts = this.convertDbmToWatts(status.pwr);
      this.elements.power.textContent = `${status.pwr}dBm (${watts})`;
    }

    // Map and update state - provide defaults for host-mock
    const networkState = status.netState || 'READY';
    const transmissionState = status.txState || 'IDLE';
    const mappedState = this.mapBeaconState(networkState, transmissionState);
    
    if (this.elements.state) {
      // Remove old state classes
      this.elements.state.className = 'header-state';
      
      // Add new state class and update text
      this.elements.state.classList.add(mappedState);
      this.elements.state.textContent = mappedState;
      
      this.currentState = mappedState;
    }

    // Update frequency display (only show when transmitting)
    if (this.elements.frequency) {
      if (mappedState === 'transmitting' && status.curBand) {
        // For host-mock, derive frequency from band if not provided
        const frequency = status.freq || this.getFrequencyForBand(status.curBand);
        this.elements.frequency.textContent = this.getFrequencyDisplay(status.curBand, frequency);
        this.elements.frequency.style.display = 'inline';
      } else {
        this.elements.frequency.textContent = '';
        this.elements.frequency.style.display = 'none';
      }
    }

    // Update page title based on state
    this.updatePageTitle(mappedState);
    
    // Update footer
    this.updateFooter(status);
  }

  // Update the persistent footer
  updateFooter(status) {
    // Update hostname/node name
    if (status.host && this.footerElements.hostname) {
      this.footerElements.hostname.textContent = status.host;
    }

    // Update uptime
    if (this.footerElements.uptime) {
      if (status.uptime !== undefined) {
        // Use uptime seconds directly from API
        this.footerElements.uptime.textContent = this.formatUptime(status.uptime);
      } else {
        // Fallback to resetTime calculation if available
        this.footerElements.uptime.textContent = this.calculateUptime(status.resetTime);
      }
    }

    // Update WiFi status
    if (this.footerElements.wifi) {
      const networkState = status.netState || 'READY';
      const ssid = status.ssid || 'Unknown';
      const wifiMode = status.wifiMode || 'sta';
      const clientCount = status.clientCount;
      this.footerElements.wifi.textContent = this.formatWifiStatus(networkState, ssid, wifiMode, clientCount);
    }

    // Update RSSI
    if (this.footerElements.rssi) {
      const wifiMode = status.wifiMode || 'sta';
      
      if (wifiMode === 'ap' || status.netState === 'AP_MODE') {
        // In AP mode, show client count instead of RSSI
        const clientCount = status.clientCount !== undefined ? status.clientCount : 0;
        this.footerElements.rssi.textContent = `${clientCount} client${clientCount === 1 ? '' : 's'}`;
        this.footerElements.rssi.className = '';
      } else {
        // In STA mode, show RSSI if available
        const rssi = status.rssi !== undefined ? status.rssi : null;
        const rssiInfo = this.formatRssi(rssi, wifiMode);
        this.footerElements.rssi.textContent = rssiInfo.text;
        
        // Remove old RSSI classes and apply new one
        this.footerElements.rssi.className = '';
        if (rssiInfo.cssClass) {
          this.footerElements.rssi.classList.add(rssiInfo.cssClass);
        }
      }
    }

    // Update next transmission info
    if (this.footerElements.nextTx) {
      const band = status.curBand || '20m';
      const frequency = status.freq || this.getFrequencyForBand(band);
      const freqMHz = typeof frequency === 'number' ? (frequency / 1000000).toFixed(6) + ' MHz' : frequency;
      
      // Check if currently transmitting
      if (status.txState === 'TRANSMITTING') {
        this.footerElements.nextTx.textContent = `TRANSMITTING @ ${freqMHz}`;
      } else if (status.txState === 'TX_PENDING') {
        this.footerElements.nextTx.textContent = `TX PENDING @ ${freqMHz}`;
      } else {
        // Use the nextTx value provided by the scheduler/beacon
        const nextTransmissionIn = status.nextTx || 0;
        
        // Use predicted next transmission frequency if available, otherwise fall back to current
        let nextFreqMHz = freqMHz; // Default to current frequency
        if (status.nextTxFreq) {
          nextFreqMHz = (status.nextTxFreq / 1000000).toFixed(6) + ' MHz';
        }
        
        if (nextTransmissionIn <= 0) {
          this.footerElements.nextTx.textContent = `Ready @ ${nextFreqMHz}`;
        } else {
          const minutes = Math.floor(nextTransmissionIn / 60);
          const seconds = Math.floor(nextTransmissionIn % 60);
          const timeStr = nextTransmissionIn < 60 ? `${seconds}s` : `${minutes}m ${seconds}s`;
          this.footerElements.nextTx.textContent = `${timeStr} @ ${nextFreqMHz}`;
        }
      }
    }

    // Update transmission count and minutes
    if (this.footerElements.txCount) {
      const txCount = status.stats?.txCnt || 
                     status.txCnt || 0;
      const txMinutes = status.stats?.txMin || 
                       status.txMin || 0;
      this.footerElements.txCount.textContent = `${txCount} ${txMinutes}mins`;
    }
  }

  // Update page title to reflect transmission state
  updatePageTitle(state) {
    const currentTitle = document.title;
    const baseTitle = currentTitle.replace(/^\[.*?\]\s*/, ''); // Remove existing prefix
    
    if (state === 'transmitting') {
      document.title = `[TX] ${baseTitle}`;
    } else if (state === 'error') {
      document.title = `[ERR] ${baseTitle}`;
    } else if (state === 'config') {
      document.title = `[CFG] ${baseTitle}`;
    } else {
      document.title = baseTitle;
    }
  }

  // Format time with date and time
  formatDateTime(unixTime) {
    const utcTime = new Date(unixTime * 1000);
    const dateStr = utcTime.toISOString().substr(0, 10); // YYYY-MM-DD
    const timeStr = utcTime.toISOString().substr(11, 8); // HH:MM:SS
    return `${dateStr} ${timeStr} UTC`;
  }

  // Format NTP sync status
  formatNtpSyncStatus(lastSyncTime) {
    if (!lastSyncTime) {
      return 'last NTP sync NEVER';
    }
    
    const now = new Date();
    const syncTime = new Date(lastSyncTime);
    const minutesAgo = Math.floor((now - syncTime) / (1000 * 60));
    
    if (minutesAgo < 1) {
      return 'last NTP sync <1 minute';
    } else if (minutesAgo === 1) {
      return 'last NTP sync 1 minute';
    } else if (minutesAgo < 60) {
      return `last NTP sync ${minutesAgo} minutes`;
    } else if (minutesAgo < 1440) {
      const hoursAgo = Math.floor(minutesAgo / 60);
      return `last NTP sync ${hoursAgo} hour${hoursAgo === 1 ? '' : 's'}`;
    } else {
      const daysAgo = Math.floor(minutesAgo / 1440);
      return `last NTP sync ${daysAgo} day${daysAgo === 1 ? '' : 's'}`;
    }
  }

  // Start periodic time updates
  startTimeSync() {
    // Update time every second
    setInterval(() => {
      // Fetch current time from server for accuracy
      fetch('/api/time')
        .then(response => response.json())
        .then(timeData => {
          if (timeData.unixTime && this.elements.time) {
            this.elements.time.textContent = this.formatDateTime(timeData.unixTime);
          }
          
          // Update NTP sync status if available
          if (timeData.lastSyncTime !== undefined) {
            this.lastNtpSync = timeData.lastSyncTime;
          }
          
          if (this.elements.ntpSync) {
            this.elements.ntpSync.textContent = this.formatNtpSyncStatus(this.lastNtpSync);
          }
        })
        .catch(error => {
          console.warn('Time sync failed:', error);
          // Fallback to local time
          const now = new Date();
          const timeStr = this.formatDateTime(Math.floor(now.getTime() / 1000));
          if (this.elements.time) {
            this.elements.time.textContent = timeStr;
          }
          if (this.elements.ntpSync) {
            this.elements.ntpSync.textContent = 'last NTP sync UNKNOWN';
          }
        });
    }, 1000);
  }

  // Initialize header with current status
  async initialize() {
    try {
      // Load initial status
      const response = await fetch('/api/status.json');
      const status = await response.json();
      this.updateHeader(status);
      
      // Start time synchronization
      this.startTimeSync();
      
      // Set up live status updates
      this.startLiveUpdates();
    } catch (error) {
      console.error('Failed to initialize header:', error);
    }
  }

  // Start live status updates
  startLiveUpdates() {
    // Status updates every 1 second for real-time transmission state
    setInterval(async () => {
      try {
        // Try live-status first, fallback to status.json for host-mock
        let response = await fetch('/api/live-status');
        if (!response.ok) {
          response = await fetch('/api/status.json');
        }
        const liveStatus = await response.json();
        this.updateHeader(liveStatus);
      } catch (error) {
        console.warn('Live status update failed:', error);
      }
    }, 1000);
    
    // Uptime is now updated every second via live status polling
  }
}

// Global header manager instance
let headerManager;

// Initialize header when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
  headerManager = new HeaderManager();
  headerManager.initialize();
});

// Export for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
  module.exports = HeaderManager;
}