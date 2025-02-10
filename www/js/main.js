// API endpoints
const API_ENDPOINTS = {
    system: '/api/v1/system',
    network: '/api/v1/network',
    networkConfig: '/api/v1/config/network',
    mqttConfig: '/api/v1/config/mqtt',
    sensorDHT11: '/api/v1/sensors/dht11'
};

// Status update functions
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

async function updateSensorInfo() {
    try {
        const response = await fetch(API_ENDPOINTS.sensorDHT11);
        const data = await response.json();
        
        if (data.valid) {
            document.getElementById('temperature').textContent = 
                `${data.temperature.toFixed(1)}°C`;
            document.getElementById('humidity').textContent = 
                `${data.humidity.toFixed(1)}%`;
        } else {
            document.getElementById('temperature').textContent = 'N/A';
            document.getElementById('humidity').textContent = 'N/A';
        }
    } catch (error) {
        console.error('Error fetching sensor data:', error);
    }
}

// Configuration loading functions
async function loadNetworkConfig() {
    try {
        const response = await fetch(API_ENDPOINTS.networkConfig);
        const data = await response.json();
        document.getElementById('wifi-ssid').value = data.wifi_ssid || '';
    } catch (error) {
        console.error('Error loading network config:', error);
        showMessage('error', 'Failed to load network configuration');
    }
}

async function loadMqttConfig() {
    try {
        const response = await fetch(API_ENDPOINTS.mqttConfig);
        const data = await response.json();
        document.getElementById('broker-url').value = data.broker_url || '';
    } catch (error) {
        console.error('Error loading MQTT config:', error);
        showMessage('error', 'Failed to load MQTT configuration');
    }
}

// Configuration form submission handlers
async function handleNetworkConfigSubmit(event) {
    event.preventDefault();
    const form = event.target;
    const formData = new FormData(form);
    const data = {
        wifi_ssid: formData.get('wifi_ssid'),
        wifi_password: formData.get('wifi_password')
    };

    try {
        const response = await fetch(API_ENDPOINTS.networkConfig, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        if (response.ok) {
            showMessage('success', 'Network configuration updated successfully');
        } else {
            showMessage('error', 'Failed to update network configuration');
        }
    } catch (error) {
        console.error('Error updating network config:', error);
        showMessage('error', 'Failed to update network configuration');
    }
}

async function handleMqttConfigSubmit(event) {
    event.preventDefault();
    const form = event.target;
    const formData = new FormData(form);
    const data = {
        broker_url: formData.get('broker_url')
    };

    try {
        const response = await fetch(API_ENDPOINTS.mqttConfig, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        if (response.ok) {
            showMessage('success', 'MQTT configuration updated successfully');
        } else {
            showMessage('error', 'Failed to update MQTT configuration');
        }
    } catch (error) {
        console.error('Error updating MQTT config:', error);
        showMessage('error', 'Failed to update MQTT configuration');
    }
}

// Helper function to show status messages
function showMessage(type, message) {
    const messageElement = document.createElement('div');
    messageElement.className = `status-message ${type}`;
    messageElement.textContent = message;
    
    const form = document.activeElement.closest('form');
    if (form) {
        form.appendChild(messageElement);
        messageElement.style.display = 'block';
        
        setTimeout(() => {
            messageElement.remove();
        }, 3000);
    }
}

// Initial setup
document.addEventListener('DOMContentLoaded', function() {
    // Load initial data
    updateSystemInfo();
    updateNetworkInfo();
    updateSensorInfo();
    loadNetworkConfig();
    loadMqttConfig();

    // Set up form submit handlers
    document.getElementById('network-config-form').addEventListener('submit', handleNetworkConfigSubmit);
    document.getElementById('mqtt-config-form').addEventListener('submit', handleMqttConfigSubmit);

    // Set up periodic updates
    setInterval(() => {
        updateSystemInfo();
        updateNetworkInfo();
        updateSensorInfo();
    }, 5000);
});
