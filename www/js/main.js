// API endpoints
const API_ENDPOINTS = {
    system: '/api/v1/system',
    network: '/api/v1/network',
    networkConfig: '/api/v1/config/network',
    mqttConfig: '/api/v1/config/mqtt',
    sensorDHT11: '/api/v1/sensors/dht11'
};

// Connection Modal Manager
class ConnectionModal {
    constructor() {
        console.log('🔍 Initializing ConnectionModal...');

        this.modal = document.getElementById('connection-modal');
        this.title = document.getElementById('modal-title');
        this.body = document.getElementById('modal-body');
        this.footer = document.getElementById('modal-footer');
        this.isActive = false;
        this.preventBackButton = false;

        console.log('✅ ConnectionModal ready');
    }

    show(title, bodyContent, footerContent = '', blocking = true) {
        this.title.textContent = title;
        this.body.innerHTML = bodyContent;
        this.footer.innerHTML = footerContent;
        
        if (blocking) {
            document.body.classList.add('modal-active');
            this.preventNavigation();
        }
        
        this.modal.classList.add('show');
        this.isActive = true;
    }

    hide() {
        this.modal.classList.remove('show');
        document.body.classList.remove('modal-active');
        this.allowNavigation();
        this.isActive = false;
    }

    preventNavigation() {
        this.preventBackButton = true;
        window.addEventListener('beforeunload', this.handleBeforeUnload);
        window.addEventListener('popstate', this.handlePopState.bind(this));
        // Push current state to prevent back button
        history.pushState(null, null, window.location.href);
    }

    allowNavigation() {
        this.preventBackButton = false;
        window.removeEventListener('beforeunload', this.handleBeforeUnload);
        window.removeEventListener('popstate', this.handlePopState.bind(this));
    }

    handleBeforeUnload(e) {
        e.preventDefault();
        e.returnValue = 'WiFi configuration in progress. Are you sure you want to leave?';
        return e.returnValue;
    }

    handlePopState(e) {
        if (this.preventBackButton && this.isActive) {
            history.pushState(null, null, window.location.href);
        }
    }

    showLoading(message = 'Connecting to WiFi...') {
        const content = `
            <div class="loading-content">
                <div class="loading-spinner"></div>
                <p><strong>${message}</strong></p>
                <p>Please wait while we connect to your network.</p>
                <p><small><strong>Do not refresh or close this page.</strong></small></p>
            </div>
        `;
        this.show('Connecting...', content, '', true);
    }

    showSuccess(wifiSSID, deviceIP) {
        const content = `
            <div class="success-content">
                <h4>✅ WiFi Connected Successfully!</h4>
                <p>Your EnviLog device is now connected to <strong>${wifiSSID}</strong>.</p>
                
                <div class="access-links">
                    <p><strong>Access your device:</strong></p>
                    <a href="http://envilog.local" class="access-link" target="_blank">
                        🌐 http://envilog.local (Recommended)
                    </a>
                    <a href="http://${deviceIP}" class="access-link secondary" target="_blank">
                        📍 http://${deviceIP} (Direct IP)
                    </a>
                </div>

                <ol>
                    <li>Connect your phone/computer to <strong>"${wifiSSID}"</strong> network</li>
                    <li>Click one of the links above, or bookmark them for future access</li>
                    <li>If links don't work, manually type the address in your browser</li>
                </ol>
            </div>
        `;
        
        const footer = `
            <button class="modal-btn primary" onclick="connectionModal.hide()">
                I've Connected
            </button>
        `;
        
        this.show('Setup Complete!', content, footer, false);
    }

    showError(errorMessage, allowRetry = true) {
        const content = `
            <div class="error-content">
                <h4>❌ Connection Failed</h4>
                <p>${errorMessage}</p>
                
                <div class="troubleshooting">
                    <h5>💡 Troubleshooting Tips:</h5>
                    <ul>
                        <li>Check that the WiFi password is correct</li>
                        <li>Ensure the network is 2.4GHz (not 5GHz only)</li>
                        <li>Try moving closer to your WiFi router</li>
                        <li>Make sure the network allows new devices to connect</li>
                    </ul>
                </div>
            </div>
        `;
        
        let footer = '';
        if (allowRetry) {
            footer = `
                <button class="modal-btn secondary" onclick="connectionModal.hide()">
                    Try Again
                </button>
            `;
        }
        
        this.show('Connection Failed', content, footer, false);
    }

