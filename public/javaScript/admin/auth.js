//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 01-04-2025


import UI from './ui.js';

const Auth = (() => {
    let currentUserRole = 6; 
    
    async function checkUserRoleAndInitialize(callback) {
        try {
            const response = await fetch('/api/current-user');
            const data = await response.json();
            if (data.user) {
                currentUserRole = data.user.role;
                if (currentUserRole <= 5) {
                    UI.setupCustomDialog();
                    if (callback) callback();
                } else {
                    window.location.href = '/login';
                }
            } else {
                window.location.href = '/login';
            }
        } catch (error) {
            console.error('Error checking user role:', error);
            window.location.href = '/login';
        }
    }
    
    function hasPermission(requiredRole) {
        return currentUserRole <= requiredRole;
    }
    
    function requirePermission(requiredRole, actionDescription) {
        if (!hasPermission(requiredRole)) {
            UI.showAlert(`You don't have permission to ${actionDescription || 'do this action'}`);
            return false;
        }
        return true;
    }
    
    function getCurrentRole() {
        return currentUserRole;
    }
    
    function getUserRoleName(roleNumber) {
        const roles = {
            1: 'Super Admin',
            2: 'Admin',
            3: 'Super Mod',
            4: 'Moderator',
            5: 'Helper',
            6: 'User'
        };
        return roles[roleNumber] || 'Unknown';
    }
    
    return {
        checkUserRoleAndInitialize,
        hasPermission,
        requirePermission,
        getCurrentRole,
        getUserRoleName
    };
})();

export default Auth;