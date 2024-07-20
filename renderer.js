// renderer.js
const si = require('systeminformation');

console.log('Initializing charts...');

// Common chart options
const commonOptions = {
    responsive: true,
    maintainAspectRatio: false,
    animation: {
        duration: 0
    },
    plugins: {
        legend: {
            display: false
        }
    }
};

// Initialize charts
const cpuChart = new Chart(document.getElementById('cpuChart').getContext('2d'), {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'CPU Usage',
            data: [],
            borderColor: 'rgb(75, 192, 192)',
            backgroundColor: 'rgba(75, 192, 192, 0.2)',
            tension: 0.1,
            fill: true
        }]
    },
    options: {
        ...commonOptions,
        scales: {
            y: {
                beginAtZero: true,
                max: 100,
                ticks: {
                    callback: function(value) {
                        return value + '%';
                    }
                }
            }
        }
    }
});

const memChart = new Chart(document.getElementById('memChart').getContext('2d'), {
    type: 'bar',
    data: {
        labels: ['Memory Usage'],
        datasets: [{
            label: 'Used Memory',
            data: [0],
            backgroundColor: 'rgba(255, 99, 132, 0.8)',
        }]
    },
    options: {
        ...commonOptions,
        scales: {
            y: {
                beginAtZero: true,
                ticks: {
                    callback: function(value) {
                        return value + ' MB';
                    }
                }
            }
        }
    }
});

const netChart = new Chart(document.getElementById('netChart').getContext('2d'), {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Network Usage',
            data: [],
            borderColor: 'rgb(153, 102, 255)',
            backgroundColor: 'rgba(153, 102, 255, 0.2)',
            tension: 0.1,
            fill: true
        }]
    },
    options: {
        ...commonOptions,
        scales: {
            y: {
                beginAtZero: true,
                ticks: {
                    callback: function(value) {
                        return value + ' KB/s';
                    }
                }
            }
        }
    }
});

console.log('Charts initialized');

// Update function
async function updateData() {
    console.log('Updating data...');
    const currentTime = new Date().toLocaleTimeString();

    try {
        // CPU
        const cpuLoad = await si.currentLoad();
        console.log('CPU Load:', cpuLoad.currentLoad);
        cpuChart.data.labels.push(currentTime);
        cpuChart.data.datasets[0].data.push(Math.round(cpuLoad.currentLoad));
        if (cpuChart.data.labels.length > 60) {
            cpuChart.data.labels.shift();
            cpuChart.data.datasets[0].data.shift();
        }
        cpuChart.update();

        // Memory (Website usage)
        const memoryUsage = process.memoryUsage();
        const usedMemInMB = Math.round(memoryUsage.heapUsed / 1024 / 1024);
        console.log('Memory Usage:', usedMemInMB, 'MB');
        memChart.data.datasets[0].data = [usedMemInMB];
        memChart.update();

        // Network
        const netStats = await si.networkStats();
        const totalBytes = netStats.reduce((acc, net) => acc + net.rx_sec + net.tx_sec, 0);
        const kbPerSec = Math.round(totalBytes / 1024);
        console.log('Network Usage:', kbPerSec);
        netChart.data.labels.push(currentTime);
        netChart.data.datasets[0].data.push(kbPerSec);
        if (netChart.data.labels.length > 60) {
            netChart.data.labels.shift();
            netChart.data.datasets[0].data.shift();
        }
        netChart.update();

        console.log('Data updated successfully');
    } catch (error) {
        console.error('Error updating data:', error);
    }
}

console.log('Setting up update interval...');
// Update every second
setInterval(updateData, 1000);

console.log('renderer.js loaded successfully');