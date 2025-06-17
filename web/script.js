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

    // --- Fetch and Populate Dynamic Data ---
    const fetchStatus = async () => {
        try {
            const response = await fetch('/api/status.json');
            if (!response.ok) throw new Error('Failed to fetch status');
            
            const data = await response.json();
            
            const identitySpan = document.getElementById('footer-identity');
            if (identitySpan) {
                identitySpan.textContent = `Callsign: ${data.callsign} | Grid: ${data.locator} | Power: ${data.power_dbm}dBm`;
            }

            const networkSpan = document.getElementById('footer-network');
            if (networkSpan) {
                networkSpan.textContent = `IP: ${data.ip_address} | Hostname: ${data.hostname}`;
            }

        } catch (error) {
            console.error('Error updating status:', error);
            const footer = document.querySelector('footer');
            if(footer) footer.textContent = 'Could not load device status.';
        }
    };
    fetchStatus();

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
            // form submission logic...
        });
    }
});
