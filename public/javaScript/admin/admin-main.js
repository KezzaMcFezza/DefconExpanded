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


import Auth from './auth.js';
import UI from './ui.js';
import Utils from './utils.js';
import API from './api.js';


function getCurrentPage() {
    const path = window.location.pathname;

    if (path.includes('adminpanel')) return 'admin-panel';
    if (path.includes('leaderboardblacklistmanage')) return 'blacklist';
    if (path.includes('demomanage')) return 'demos';
    if (path.includes('playerlookup')) return 'player-lookup';
    if (path.includes('dedconmanagment')) return 'dedcon';
    if (path.includes('resourcemanage')) return 'resources';
    if (path.includes('modlistmanage')) return 'mods';
    if (path.includes('accountmanage')) return 'accounts';
    if (path.includes('serverconsole')) return 'console';
    if (path.includes('defconservers')) return 'server-config';


    return 'unknown';
}

const ModuleLoader = {
    loadedModules: {},

    async loadModule(name) {
        if (this.loadedModules[name]) {
            return this.loadedModules[name];
        }

        try {
            const module = await import(`./modules/${name}.js`);
            this.loadedModules[name] = module.default;
            return module.default;
        } catch (error) {
            console.error(`Failed to load module: ${name}`, error);
            return null;
        }
    }
};


async function initializeAdminPanel() {

    document.querySelectorAll('[data-min-role]').forEach(element => {
        const minRole = parseInt(element.dataset.minRole);
        if (Auth.hasPermission(minRole)) {
            element.style.display = 'block';
        } else {
            element.style.display = 'none';
        }
    });

    const currentPage = getCurrentPage();
    const modulesToLoad = [];

    switch (currentPage) {
        case 'admin-panel':
            modulesToLoad.push('reports', 'monitoring');
            break;
        case 'blacklist':
            modulesToLoad.push('whitelist');
            break;
        case 'demos':
            modulesToLoad.push('demos');
            break;
        case 'player-lookup':
            modulesToLoad.push('users');
            break;
        case 'dedcon':
            modulesToLoad.push('dedcon-builds');
            break;
        case 'resources':
            modulesToLoad.push('resources');
            break;
        case 'mods':
            modulesToLoad.push('mods');
            break;
        case 'accounts':
            modulesToLoad.push('users');
            break;
        case 'console':
            modulesToLoad.push('console');
            break;
        case 'server-config':
            modulesToLoad.push('config');
            break;
        default:

            modulesToLoad.push('monitoring');
            break;
    }


    for (const moduleName of modulesToLoad) {
        const module = await ModuleLoader.loadModule(moduleName);
        if (module && typeof module.setupEventListeners === 'function') {
            module.setupEventListeners();
        }
    }


    if (currentPage === 'admin-panel') {
        const ReportsModule = await ModuleLoader.loadModule('reports');
        const MonitoringModule = await ModuleLoader.loadModule('monitoring');

        if (ReportsModule) {
            ReportsModule.loadPendingReports();
            ReportsModule.loadPendingModReports();
            ReportsModule.loadPendingRequests();
            ReportsModule.updatePendingReportsCount();
        }

        if (MonitoringModule) {
            MonitoringModule.loadMonitoringData();
        }
    } else if (currentPage === 'blacklist') {
        const WhitelistModule = await ModuleLoader.loadModule('whitelist');
        if (WhitelistModule) {
            WhitelistModule.loadWhitelist();
        }
    } else if (currentPage === 'demos') {
        const DemosModule = await ModuleLoader.loadModule('demos');
        if (DemosModule) {
            DemosModule.loadDemos();
        }
    } else if (currentPage === 'player-lookup') {
        const UsersModule = await ModuleLoader.loadModule('users');
        if (UsersModule) {
            UsersModule.loadTopPlayers();
        }
    } else if (currentPage === 'dedcon') {
        const DedconBuildsModule = await ModuleLoader.loadModule('dedcon-builds');
        if (DedconBuildsModule) {
            DedconBuildsModule.loadDedconBuilds();
        }
    } else if (currentPage === 'resources') {
        const ResourcesModule = await ModuleLoader.loadModule('resources');
        if (ResourcesModule) {
            ResourcesModule.loadResources();
        }
    } else if (currentPage === 'mods') {
        const ModsModule = await ModuleLoader.loadModule('mods');
        if (ModsModule) {
            ModsModule.loadMods();
        }
    } else if (currentPage === 'accounts') {
        const UsersModule = await ModuleLoader.loadModule('users');
        if (UsersModule) {
            UsersModule.loadUsers();
        }
    } else if (currentPage === 'server-config') {
        const ConfigModule = await ModuleLoader.loadModule('config');
        if (ConfigModule) {
            ConfigModule.loadConfigFiles();
            ConfigModule.populateServerList();
        }
    }
}

window.AdminApp = {};

