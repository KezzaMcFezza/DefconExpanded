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
//Last Edited 25-05-2025


import UI from './ui.js';

const Auth = (() => {
    let userPermissions = [];
    let permissionManifest = {};
    let isInitialized = false;
    
    async function init() {
        if (isInitialized) return true;
        
        try {
            const response = await fetch('/api/permissions/manifest');
            const data = await response.json();
            
            if (data.manifest && data.userPermissions) {
                permissionManifest = data.manifest;
                userPermissions = data.userPermissions;
                isInitialized = true;
                return true;
            } else {
                throw new Error('Invalid permission data received');
            }
        } catch (error) {
            console.error('Error initializing permissions:', error);
            return false;
        }
    }
    
    async function checkUserPermissionsAndInitialize(callback) {
        try {
            const response = await fetch('/api/current-user');
            const data = await response.json();
            
            if (data.user && data.user.permissions) {
                userPermissions = data.user.permissions;

                const initSuccess = await init();
                if (!initSuccess) {
                    throw new Error('Failed to initialize permission system');
                }

                const hasAdminAccess = hasPermission('PAGE_ADMIN_PANEL');
                
                if (hasAdminAccess) {
                    UI.setupCustomDialog();
                    if (callback) callback();
                } else {
                    window.location.href = '/login';
                }
            } else {
                window.location.href = '/login';
            }
        } catch (error) {
            console.error('Error checking user permissions:', error);
            window.location.href = '/login';
        }
    }
    
    function hasPermission(permissionName) {
        
        if (!isInitialized) {
            return false;
        }
        
        const permissionId = permissionManifest[permissionName];
        
        if (permissionId === undefined) {
            return false;
        }
        
        const hasAccess = userPermissions.includes(permissionId);
        return hasAccess;
    }
    
    function hasAnyPermission(permissionNames) {
        return permissionNames.some(permissionName => hasPermission(permissionName));
    }
    
    function requirePermission(permissionName, actionDescription) {
        if (!hasPermission(permissionName)) {
            UI.showAlert(`You don't have permission to ${actionDescription || 'do this action'}`);
            return false;
        }
        return true;
    }
    
    function getAllPermissions() {
        return [...userPermissions];
    }
    
    function getPermissionManifest() {
        return { ...permissionManifest };
    }
    
    return {
        init,
        checkUserPermissionsAndInitialize,
        hasPermission,
        hasAnyPermission,
        requirePermission,
        getAllPermissions,
        getPermissionManifest,
    };
})();

export default Auth;