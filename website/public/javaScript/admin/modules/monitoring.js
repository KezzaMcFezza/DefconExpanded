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


import API from '../api.js';
import Utils from '../utils.js';

const MonitoringModule = (() => {
    let serverStartTime;
    
    async function loadMonitoringData() {
        try {
            const response = await API.get('/api/monitoring-data');
            const uptimeElement = document.getElementById('server-uptime');
            const totalDemosElement = document.getElementById('total-demos');
            const userRequestsElement = document.getElementById('user-requests');

            if (uptimeElement) uptimeElement.textContent = Utils.formatUptime(response.uptime);
            if (totalDemosElement) totalDemosElement.textContent = response.totalDemos;

            if (userRequestsElement) {
                if (response.userRequests === 0) {
                    userRequestsElement.textContent = `0 😊`;
                    userRequestsElement.style.color = 'green';
                    userRequestsElement.classList.remove('sad-face');
                } else {
                    userRequestsElement.textContent = response.userRequests;

                    if (response.userRequests === 1) {
                        userRequestsElement.style.color = 'orange';
                        userRequestsElement.classList.remove('sad-face');
                    } else if (response.userRequests >= 2) {
                        userRequestsElement.style.color = 'red';
                        userRequestsElement.classList.add('sad-face');
                    }
                }
            }

            serverStartTime = new Date(Date.now() - response.uptime * 1000);
            updateUptimeClock();
        } catch (error) {
            console.error('Error loading monitoring data:', error);
            updateMonitoringUIError();
        }
    }

    function updateMonitoringUI(data) {
        const uptimeElement = document.getElementById('server-uptime');
        const totalDemosElement = document.getElementById('total-demos');
        const userRequestsElement = document.getElementById('user-requests');
        const demosProcessedElement = document.getElementById('demos-processed');
        
        if (uptimeElement) uptimeElement.textContent = Utils.formatUptime(data.uptime);
        if (totalDemosElement) totalDemosElement.textContent = data.totalDemos;
        if (userRequestsElement) userRequestsElement.textContent = data.userRequests;
        if (demosProcessedElement) demosProcessedElement.textContent = data.demosProcessedToday;

        serverStartTime = new Date(Date.now() - data.uptime * 1000);
        updateUptimeClock();
    }

    function updateMonitoringUIError() {
        const uptimeElement = document.getElementById('server-uptime');
        const totalDemosElement = document.getElementById('total-demos');
        const userRequestsElement = document.getElementById('user-requests');
        const demosProcessedElement = document.getElementById('demos-processed');
        
        if (uptimeElement) uptimeElement.textContent = 'Error loading data';
        if (totalDemosElement) totalDemosElement.textContent = 'Error';
        if (userRequestsElement) userRequestsElement.textContent = 'Error';
        if (demosProcessedElement) demosProcessedElement.textContent = 'Error';
    }

    function updateUptimeClock() {
        if (serverStartTime) {
            const now = new Date();
            const uptime = Math.floor((now - serverStartTime) / 1000);
            const uptimeElement = document.getElementById('server-uptime');
            if (uptimeElement) uptimeElement.textContent = Utils.formatUptime(uptime);
        }
    }

    function refreshMonitoringData() {
        loadMonitoringData();
    }

    function setupEventListeners() {
        
        setInterval(updateUptimeClock, 1000);
        
        
        setInterval(refreshMonitoringData, 60000);
    }

    return {
        loadMonitoringData,
        refreshMonitoringData,
        updateUptimeClock,
        setupEventListeners
    };
})();

export default MonitoringModule;