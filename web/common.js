// common.js: Injects header, nav, and footer, and fills dynamic values from /api/status.json

// Converts dbm to Milliwatts
function dbmToMW(dbm) {
  return Math.pow(10, dbm / 10);
}

// Converts Milliwatts to dbm
function wattsToDbm(mw) {
  return 10 * Math.log10((mw || 1));
}

function updateFooter(data) {
  if (data.callsign) document.getElementById('footer-callsign').textContent = `Callsign: ${data.callsign}`;
  if (data.hostname) document.getElementById('footer-hostname').textContent = `Hostname: ${data.hostname}`;
}

function injectLayout() {
  console.log("Injecting layout");
  const header = document.createElement('header');
  header.innerHTML = `
    <div style="background: #003366; color: white; padding: 1em; font-size: 1.5em;">
      WSPR Beacon
    </div>
  `;
  document.body.prepend(header);

  const nav = document.createElement('nav');
  nav.innerHTML = `
    <div style="width: 200px; background: #f0f0f0; height: 100%; position: fixed; top: 60px; bottom: 30px; padding: 1em;">
      <a href="index.html" id="nav-index" style="display: block; margin-bottom: 0.5em;">Home</a>
      <a href="settings.html" id="nav-settings" style="display: block; margin-bottom: 0.5em;">Settings</a>
      <a href="security.html" id="nav-security" style="display: block; margin-bottom: 0.5em;">Security</a>
    </div>
  `;
  document.body.appendChild(nav);

  const main = document.querySelector('main');
  if (main) {
    main.style.marginLeft = '220px';
    main.style.marginTop = '60px';
    main.style.marginBottom = '30px';
    main.style.padding = '1em';
  }

  const footer = document.createElement('footer');
  footer.innerHTML = `
    <div style="display: flex; justify-content: space-between; background: #e0e0e0; padding: 0.5em 1em; font-size: 0.9em; position: fixed; bottom: 0; left: 0; right: 0;">
      <div id="footer-callsign">Callsign: ...</div>
      <div id="footer-hostname">Hostname: ...</div>
    </div>
  `;
  document.body.appendChild(footer);

  fetch('/api/status.json')
    .then(res => res.json())
    .then(data => updateFooter(data));

  // Highlight active nav
  const currentPage = location.pathname.split('/').pop().split('.')[0];
  const activeLink = document.getElementById(`nav-${currentPage}`);
  if (activeLink) activeLink.style.fontWeight = 'bold';
}

document.addEventListener('DOMContentLoaded', injectLayout);
