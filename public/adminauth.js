// adminCore.js
window.adminState = {
    currentUserRole: 6,
    serverStartTime: null,
    isAuthenticated: false
};

async function initializeAdmin() {
    try {
        const response = await fetch('/api/current-user');
        const data = await response.json();
        if (data.user) {
            window.adminState.currentUserRole = data.user.role;
            window.adminState.isAuthenticated = true;
            if (data.user.role <= 5) {
                setupCustomDialog();
                initializeAdminPanel();
                setupMobileMenu();
                refreshMonitoringData();
                loadMonitoringData();
                setActiveNavItem();
            } else {
                window.location.href = '/login';
            }
        } else {
            window.location.href = '/login';
        }
    } catch (error) {
        window.location.href = '/login';
    }
}

document.addEventListener('DOMContentLoaded', initializeAdmin);