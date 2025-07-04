// Persistent Header and Footer Management
class HeaderManager {
  constructor() {
    this.elements = {
      callsign: document.getElementById('header-callsign'),
      locator: document.getElementById('header-locator'),
      power: document.getElementById('header-power'),
      state: document.getElementById('header-state'),
      frequency: document.getElementById('header-frequency'),
      time: document.getElementById('header-time')
    };
    
    this.footerElements = {
      hostname: document.getElementById('footer-hostname'),
      resetTime: document.getElementById('footer-reset-time'),
      nextTx: document.getElementById('footer-next-tx'),
      txCount: document.getElementById('footer-tx-count')
    };
    
    this.currentState = 'idle';
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
      '160m': '1.838100 MHz',
      '80m': '3.570100 MHz', 
      '40m': '7.040100 MHz',
      '30m': '10.140200 MHz',
      '20m': '14.097100 MHz',
      '17m': '18.106100 MHz',
      '15m': '21.096100 MHz',
      '12m': '24.926100 MHz',
      '10m': '28.126100 MHz'
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

  // Update the persistent header
  updateHeader(status) {
    // Update callsign
    if (status.callsign && this.elements.callsign) {
      this.elements.callsign.textContent = status.callsign;
    }

    // Update locator (grid square)
    if (status.locator && this.elements.locator) {
      this.elements.locator.textContent = status.locator;
    }

    // Update power (convert dBm to watts)
    if (status.powerDbm !== undefined && this.elements.power) {
      const watts = this.convertDbmToWatts(status.powerDbm);
      this.elements.power.textContent = `${status.powerDbm}dBm (${watts})`;
    }

    // Map and update state - provide defaults for host-mock
    const networkState = status.networkState || 'READY';
    const transmissionState = status.transmissionState || 'IDLE';
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
      if (mappedState === 'transmitting' && status.currentBand) {
        // For host-mock, derive frequency from band if not provided
        const frequency = status.frequency || this.getFrequencyForBand(status.currentBand);
        this.elements.frequency.textContent = this.getFrequencyDisplay(status.currentBand, frequency);
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
    if (status.hostname && this.footerElements.hostname) {
      this.footerElements.hostname.textContent = status.hostname;
    }

    // Update last reset time
    if (status.lastResetTime && this.footerElements.resetTime) {
      const resetDate = new Date(status.lastResetTime);
      const dateStr = resetDate.toISOString().substr(0, 10);
      const timeStr = resetDate.toISOString().substr(11, 8);
      this.footerElements.resetTime.textContent = `${dateStr} ${timeStr} UTC`;
    }

    // Update next transmission info
    if (this.footerElements.nextTx) {
      const nextTransmissionIn = status.nextTransmissionIn || 120;
      const band = status.currentBand || '20m';
      const frequency = status.frequency || this.getFrequencyForBand(band);
      
      const minutes = Math.floor(nextTransmissionIn / 60);
      const seconds = nextTransmissionIn % 60;
      const timeStr = nextTransmissionIn <= 0 ? 'Now' : 
                     nextTransmissionIn < 60 ? `${seconds}s` :
                     `${minutes}m ${seconds}s`;
      
      this.footerElements.nextTx.textContent = `${timeStr} @ ${frequency}`;
    }

    // Update transmission count
    if (this.footerElements.txCount) {
      const txCount = status.statistics?.totalTransmissions || 
                     status.transmissionCount || 0;
      this.footerElements.txCount.textContent = txCount.toString();
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
        })
        .catch(error => {
          console.warn('Time sync failed:', error);
          // Fallback to local time
          const now = new Date();
          const timeStr = this.formatDateTime(Math.floor(now.getTime() / 1000));
          if (this.elements.time) {
            this.elements.time.textContent = timeStr;
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
    }, 5000); // Slower updates for status.json
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