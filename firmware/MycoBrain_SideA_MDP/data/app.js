// WebSocket connection
let ws = null;
let reconnectInterval = null;

function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log('WebSocket connected');
        document.getElementById('sensor-status').textContent = 'Connected';
        document.getElementById('sensor-status').className = 'status connected';
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
    };
    
    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            updateSensorDisplay(data);
        } catch (e) {
            console.error('Failed to parse WebSocket message:', e);
        }
    };
    
    ws.onerror = (error) => {
        console.error('WebSocket error:', error);
    };
    
    ws.onclose = () => {
        console.log('WebSocket disconnected');
        document.getElementById('sensor-status').textContent = 'Disconnected';
        document.getElementById('sensor-status').className = 'status disconnected';
        
        // Reconnect after 3 seconds
        if (!reconnectInterval) {
            reconnectInterval = setInterval(connectWebSocket, 3000);
        }
    };
}

function updateSensorDisplay(data) {
    if (data.ai_volts) {
        for (let i = 0; i < 4; i++) {
            const elem = document.getElementById(`ai${i + 1}`);
            if (elem && data.ai_volts[i] !== undefined) {
                elem.textContent = data.ai_volts[i].toFixed(3);
            }
        }
    }
    
    if (data.mos) {
        for (let i = 0; i < 3; i++) {
            const elem = document.getElementById(`mos${i + 1}`);
            if (elem && data.mos[i] !== undefined) {
                elem.textContent = data.mos[i] ? 'ON' : 'OFF';
            }
        }
    }
}

// Initialize forms
function initForms() {
    // WiFi form
    const wifiForm = document.getElementById('wifi-form');
    const staEnabled = document.getElementById('sta-enabled');
    const staConfig = document.getElementById('sta-config');
    
    staEnabled.addEventListener('change', () => {
        staConfig.style.display = staEnabled.checked ? 'block' : 'none';
    });
    
    wifiForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = {
            wifi_mode: parseInt(document.getElementById('wifi-mode').value),
            ap_ssid: document.getElementById('ap-ssid').value,
            ap_password: document.getElementById('ap-password').value,
            sta_enabled: staEnabled.checked,
            sta_ssid: document.getElementById('sta-ssid').value,
            sta_password: document.getElementById('sta-password').value
        };
        
        try {
            const response = await fetch('/api/wifi/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData)
            });
            const result = await response.json();
            showMessage(wifiForm, result.status === 'ok' ? 'WiFi config saved. Reboot required.' : 'Error: ' + result.error, result.status === 'ok');
        } catch (error) {
            showMessage(wifiForm, 'Error: ' + error.message, false);
        }
    });
    
    // Calibration form
    initCalibrationForm();
    
    // Pins form
    initPinsForm();
    
    // Thresholds form
    initThresholdsForm();
    
    // Load WiFi status
    loadWiFiStatus();
}

function initCalibrationForm() {
    const container = document.getElementById('analog-calibration');
    for (let i = 0; i < 4; i++) {
        const div = document.createElement('div');
        div.className = 'form-group';
        div.innerHTML = `
            <h4>AI${i + 1}</h4>
            <label>Offset:</label>
            <input type="number" id="ai${i}-offset" step="0.001" value="0.0">
            <label>Gain:</label>
            <input type="number" id="ai${i}-gain" step="0.001" min="0.1" max="10.0" value="1.0">
        `;
        container.appendChild(div);
    }
    
    document.getElementById('calibration-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = {
            adc_reference: parseFloat(document.getElementById('adc-ref').value),
            analog_offset: [],
            analog_gain: [],
            bme_temp_offset: parseFloat(document.getElementById('bme-temp-offset').value),
            bme_humidity_offset: parseFloat(document.getElementById('bme-humidity-offset').value)
        };
        
        for (let i = 0; i < 4; i++) {
            formData.analog_offset.push(parseFloat(document.getElementById(`ai${i}-offset`).value));
            formData.analog_gain.push(parseFloat(document.getElementById(`ai${i}-gain`).value));
        }
        
        try {
            const response = await fetch('/api/config/calibration', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData)
            });
            const result = await response.json();
            showMessage(document.getElementById('calibration-form'), result.status === 'ok' ? 'Calibration saved' : 'Error: ' + result.error, result.status === 'ok');
        } catch (error) {
            showMessage(document.getElementById('calibration-form'), 'Error: ' + error.message, false);
        }
    });
}