    showNetworkError() {
        const content = `
            <div class="error-content">
                <h4>❌ Network Error</h4>
                <p>Could not communicate with the device. This might be a temporary issue.</p>
                
                <div class="troubleshooting">
                    <h5>💡 What to try:</h5>
                    <ul>
                        <li>Check that you're still connected to the EnviLog network</li>
                        <li>Try refreshing this page</li>
                        <li>Make sure your device's WiFi is working properly</li>
                    </ul>
                </div>
            </div>
        `;
        
        const footer = `
            <button class="modal-btn secondary" onclick="window.location.reload()">
                Refresh Page
            </button>
            <button class="modal-btn primary" onclick="connectionModal.hide()">
                Try Again
            </button>
        `;
        
        this.show('Network Error', content, footer, false);
    }
}

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
            // Update the new metric value elements
            document.getElementById('temperature-value').textContent = 
                `${data.temperature.toFixed(1)}°C`;
            document.getElementById('humidity-value').textContent = 
                `${data.humidity.toFixed(1)}%`;
            
            // Update sensor status
            const sensorStatus = document.getElementById('sensor-status');
            sensorStatus.textContent = '◯';
            sensorStatus.className = 'metric-value status-connected';
        } else {
            document.getElementById('temperature-value').textContent = '--°C';
            document.getElementById('humidity-value').textContent = '--%';
            
            // Update sensor status for error
            const sensorStatus = document.getElementById('sensor-status');
            sensorStatus.textContent = '◯';
            sensorStatus.className = 'metric-value status-disconnected';
        }
    } catch (error) {
        console.error('Error fetching sensor data:', error);
        // Show error state
        document.getElementById('temperature-value').textContent = '--°C';
        document.getElementById('humidity-value').textContent = '--%';
        
        const sensorStatus = document.getElementById('sensor-status');
        sensorStatus.textContent = '◯';
        sensorStatus.className = 'metric-value status-disconnected';
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

    // Basic validation
    if (!data.wifi_ssid.trim()) {
        alert('Please enter a WiFi network name');
        return;
    }
    if (!data.wifi_password.trim()) {
        alert('Please enter a WiFi password');
        return;
    }

    // Show loading modal
    connectionModal.showLoading('Saving configuration...');

    try {
        const response = await fetch('/api/v1/config/network', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data),
            signal: AbortSignal.timeout(8000)  // 8 second timeout
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const result = await response.json();
        console.log('Backend response:', result);

        if (result.status === 'attempting') {
            // Got response! Show universal guidance
            showUniversalGuidance(data.wifi_ssid);
        } else {
            // Unexpected response
            connectionModal.showError(result.message || 'Unexpected response from device', true);
        }

    } catch (error) {
        console.error('Network configuration error:', error);
        
        // ANY error (timeout, connection lost, etc.) = show universal guidance
        // This is expected when device switches networks
        showUniversalGuidance(data.wifi_ssid);
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

let pollingInterval = null;

function startPolling() {
    // Clear any existing interval
    if (pollingInterval) {
        clearInterval(pollingInterval);
    }
    
    pollingInterval = setInterval(async () => {
        try {
            // Test if backend is accessible
            const response = await fetch('/api/v1/system', { 
                signal: AbortSignal.timeout(3000) 
            });
            
            if (response.ok) {
                // Backend accessible - update all data
                updateSystemInfo();
                updateNetworkInfo();
                updateSensorInfo();
            } else {
                throw new Error('Backend not responding');
            }
        } catch (error) {
            console.log('Backend unreachable, stopping polls');
            stopPolling();
        }
    }, 5000);
}

function stopPolling() {
    if (pollingInterval) {
        clearInterval(pollingInterval);
        pollingInterval = null;
        console.log('Polling stopped');
    }
}

// Password Toggle Functions
function showPassword(button) {
    const inputId = button.dataset.target;
    const input = document.getElementById(inputId);
    
    if (input) {
        input.type = 'text';
        button.setAttribute('aria-pressed', 'true');
        button.setAttribute('aria-label', 'Hide password');
    }
}

function hidePassword(button) {
    const inputId = button.dataset.target;
    const input = document.getElementById(inputId);
    
    if (input) {
        input.type = 'password';
        button.setAttribute('aria-pressed', 'false');
        button.setAttribute('aria-label', 'Show password');
    }
}

// Modern Event Listener Initialization
function initPasswordToggles() {
    const passwordToggles = document.querySelectorAll('.password-toggle');
    
    passwordToggles.forEach(button => {
        // Touch/pointer events for mobile and desktop
        button.addEventListener('pointerdown', () => showPassword(button));
        button.addEventListener('pointerup', () => hidePassword(button));
        button.addEventListener('pointerleave', () => hidePassword(button));
        
        // Click event for accessibility (keyboard users)
        button.addEventListener('click', (e) => {
            e.preventDefault(); // Prevent form submission
            const isPressed = button.getAttribute('aria-pressed') === 'true';
            if (isPressed) {
                hidePassword(button);
            } else {
                showPassword(button);
            }
        });
        
        // Keyboard support
        button.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                const isPressed = button.getAttribute('aria-pressed') === 'true';
                if (isPressed) {
                    hidePassword(button);
                } else {
                    showPassword(button);
                }
            }
        });
    });
}

