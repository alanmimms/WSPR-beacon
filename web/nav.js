// Shared navigation component
function createNavigation(currentPage = '') {
  const navItems = [
    { id: 'home', href: '/index.html', text: 'Home' },
    { id: 'settings', href: '/settings.html', text: 'Settings' },
    { id: 'calibration', href: '/calibration.html', text: 'Calibration' }
  ];

  return `
    <aside class="nav-panel">
      <nav>
        ${navItems.map(item => 
          `<a href="${item.href}" ${currentPage === item.id ? 'class="active"' : ''}>${item.text}</a>`
        ).join('')}
      </nav>
    </aside>
  `;
}

// Initialize navigation on page load
document.addEventListener('DOMContentLoaded', () => {
  const navContainer = document.getElementById('nav-container');
  if (navContainer) {
    const currentPage = document.body.getAttribute('data-page') || '';
    navContainer.innerHTML = createNavigation(currentPage);
  }
});