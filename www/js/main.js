// API endpoints
const API_ENDPOINTS = {
    system: '/api/v1/system',
    network: '/api/v1/network',
    networkConfig: '/api/v1/config/network',
    mqttConfig: '/api/v1/config/mqtt',
    sensorDHT11: '/api/v1/sensors/dht11',
    networkMode: '/api/v1/network/mode'
};

// Global state
let currentNetworkMode = 'unknown';
let isProvisioned = false;

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
        
        // Update global state
        currentNetworkMode = data.mode.toLowerCase();
        isProvisioned = data.is_provisioned;
        
        // Update mode status
        updateModeStatus(data);
        
        // Update network details based on mode
        updateNetworkDetails(data);
        
        // Update configuration info
        updateConfigurationInfo(data);
        
        // Update mode controls
        updateModeControls(data);
        
    } catch (error) {
        console.error('Error fetching network info:', error);
        updateModeStatus({ mode: 'Unknown', is_provisioned: false });
    }
}

function updateModeStatus(data) {
    const modeBadge = document.getElementById('mode-badge');
    const modeDetails = document.getElementById('mode-details');
    const connectionStatus = document.getElementById('connection-status');
    
    // Update mode badge
    modeBadge.textContent = data.mode;
    modeBadge.className = 'mode-badge ' + getModeClass(data.mode);
    
    // Update mode details
    const provisionedText = data.is_provisioned ? 'WiFi credentials configured' : 'No WiFi credentials';
    modeDetails.textContent = provisionedText;
    
    // Update connection status
    updateConnectionStatus(data, connectionStatus);
}

function getModeClass(mode) {
    switch (mode.toLowerCase()) {
        case 'station': return 'station';
        case 'access point': return 'ap';
        case 'switching': return 'switching';
        default: return 'unknown';
    }
}

function updateConnectionStatus(data, statusElement) {
    if (data.mode === 'Station') {
        if (data.sta_status === 'Connected') {
            statusElement.textContent = `Connected to ${data.sta_ssid || 'WiFi network'}`;
            statusElement.className = 'connection-status connected';
        } else {
            statusElement.textContent = 'Disconnected from WiFi network';
            statusElement.className = 'connection-status disconnected';
        }
    } else if (data.mode === 'Access Point') {
        statusElement.textContent = `Access Point "${data.ap_ssid}" is active`;
        statusElement.className = 'connection-status ap-active';
    } else if (data.mode === 'Switching') {
        statusElement.textContent = 'Switching network modes...';
        statusElement.className = 'connection-status';
    } else {
        statusElement.textContent = 'Network status unknown';
        statusElement.className = 'connection-status';
    }
}

function updateNetworkDetails(data) {
    const ipElement = document.getElementById('ip-address');
    const wifiSsidElement = document.getElementById('wifi-ssid');
    const rssiElement = document.getElementById('rssi');
    const staInfo = document.getElementById('sta-info');
    const rssiInfo = document.getElementById('rssi-info');
    
    if (data.mode === 'Station') {
        ipElement.textContent = data.sta_ip_address || 'Not assigned';
        wifiSsidElement.textContent = data.sta_ssid || 'Unknown';
        
        if (data.sta_rssi !== undefined) {
            rssiElement.textContent = `${data.sta_rssi} dBm`;
            rssiInfo.style.display = 'flex';
        } else {
            rssiInfo.style.display = 'none';
        }
        
        staInfo.style.display = 'flex';
    } else if (data.mode === 'Access Point') {
        ipElement.textContent = data.ap_ip_address || '192.168.4.1';
        staInfo.style.display = 'none';
        rssiInfo.style.display = 'none';
    } else {
        ipElement.textContent = 'Not available';
        staInfo.style.display = 'none';
        rssiInfo.style.display = 'none';
    }
}

function updateConfigurationInfo(data) {
    const configInfo = document.getElementById('config-info');
    const infoText = configInfo.querySelector('.info-text');
    const saveBtn = document.getElementById('save-wifi-btn');
    
    if (data.mode === 'Access Point') {
        infoText.textContent = 'Currently in Access Point mode. Configure WiFi settings to connect to your network.';
        saveBtn.textContent = 'Save & Connect to WiFi';
        configInfo.style.borderLeftColor = '#856404';
        configInfo.style.backgroundColor = '#fff3cd';
    } else if (data.mode === 'Station' && data.sta_status === 'Connected') {
        infoText.textContent = 'Connected to WiFi. You can update settings below (device will reconnect).';
        saveBtn.textContent = 'Update WiFi Settings';
        configInfo.style.borderLeftColor = '#155724';
        configInfo.style.backgroundColor = '#d4edda';
    } else {
        infoText.textContent = 'Configure your WiFi settings below. Changes will be applied based on the current network mode.';
        saveBtn.textContent = 'Save WiFi Settings';
        configInfo.style.borderLeftColor = '#0066cc';
        configInfo.style.backgroundColor = '#e7f3ff';
    }
}

