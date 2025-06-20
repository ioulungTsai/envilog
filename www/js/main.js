// API endpoints
const API_ENDPOINTS = {
    system: '/api/v1/system',
    network: '/api/v1/network',
    networkConfig: '/api/v1/config/network',
    mqttConfig: '/api/v1/config/mqtt',
    sensorDHT11: '/api/v1/sensors/dht11'
};

// Modal functions
function showUniversalGuidance(homeSSID) {
    const modalBody = document.getElementById('modal-body');
    const modalFooter = document.getElementById('modal-footer');
    
    // Build the complete modal content
    modalBody.innerHTML = `
        <div class="universal-guidance">
            <h3 id="modal-title">WiFi Configuration Sent</h3>
            <p>Check status:</p>
            
            <div class="status-checks">
                <div class="status-option success">
                    <h4><span class="led-indicator blue">●</span> Blue LED</h4>
                    <p>Connected to "${homeSSID}"</p>
                </div>
                
                <div class="option-divider"><span>OR</span></div>
                
                <div class="status-option fallback">
                    <h4><span class="led-indicator green">●</span> Green LED</h4>
                    <p>Connect to "EnviLog"<br>(password: <span class="password-display">envilog1212</span>)</p>
                </div>
            </div>
            
            <div class="next-step">
                <p>After connect to correct network<br>→ Browse to envilog.local</p>
            </div>
            
            <div class="led-fallback">
                <p>Can't see the LED?<br>Go check your WiFi list instead.<br>Refresh the WiFi list may required</p>
            </div>
        </div>
    `;
    
    modalFooter.innerHTML = `
        <button class="modal-btn primary" onclick="window.location.reload()">
            RELOAD
        </button>
    `;
    
    // Show modal with animation
    const modal = document.getElementById('connection-modal');
    modal.removeAttribute('hidden');
    setTimeout(() => modal.classList.add('show'), 50);
}

function hideModal() {
    const modal = document.getElementById('connection-modal');
    modal.classList.remove('show');  // Trigger hide animation
    
    // Wait for actual transition to complete
    modal.addEventListener('transitionend', function hideComplete() {
        modal.setAttribute('hidden', '');
        modal.removeEventListener('transitionend', hideComplete);
    });
}

// Helper function to show status messages
function showToast(message, type = 'success') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    document.body.appendChild(toast);

    // Show animation
    setTimeout(() => toast.classList.add('show'), 50);
    
    // Hide animation after 3 seconds
    setTimeout(() => {
        toast.classList.remove('show');  // Trigger hide animation
        setTimeout(() => toast.remove(), 800);  // Remove after animation completes (match CSS duration)
    }, 3000);
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
            document.getElementById('temperature-value').textContent = 
                `${data.temperature.toFixed(1)}°C`;
            document.getElementById('humidity-value').textContent = 
                `${data.humidity.toFixed(1)}%`;
            
            const sensorStatus = document.getElementById('sensor-status');
            sensorStatus.textContent = '◯';
            sensorStatus.className = 'metric-value status-connected';
        } else {
            document.getElementById('temperature-value').textContent = '--°C';
            document.getElementById('humidity-value').textContent = '--%';
            
            const sensorStatus = document.getElementById('sensor-status');
            sensorStatus.textContent = '◯';
            sensorStatus.className = 'metric-value status-disconnected';
        }
    } catch (error) {
        console.error('Error fetching sensor data:', error);
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
    }
}

async function loadMqttConfig() {
    try {
        const response = await fetch(API_ENDPOINTS.mqttConfig);
        const data = await response.json();
        document.getElementById('broker-url').value = data.broker_url || '';
    } catch (error) {
        console.error('Error loading MQTT config:', error);
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

    if (!data.wifi_ssid.trim() || !data.wifi_password.trim()) {
        alert('Please enter WiFi credentials');
        return;
    }

    try {
        await fetch(API_ENDPOINTS.networkConfig, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    } catch (error) {
        // Expected when network switches
    }
    
    showUniversalGuidance(data.wifi_ssid);
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
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (response.ok) {
            showToast('MQTT broker updated');
        } else {
            showToast('Failed to update MQTT settings', 'error');
        }
    } catch (error) {
        console.error('Error updating MQTT config:', error);  // Keep error logging
        showToast('Failed to update MQTT settings', 'error');
    }
}

// Smart polling
let pollingInterval = null;

function startPolling() {
    if (pollingInterval) {
        clearInterval(pollingInterval);
    }
    
    pollingInterval = setInterval(async () => {
        try {
            const response = await fetch(API_ENDPOINTS.system, { 
                signal: AbortSignal.timeout(3000) 
            });
            
            if (response.ok) {
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
    }
}

// Password toggle functionality
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

function initPasswordToggles() {
    const passwordToggles = document.querySelectorAll('.password-toggle');
    
    passwordToggles.forEach(button => {
        // Mouse events
        button.addEventListener('mousedown', () => showPassword(button));
        button.addEventListener('mouseup', () => hidePassword(button));
        button.addEventListener('mouseleave', () => hidePassword(button));
        
        // Touch events  
        button.addEventListener('touchstart', () => showPassword(button));
        button.addEventListener('touchend', () => hidePassword(button));
        button.addEventListener('touchcancel', () => hidePassword(button));
        
        // Keyboard accessibility
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
        
        button.addEventListener('blur', () => hidePassword(button));
    });
}

// Initialize everything
document.addEventListener('DOMContentLoaded', function() {
    initPasswordToggles();
    
    // Load initial data
    updateSystemInfo();
    updateNetworkInfo();
    updateSensorInfo();
    loadNetworkConfig();
    loadMqttConfig();

    // Set up form handlers
    const networkForm = document.getElementById('network-config-form');
    if (networkForm) {
        networkForm.addEventListener('submit', handleNetworkConfigSubmit);
    }

    const mqttForm = document.getElementById('mqtt-config-form');
    if (mqttForm) {
        mqttForm.addEventListener('submit', handleMqttConfigSubmit);
    }

    // Start polling
    startPolling();
});
