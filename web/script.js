document.addEventListener('DOMContentLoaded', () => {
  // --- Highlight Active Nav Link ---
  const currentPage = window.location.pathname;
  const navLinks = document.querySelectorAll('nav a');
  navLinks.forEach(link => {
    const linkPath = link.getAttribute('href');
    // Treat "/" as a request for "index.html" for highlighting purposes
    if (linkPath === currentPage || (currentPage === '/' && linkPath === '/index.html')) {
      link.classList.add('active');
    } else {
      link.classList.remove('active');
    }
  });

  // --- Fetch and Populate Footer Values ---
  async function updateFooter() {
    try {
      const response = await fetch('/api/status.json');
      if (!response.ok) throw new Error('Failed to fetch status');
      const data = await response.json();

      const callsignDiv = document.getElementById('footer-callsign');
      if (callsignDiv) {
        callsignDiv.textContent = data.callsign
          ? `Callsign: ${data.callsign}` : 'Callsign: ?';
      }

      const hostnameDiv = document.getElementById('footer-hostname');
      if (hostnameDiv) {
        hostnameDiv.textContent = data.hostname
          ? `Hostname: ${data.hostname}` : 'Hostname: ?';
      }
    } catch (error) {
      const callsignDiv = document.getElementById('footer-callsign');
      const hostnameDiv = document.getElementById('footer-hostname');
      if (callsignDiv) callsignDiv.textContent = 'Callsign: ?';
      if (hostnameDiv) hostnameDiv.textContent = 'Hostname: ?';
    }
  }
  updateFooter();

  // --- Fetch and Populate Home Page (index.html) Status Values ---
  async function updateHomeStatus() {
    try {
      const response = await fetch('/api/status.json');
      if (!response.ok) throw new Error('Failed to fetch status');
      const data = await response.json();

      // Example for status fields on index.html:
      if (document.getElementById('status-callsign')) {
        document.getElementById('status-callsign').textContent = data.callsign || '?';
      }
      if (document.getElementById('status-locator')) {
        document.getElementById('status-locator').textContent = data.locator || '?';
      }
      if (document.getElementById('status-power_dbm')) {
        document.getElementById('status-power_dbm').textContent = (data.power_dbm !== undefined) ? data.power_dbm : '?';
      }
      // Add any other dynamic status fields you display
    } catch (error) {
      if (document.getElementById('status-callsign')) {
        document.getElementById('status-callsign').textContent = '?';
      }
      if (document.getElementById('status-locator')) {
        document.getElementById('status-locator').textContent = '?';
      }
      if (document.getElementById('status-power_dbm')) {
        document.getElementById('status-power_dbm').textContent = '?';
      }
    }
  }

  // --- Provisioning Form Handler ---
  const form = document.getElementById('settings-form');
  if (form) {
    const statusMessage = document.getElementById('status-message');
    const submitBtn = document.getElementById('submit-btn');

    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      submitBtn.disabled = true;
      submitBtn.textContent = 'Saving...';
      const data = new URLSearchParams(new FormData(form));
      try {
        const response = await fetch('/api/settings', {
          method: 'POST',
          body: data,
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
          }
        });
        if (response.ok) {
          if (statusMessage) statusMessage.textContent = 'Settings saved!';
          await updateFooter(); // Immediately update footer after save
        } else {
          if (statusMessage) statusMessage.textContent = 'Failed to save settings.';
        }
      } catch (error) {
        if (statusMessage) statusMessage.textContent = 'Error saving settings.';
      } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = 'Save';
      }
    });
  }

  // --- Refresh footer when switching pages (nav click) ---
  const navLinksArr = Array.from(document.querySelectorAll('nav a'));
  navLinksArr.forEach(link => {
    link.addEventListener('click', () => {
      setTimeout(updateFooter, 250); // Give page time to load, then update footer
    });
  });

  // --- When on Home page, keep status live ---
  if (window.location.pathname.endsWith('index.html') || window.location.pathname === '/') {
    updateHomeStatus();
    // Optionally, refresh status every few seconds:
    // setInterval(updateHomeStatus, 5000);
  }
});