function updateModeControls(data) {
    const switchToApBtn = document.getElementById('switch-to-ap-btn');
    const switchToStationBtn = document.getElementById('switch-to-station-btn');
    const modeHelp = document.getElementById('mode-help');
    
    // Hide all controls by default
    switchToApBtn.style.display = 'none';
    switchToStationBtn.style.display = 'none';
    modeHelp.style.display = 'none';
    
    if (data.mode === 'Station') {
        // In station mode - offer switch to AP
        switchToApBtn.style.display = 'inline-block';
        switchToApBtn.disabled = false;
    } else if (data.mode === 'Access Point') {
        // In AP mode - offer switch to station if provisioned
        if (data.is_provisioned) {
            switchToStationBtn.style.display = 'inline-block';
            switchToStationBtn.disabled = false;
            switchToStationBtn.textContent = 'Connect to WiFi';
        } else {
            modeHelp.style.display = 'block';
            switchToStationBtn.style.display = 'inline-block';
            switchToStationBtn.disabled = true;
            switchToStationBtn.textContent = 'Connect to WiFi (Configure First)';
        }
    } else if (data.mode === 'Switching') {
        // Disable all buttons during mode switching
        switchToApBtn.style.display = 'inline-block';
        switchToStationBtn.style.display = 'inline-block';
        switchToApBtn.disabled = true;
        switchToStationBtn.disabled = true;
        switchToApBtn.textContent = 'Switching...';
        switchToStationBtn.textContent = 'Switching...';
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
        document.getElementById('wifi-ssid-input').value = data.wifi_ssid || '';
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

// Mode switching functions
async function switchNetworkMode(targetMode) {
    try {
        const response = await fetch(API_ENDPOINTS.networkMode, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ mode: targetMode })
        });

        if (response.ok) {
            const result = await response.json();
            showMessage('success', result.message || `Switching to ${targetMode} mode...`);
            
            // Disable mode buttons temporarily
            document.getElementById('switch-to-ap-btn').disabled = true;
            document.getElementById('switch-to-station-btn').disabled = true;
            
            // Refresh status after a delay to show the switching state
            setTimeout(() => {
                updateNetworkInfo();
            }, 1000);
            
            // Refresh again after mode switch should be complete
            setTimeout(() => {
                updateNetworkInfo();
            }, 5000);
            
        } else {
            const errorText = await response.text();
            showMessage('error', `Failed to switch mode: ${errorText}`);
        }
    } catch (error) {
        console.error('Error switching network mode:', error);
        showMessage('error', 'Failed to switch network mode');
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

    // Validate input
    if (!data.wifi_ssid.trim()) {
        showMessage('error', 'WiFi SSID is required');
        return;
    }

    if (!data.wifi_password.trim()) {
        showMessage('error', 'WiFi password is required');
        return;
    }

    try {
        const response = await fetch(API_ENDPOINTS.networkConfig, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        if (response.ok) {
            if (currentNetworkMode === 'access point') {
                showMessage('success', 'WiFi configuration saved. Device will attempt to connect to your network.');
            } else {
                showMessage('success', 'WiFi configuration updated successfully');
            }
            
            // Clear password field for security
            document.getElementById('wifi-password-input').value = '';
            
            // Refresh network status after a short delay
            setTimeout(updateNetworkInfo, 2000);
        } else {
            showMessage('error', 'Failed to update WiFi configuration');
        }
    } catch (error) {
        console.error('Error updating network config:', error);
        showMessage('error', 'Failed to update WiFi configuration');
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
        }, 5000);
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

    // Set up mode switching handlers
    document.getElementById('switch-to-ap-btn').addEventListener('click', function() {
        if (confirm('Switch to Access Point mode? This will disconnect from current WiFi network.')) {
            switchNetworkMode('ap');
        }
    });

    document.getElementById('switch-to-station-btn').addEventListener('click', function() {
        const isProvisioned = document.getElementById('switch-to-station-btn').disabled === false;
        if (isProvisioned) {
            if (confirm('Connect to WiFi using saved credentials?')) {
                switchNetworkMode('station');
            }
        } else {
            showMessage('warning', 'Please configure WiFi settings first');
        }
    });

    // Set up periodic updates
    setInterval(() => {
        updateSystemInfo();
        updateNetworkInfo();
        updateSensorInfo();
    }, 5000);
});