async function getModuleFunction(moduleName, functionName) {
    try {
        const module = await ModuleLoader.loadModule(moduleName);
        if (module && typeof module[functionName] === 'function') {
            return module[functionName];
        } else {
            console.error(`Function ${functionName} not found in module ${moduleName}`);
            return () => console.error(`Function ${functionName} not found in module ${moduleName}`);
        }
    } catch (error) {
        console.error(`Error getting function ${functionName} from module ${moduleName}:`, error);
        return () => console.error(`Error in ${moduleName}.${functionName}:`, error);
    }
}



AdminApp.addToWhitelist = async (...args) => (await getModuleFunction('whitelist', 'addToWhitelist'))(...args);
AdminApp.removeFromWhitelist = async (...args) => (await getModuleFunction('whitelist', 'removeFromWhitelist'))(...args);
AdminApp.uploadDemo = async (...args) => (await getModuleFunction('demos', 'uploadDemo'))(...args);
AdminApp.editDemo = async (...args) => (await getModuleFunction('demos', 'editDemo'))(...args);
AdminApp.deleteDemo = async (...args) => (await getModuleFunction('demos', 'deleteDemo'))(...args);
AdminApp.saveEditDemo = async (...args) => (await getModuleFunction('demos', 'saveEditDemo'))(...args);
AdminApp.cancelEditDemo = async (...args) => (await getModuleFunction('demos', 'cancelEditDemo'))(...args);
AdminApp.searchDemos = async (...args) => (await getModuleFunction('demos', 'searchDemos'))(...args);
AdminApp.uploadResource = async (...args) => (await getModuleFunction('resources', 'uploadResource'))(...args);
AdminApp.editResource = async (...args) => (await getModuleFunction('resources', 'editResource'))(...args);
AdminApp.deleteResource = async (...args) => (await getModuleFunction('resources', 'deleteResource'))(...args);
AdminApp.saveEditResource = async (...args) => (await getModuleFunction('resources', 'saveEditResource'))(...args);
AdminApp.cancelEditResource = async (...args) => (await getModuleFunction('resources', 'cancelEditResource'))(...args);
AdminApp.editUser = async (...args) => (await getModuleFunction('users', 'editUser'))(...args);
AdminApp.deleteUser = async (...args) => (await getModuleFunction('users', 'deleteUser'))(...args);
AdminApp.saveEditUser = async (...args) => (await getModuleFunction('users', 'saveEditUser'))(...args);
AdminApp.cancelEditUser = async (...args) => (await getModuleFunction('users', 'cancelEditUser'))(...args);
AdminApp.searchPlayers = async (...args) => (await getModuleFunction('users', 'searchPlayers'))(...args);
AdminApp.addMod = async (...args) => (await getModuleFunction('mods', 'addMod'))(...args);
AdminApp.editMod = async (...args) => (await getModuleFunction('mods', 'editMod'))(...args);
AdminApp.deleteMod = async (...args) => (await getModuleFunction('mods', 'deleteMod'))(...args);
AdminApp.saveEditMod = async (...args) => (await getModuleFunction('mods', 'saveEditMod'))(...args);
AdminApp.cancelEditMod = async (...args) => (await getModuleFunction('mods', 'cancelEditMod'))(...args);
AdminApp.resolveReport = async (...args) => (await getModuleFunction('reports', 'resolveReport'))(...args);
AdminApp.resolveModReport = async (...args) => (await getModuleFunction('reports', 'resolveModReport'))(...args);
AdminApp.resolveRequest = async (...args) => (await getModuleFunction('reports', 'resolveRequest'))(...args);
AdminApp.loadFileContent = async (...args) => (await getModuleFunction('config', 'loadFileContent'))(...args);
AdminApp.saveFileContent = async (...args) => (await getModuleFunction('config', 'saveFileContent'))(...args);
AdminApp.cancelEdit = async (...args) => (await getModuleFunction('config', 'cancelEdit'))(...args);
AdminApp.uploadDedconBuild = async (...args) => (await getModuleFunction('dedcon-builds', 'uploadDedconBuild'))(...args);
AdminApp.editDedconBuild = async (...args) => (await getModuleFunction('dedcon-builds', 'editDedconBuild'))(...args);
AdminApp.deleteDedconBuild = async (...args) => (await getModuleFunction('dedcon-builds', 'deleteDedconBuild'))(...args);
AdminApp.saveEditDedconBuild = async (...args) => (await getModuleFunction('dedcon-builds', 'saveEditDedconBuild'))(...args);
AdminApp.cancelEditDedcon = async (...args) => (await getModuleFunction('dedcon-builds', 'cancelEditDedcon'))(...args);
AdminApp.downloadLogs = async (...args) => (await getModuleFunction('console', 'downloadLogs'))(...args);
AdminApp.scrollToTop = (...args) => UI.scrollToTop(...args);
AdminApp.toggleSpectators = (...args) => UI.toggleSpectators(...args);


document.addEventListener('DOMContentLoaded', function () {
    Auth.checkUserRoleAndInitialize(initializeAdminPanel);
    UI.setupMobileMenu();
    UI.setActiveNavItem();
});

export default { initializeAdminPanel };