// Initial setup
document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM loaded, initializing modal...');
    initPasswordToggles();

    // Initialize modal AFTER DOM is ready
    const connectionModal = new ConnectionModal();
    
    // Make it globally accessible
    window.connectionModal = connectionModal;

    console.log('✅ Modal initialized successfully');
    
    // Rest of your existing DOMContentLoaded code...
    updateSystemInfo();
    updateNetworkInfo();
    updateSensorInfo();
    loadNetworkConfig();
    loadMqttConfig();

    const networkForm = document.getElementById('network-config-form');
    if (networkForm) {
        networkForm.addEventListener('submit', handleNetworkConfigSubmit);
    }

    const mqttForm = document.getElementById('mqtt-config-form');
    if (mqttForm) {
        mqttForm.addEventListener('submit', handleMqttConfigSubmit);
    }

    // Start smart polling instead of simple setInterval
    startPolling();
});

// Universal guidance modal - covers all scenarios
function showUniversalGuidance(homeSSID) {
    const content = `
        <div class="universal-guidance">
            <h4>WiFi Configuration Sent</h4>
            <p>Device is attempting connection. Choose one option:</p>
            
            <div class="connection-options">
                <div class="option-card primary-option">
                    <h5>Connect to: "${homeSSID}"</h5>
                    <p>Use your entered password → Access: <a href="http://envilog.local" target="_blank">envilog.local</a></p>
                </div>
                
                <div class="option-divider"><span>OR</span></div>
                
                <div class="option-card secondary-option">
                    <h5>Connect to: "EnviLog"</h5>
                    <p>Password: <span class="password-display">envilog1212</span> → Access: <a href="http://192.168.4.1" target="_blank">192.168.4.1</a></p>
                </div>
            </div>
            
            <div class="status-indicators">
                <p><strong>Tip:</strong> Refresh WiFi list to see if "EnviLog" reappears (indicates connection failed)</p>
            </div>
        </div>
    `;
    
    const footer = `
        <button class="modal-btn secondary" onclick="openWiFiSettings()">
            OPEN WIFI SETTINGS
        </button>
        <button class="modal-btn primary" onclick="connectionModal.hide()">
            I'LL CONNECT NOW
        </button>
    `;
    
    connectionModal.show('Choose Connection Method', content, footer, false);
}

// Helper functions
function copyToClipboard(text) {
    navigator.clipboard.writeText(text).then(() => {
        // Provide visual feedback
        const copyBtn = event.target;
        const originalText = copyBtn.textContent;
        copyBtn.textContent = '✅ Copied!';
        copyBtn.style.background = '#28a745';
        
        setTimeout(() => {
            copyBtn.textContent = originalText;
            copyBtn.style.background = '';
        }, 2000);
    }).catch(() => {
        // Fallback for older browsers
        alert('Password: envilog1212');
    });
}

function openWiFiSettings() {
    // Attempt to open WiFi settings (limited browser support)
    if (navigator.userAgent.includes('iPhone') || navigator.userAgent.includes('iPad')) {
        alert('Please open Settings > WiFi on your device');
    } else if (navigator.userAgent.includes('Android')) {
        alert('Please open Settings > WiFi on your device');
    } else {
        alert('Please open your WiFi settings');
    }
}
