// API endpoints
const API_ENDPOINTS = {
    system: '/api/v1/system',
    network: '/api/v1/network'
};

// Update functions
async function updateSystemInfo() {
    try {
        const response = await fetch(API_ENDPOINTS.system);
        const data = await response.json();
        document.getElementById('heap-size').textContent = 
            `${Math.round(data.free_heap / 1024)} KB`;
        document.getElementById('uptime').textContent = 
            `${Math.round(data.uptime_ms / 1000)} seconds`;
    } catch (error) {
        console.error('Error fetching system info:', error);
    }
}

async function updateNetworkInfo() {
    try {
        const response = await fetch(API_ENDPOINTS.network);
        const data = await response.json();
        document.getElementById('wifi-status').textContent = data.status;
        document.getElementById('rssi').textContent = `${data.rssi} dBm`;
    } catch (error) {
        console.error('Error fetching network info:', error);
    }
}

// Update interval
const UPDATE_INTERVAL = 5000; // 5 seconds

// Setup periodic updates
setInterval(() => {
    updateSystemInfo();
    updateNetworkInfo();
}, UPDATE_INTERVAL);

// Initial update
updateSystemInfo();
updateNetworkInfo();