function initPinsForm() {
    const aiContainer = document.getElementById('ai-pins');
    const mosContainer = document.getElementById('mos-pins');
    
    for (let i = 0; i < 4; i++) {
        const div = document.createElement('div');
        div.className = 'form-group';
        div.innerHTML = `<label>AI${i + 1} Pin:</label><input type="number" id="ai${i}-pin" min="0" max="48" value="${[6,7,10,11][i]}">`;
        aiContainer.appendChild(div);
    }
    
    for (let i = 0; i < 3; i++) {
        const div = document.createElement('div');
        div.className = 'form-group';
        div.innerHTML = `<label>MOS${i + 1} Pin:</label><input type="number" id="mos${i}-pin" min="0" max="48" value="${[12,13,14][i]}">`;
        mosContainer.appendChild(div);
    }
    
    document.getElementById('pins-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = {
            ai_pins: [],
            mos_pins: [],
            i2c_sda: parseInt(document.getElementById('i2c-sda').value),
            i2c_scl: parseInt(document.getElementById('i2c-scl').value)
        };
        
        for (let i = 0; i < 4; i++) {
            formData.ai_pins.push(parseInt(document.getElementById(`ai${i}-pin`).value));
        }
        for (let i = 0; i < 3; i++) {
            formData.mos_pins.push(parseInt(document.getElementById(`mos${i}-pin`).value));
        }
        
        try {
            const response = await fetch('/api/config/pins', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData)
            });
            const result = await response.json();
            showMessage(document.getElementById('pins-form'), result.status === 'ok' ? 'Pin config saved. Reboot required.' : 'Error: ' + result.error, result.status === 'ok');
        } catch (error) {
            showMessage(document.getElementById('pins-form'), 'Error: ' + error.message, false);
        }
    });
}

function initThresholdsForm() {
    const container = document.getElementById('threshold-config');
    for (let i = 0; i < 4; i++) {
        const div = document.createElement('div');
        div.className = 'form-group';
        div.innerHTML = `
            <h4>AI${i + 1}</h4>
            <label>Low Threshold:</label>
            <input type="number" id="ai${i}-low" step="0.01" value="0.1">
            <label>High Threshold:</label>
            <input type="number" id="ai${i}-high" step="0.01" value="3.0">
        `;
        container.appendChild(div);
    }
    
    document.getElementById('thresholds-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = {
            analog_low: [],
            analog_high: []
        };
        
        for (let i = 0; i < 4; i++) {
            formData.analog_low.push(parseFloat(document.getElementById(`ai${i}-low`).value));
            formData.analog_high.push(parseFloat(document.getElementById(`ai${i}-high`).value));
        }
        
        try {
            const response = await fetch('/api/config/thresholds', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData)
            });
            const result = await response.json();
            showMessage(document.getElementById('thresholds-form'), result.status === 'ok' ? 'Thresholds saved' : 'Error: ' + result.error, result.status === 'ok');
        } catch (error) {
            showMessage(document.getElementById('thresholds-form'), 'Error: ' + error.message, false);
        }
    });
}

async function loadWiFiStatus() {
    try {
        const response = await fetch('/api/wifi/status');
        const status = await response.json();
        
        document.getElementById('wifi-mode').value = status.wifi_mode || 0;
        document.getElementById('ap-ssid').value = status.ap_ssid || '';
        document.getElementById('sta-enabled').checked = status.sta_enabled || false;
        if (status.sta_enabled) {
            document.getElementById('sta-config').style.display = 'block';
            document.getElementById('sta-ssid').value = status.sta_ssid || '';
        }
        
        const statusDiv = document.getElementById('wifi-status');
        let statusText = `AP IP: ${status.ap_ip || 'N/A'}`;
        if (status.sta_connected) {
            statusText += ` | STA IP: ${status.sta_ip} | RSSI: ${status.sta_rssi} dBm`;
        }
        statusDiv.textContent = statusText;
    } catch (error) {
        console.error('Failed to load WiFi status:', error);
    }
}

function showMessage(form, message, success) {
    const existing = form.querySelector('.message');
    if (existing) {
        existing.remove();
    }
    
    const msg = document.createElement('div');
    msg.className = `message ${success ? 'success' : 'error'}`;
    msg.textContent = message;
    form.appendChild(msg);
    
    setTimeout(() => msg.remove(), 5000);
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    initForms();
    
    // Refresh WiFi status every 10 seconds
    setInterval(loadWiFiStatus, 10000);
});

