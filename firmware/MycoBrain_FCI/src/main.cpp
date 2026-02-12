/**
 * MycoBrain FCI - Fungal Computer Interface Firmware
 * 
 * This firmware transforms MycoBrain into a Fungal Computer Interface (FCI),
 * enabling bidirectional communication with mycelial networks.
 * 
 * PHYSICS: Ion channel dynamics (K+, Ca2+, Na+), membrane potentials (-70 to +40 mV)
 * CHEMISTRY: Glutamate/GABA signaling, chemotropic gradients
 * BIOLOGY: Action potential-like spikes, network propagation (0.5-50 mm/min)
 * 
 * Based on: Global Fungi Symbiosis Theory (GFST)
 * References: Adamatzky (2018), Olsson & Hansson (1995), Simard (2018)
 * 
 * (c) 2026 Mycosoft Labs - Data Protocol for Nature
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_BME680.h>
#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>

#include "fci_config.h"
#include "fci_signal.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

Adafruit_ADS1115 ads;                    // 16-bit ADC for bioelectric signals
Adafruit_BME680 bme;                     // Environmental sensor
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsClient webSocket;
FCISignalProcessor signalProcessor;
FCIStimulusGenerator stimulator;

// Device identity
char deviceId[16];
char macAddress[18];

// WiFi credentials (loaded from preferences or hardcoded for dev)
const char* WIFI_SSID = "MYCOSOFT_LAB";
const char* WIFI_PASS = "Mushroom1!";

// Mycorrhizae Protocol configuration
const char* MYCORRHIZAE_URL = "192.168.0.188";
const int MYCORRHIZAE_PORT = 8002;
const char* API_KEY = "mcr_fci_device_key";  // Will be provisioned

// ============================================================================
// SIGNAL BUFFERS
// ============================================================================

// Raw sample buffer (circular)
volatile int16_t sampleBuffer[ADC_BUFFER_SIZE];
volatile size_t sampleWriteIndex = 0;
volatile size_t sampleReadIndex = 0;
volatile bool bufferReady = false;

// Processing buffer
float processingBuffer[FFT_SAMPLES];
float frequencyBuffer[FFT_SAMPLES];

// FFT objects
ArduinoFFT<float> FFT = ArduinoFFT<float>(processingBuffer, frequencyBuffer, FFT_SAMPLES, FFT_SAMPLE_FREQ, true);

// ============================================================================
// TELEMETRY STATE
// ============================================================================

fci_telemetry_t currentTelemetry;
fci_features_t currentFeatures;
uint32_t lastTelemetryTime = 0;
uint32_t lastEnvReadTime = 0;
uint32_t bootTime = 0;
bool wsConnected = false;

// ============================================================================
// SAMPLING TIMER (Hardware timer for precise sampling)
// ============================================================================

hw_timer_t* sampleTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onSampleTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    
    // Read differential bioelectric signal (A0-A1)
    // Note: This is blocking in ISR but ADS1115 is fast enough at 128 SPS
    int16_t raw = ads.readADC_Differential_0_1();
    
    // Store in circular buffer
    sampleBuffer[sampleWriteIndex] = raw;
    sampleWriteIndex = (sampleWriteIndex + 1) % ADC_BUFFER_SIZE;
    
    // Check if buffer is full
    if (sampleWriteIndex == 0) {
        bufferReady = true;
    }
    
    portEXIT_CRITICAL_ISR(&timerMux);
}

// ============================================================================
// WEBSOCKET CALLBACKS
// ============================================================================

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected");
            wsConnected = false;
            pixel.setPixelColor(0, pixel.Color(255, 165, 0)); // Orange
            pixel.show();
            break;
            
        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to %s\n", payload);
            wsConnected = true;
            pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green
            pixel.show();
            // Subscribe to device command channel
            {
                JsonDocument doc;
                doc["action"] = "subscribe";
                doc["channel"] = String("device.") + deviceId + ".commands";
                String msg;
                serializeJson(doc, msg);
                webSocket.sendTXT(msg);
            }
            break;
            
        case WStype_TEXT:
            Serial.printf("[WS] Received: %s\n", payload);
            handleWebSocketMessage((char*)payload, length);
            break;
            
        case WStype_BIN:
            Serial.printf("[WS] Received binary data, length: %u\n", length);
            break;
            
        case WStype_PING:
            Serial.println("[WS] Ping");
            break;
            
        case WStype_PONG:
            Serial.println("[WS] Pong");
            break;
    }
}

void handleWebSocketMessage(char* payload, size_t length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[WS] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    const char* action = doc["action"] | "";
    
    if (strcmp(action, "stimulus") == 0) {
        // Handle stimulus command
        const char* waveform = doc["waveform"] | "pulse";
        float amplitude = doc["amplitude"] | 10.0f;
        float frequency = doc["frequency"] | 1.0f;
        uint32_t duration = doc["duration"] | 1000;
        
        stim_waveform_t wf = STIM_WAVEFORM_PULSE;
        if (strcmp(waveform, "sine") == 0) wf = STIM_WAVEFORM_SINE;
        else if (strcmp(waveform, "dc") == 0) wf = STIM_WAVEFORM_DC;
        else if (strcmp(waveform, "ramp") == 0) wf = STIM_WAVEFORM_RAMP;
        
        Serial.printf("[STIM] Starting %s stimulus: %.1f µV @ %.1f Hz for %u ms\n",
                      waveform, amplitude, frequency, duration);
        stimulator.startStimulus(wf, amplitude, frequency, duration);
        
    } else if (strcmp(action, "calibrate") == 0) {
        // Handle calibration command
        Serial.println("[CAL] Starting calibration...");
        performCalibration();
        
    } else if (strcmp(action, "config") == 0) {
        // Handle configuration update
        if (doc.containsKey("sample_rate")) {
            // Would need to restart timer with new rate
            Serial.printf("[CFG] Sample rate update requested: %d\n", (int)doc["sample_rate"]);
        }
    }
}

// ============================================================================
// TELEMETRY FUNCTIONS
// ============================================================================

void sendTelemetry() {
    // Build telemetry JSON
    JsonDocument doc;
    
    // Envelope header (Mycorrhizae Protocol format)
    doc["id"] = generateUUID();
    doc["channel"] = String("device.") + deviceId + ".telemetry";
    doc["timestamp"] = getISOTimestamp();
    doc["ttl_seconds"] = 3600;
    
    // Source identification
    JsonObject source = doc["source"].to<JsonObject>();
    source["type"] = "fci";
    source["id"] = deviceId;
    source["device_serial"] = macAddress;
    source["firmware"] = FCI_FIRMWARE_VERSION;
    
    doc["message_type"] = "fci_telemetry";
    
    // Payload - bioelectric features
    JsonObject payload = doc["payload"].to<JsonObject>();
    
    JsonObject bio = payload["bioelectric"].to<JsonObject>();
    bio["amplitude_uv"] = currentFeatures.amplitude_uv;
    bio["rms_uv"] = currentFeatures.rms_uv;
    bio["mean_uv"] = currentFeatures.mean_uv;
    bio["std_uv"] = currentFeatures.std_uv;
    bio["dominant_freq_hz"] = currentFeatures.dominant_freq_hz;
    bio["total_power"] = currentFeatures.total_power;
    bio["snr_db"] = currentFeatures.snr_db;
    bio["pattern"] = patternToString(currentFeatures.pattern);
    bio["pattern_confidence"] = currentFeatures.pattern_confidence;
    bio["sample_count"] = currentTelemetry.sample_count;
    
    // Environmental data
    JsonObject env = payload["environment"].to<JsonObject>();
    env["temperature_c"] = currentTelemetry.temperature_c;
    env["humidity_pct"] = currentTelemetry.humidity_pct;
    env["pressure_hpa"] = currentTelemetry.pressure_hpa;
    env["voc_index"] = currentTelemetry.voc_index;
    
    // Device status
    JsonObject status = payload["status"].to<JsonObject>();
    status["uptime_ms"] = millis() - bootTime;
    status["wifi_rssi"] = WiFi.RSSI();
    status["impedance_ohms"] = currentTelemetry.impedance_ohms;
    status["stimulus_active"] = stimulator.isActive();
    
    // Serialize and send
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    if (wsConnected) {
        webSocket.sendTXT(jsonStr);
    } else {
        // Fallback to HTTP POST
        sendHTTPTelemetry(jsonStr);
    }
}

void sendHTTPTelemetry(const String& json) {
    HTTPClient http;
    
    String url = String("http://") + MYCORRHIZAE_URL + ":" + MYCORRHIZAE_PORT +
                 "/api/channels/device." + deviceId + ".telemetry/publish";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", API_KEY);
    
    int httpCode = http.POST(json);
    
    if (httpCode > 0) {
        if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_CREATED) {
            Serial.printf("[HTTP] Telemetry error: %d\n", httpCode);
        }
    } else {
        Serial.printf("[HTTP] Connection failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}

// ============================================================================
// PATTERN DETECTION (Based on GFST signal characteristics)
// ============================================================================

fci_pattern_t detectSignalPattern(const fci_features_t* features) {
    float freq = features->dominant_freq_hz;
    float amp = features->amplitude_uv;
    float confidence = 0.0f;
    fci_pattern_t pattern = PATTERN_BASELINE;
    
    // Growth pattern: 0.1-5 Hz, 0.5-1.0 µV quasi-periodic
    if (freq >= GROWTH_FREQ_MIN && freq <= GROWTH_FREQ_MAX &&
        amp >= GROWTH_AMP_MIN_UV && amp <= GROWTH_AMP_MAX_UV) {
        pattern = PATTERN_GROWTH;
        confidence = 0.7f + 0.3f * (1.0f - abs(freq - 1.0f) / 5.0f);
    }
    
    // Stress pattern: 5-20 Hz, >1.0 µV elevated activity
    else if (freq >= STRESS_FREQ_MIN && freq <= STRESS_FREQ_MAX &&
             amp >= STRESS_AMP_MIN_UV) {
        pattern = PATTERN_STRESS;
        confidence = 0.6f + 0.4f * min(1.0f, amp / 5.0f);
    }
    
    // Seismic precursor: 0.01-0.1 Hz, very low frequency drift
    else if (freq >= SEISMIC_FREQ_MIN && freq <= SEISMIC_FREQ_MAX) {
        pattern = PATTERN_SEISMIC;
        confidence = 0.5f; // Requires time validation
    }
    
    // Spike detection: high amplitude, short duration
    else if (amp > 3.0f * features->std_uv) {
        pattern = PATTERN_SPIKE;
        confidence = min(1.0f, amp / (5.0f * features->std_uv));
    }
    
    // Low activity baseline
    else if (amp < 0.1f) {
        pattern = PATTERN_BASELINE;
        confidence = 0.9f;
    }
    
    // Unknown pattern
    else {
        pattern = PATTERN_UNKNOWN;
        confidence = 0.3f;
    }
    
    return pattern;
}

const char* patternToString(fci_pattern_t pattern) {
    switch (pattern) {
        case PATTERN_BASELINE:     return "baseline";
        case PATTERN_GROWTH:       return "growth";
        case PATTERN_STRESS:       return "stress";
        case PATTERN_NUTRIENT_SEEK: return "nutrient_seeking";
        case PATTERN_COMMUNICATION: return "communication";
        case PATTERN_SEISMIC:      return "seismic_precursor";
        case PATTERN_SPIKE:        return "spike";
        default:                   return "unknown";
    }
}

// ============================================================================
// CALIBRATION
// ============================================================================

void performCalibration() {
    Serial.println("[CAL] Calibrating ADC...");
    
    // Disconnect mycelium (user should ensure electrodes are shorted)
    pixel.setPixelColor(0, pixel.Color(255, 255, 0)); // Yellow
    pixel.show();
    
    // Collect baseline samples
    float sum = 0;
    float sumSq = 0;
    const int CAL_SAMPLES = 1000;
    
    for (int i = 0; i < CAL_SAMPLES; i++) {
        int16_t raw = ads.readADC_Differential_0_1();
        float uv = signalProcessor.rawToMicrovolts(raw);
        sum += uv;
        sumSq += uv * uv;
        delay(1);
    }
    
    float mean = sum / CAL_SAMPLES;
    float variance = (sumSq / CAL_SAMPLES) - (mean * mean);
    float noise_floor = sqrt(variance);
    
    Serial.printf("[CAL] Baseline: %.2f µV, Noise floor: %.2f µV RMS\n", mean, noise_floor);
    
    // Store calibration values
    // TODO: Save to EEPROM/Preferences
    
    pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green
    pixel.show();
    
    // Send calibration result to server
    JsonDocument doc;
    doc["action"] = "calibration_complete";
    doc["device_id"] = deviceId;
    doc["baseline_uv"] = mean;
    doc["noise_floor_uv"] = noise_floor;
    doc["timestamp"] = getISOTimestamp();
    
    String msg;
    serializeJson(doc, msg);
    if (wsConnected) {
        webSocket.sendTXT(msg);
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

String generateUUID() {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();
    
    char uuid[37];
    snprintf(uuid, sizeof(uuid), "%08x-%04x-%04x-%04x-%012llx",
             r1, (r2 >> 16) & 0xFFFF, (r2 & 0xFFFF) | 0x4000,
             ((r3 >> 16) & 0x3FFF) | 0x8000, ((uint64_t)r3 << 32) | r4);
    return String(uuid);
}

String getISOTimestamp() {
    // Get time from NTP or use uptime-based timestamp
    // For now, use a placeholder format
    uint32_t uptime_s = millis() / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "2026-02-10T%02d:%02d:%02dZ",
             (uptime_s / 3600) % 24, (uptime_s / 60) % 60, uptime_s % 60);
    return String(buf);
}

void initDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(deviceId, sizeof(deviceId), "FCI-%02X%02X%02X",
             mac[3], mac[4], mac[5]);
    Serial.printf("[INIT] Device ID: %s\n", deviceId);
}

// ============================================================================
// STATUS LED PATTERNS
// ============================================================================

void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

void pulseStatusLED() {
    static uint8_t brightness = 0;
    static int8_t direction = 5;
    
    brightness += direction;
    if (brightness >= 250 || brightness <= 5) direction = -direction;
    
    // Color based on pattern
    switch (currentFeatures.pattern) {
        case PATTERN_GROWTH:
            pixel.setPixelColor(0, pixel.Color(0, brightness, 0)); // Green pulse
            break;
        case PATTERN_STRESS:
            pixel.setPixelColor(0, pixel.Color(brightness, brightness/2, 0)); // Orange pulse
            break;
        case PATTERN_SEISMIC:
            pixel.setPixelColor(0, pixel.Color(brightness, 0, 0)); // Red pulse
            break;
        case PATTERN_SPIKE:
            pixel.setPixelColor(0, pixel.Color(brightness, brightness, brightness)); // White pulse
            break;
        default:
            pixel.setPixelColor(0, pixel.Color(0, 0, brightness)); // Blue pulse
            break;
    }
    pixel.show();
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════════════════════════════╗");
    Serial.println("║     MycoBrain FCI - Fungal Computer Interface       ║");
    Serial.println("║          Data Protocol for Nature v" FCI_FIRMWARE_VERSION "             ║");
    Serial.println("║               (c) 2026 Mycosoft Labs                ║");
    Serial.println("╚════════════════════════════════════════════════════╝\n");
    
    bootTime = millis();
    
    // Initialize NeoPixel (status LED)
    pixel.begin();
    pixel.setBrightness(50);
    setStatusLED(0, 0, 255); // Blue - starting up
    
    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000); // 400 kHz I2C
    
    // Initialize ADS1115 (16-bit ADC for bioelectric signals)
    Serial.print("[INIT] ADS1115 ADC... ");
    if (!ads.begin(ADS1115_I2C_ADDR)) {
        Serial.println("FAILED!");
        setStatusLED(255, 0, 0);
        while(1) delay(100);
    }
    
    // Configure ADC for bioelectric signals
    // Gain 16 = ±256mV range, 7.8125 µV resolution - perfect for mycelium
    ads.setGain(GAIN_SIXTEEN);
    ads.setDataRate(RATE_ADS1115_128SPS);
    Serial.println("OK (Gain 16x, 128 SPS)");
    
    // Initialize BME688 (environmental sensor)
    Serial.print("[INIT] BME688... ");
    if (!bme.begin(BME688_I2C_ADDR)) {
        Serial.println("Not found (continuing without environmental data)");
    } else {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); // 320°C for 150ms
        Serial.println("OK");
    }
    
    // Initialize signal processor
    Serial.print("[INIT] Signal Processor... ");
    if (!signalProcessor.begin(ADC_SAMPLE_FREQ)) {
        Serial.println("FAILED!");
        setStatusLED(255, 0, 0);
        while(1) delay(100);
    }
    Serial.println("OK");
    
    // Initialize stimulus generator
    Serial.print("[INIT] Stimulus Generator... ");
    if (!stimulator.begin()) {
        Serial.println("FAILED!");
    } else {
        Serial.println("OK");
    }
    
    // Get device identity
    initDeviceId();
    
    // Connect to WiFi
    Serial.printf("[WIFI] Connecting to %s... ", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
        delay(500);
        Serial.print(".");
        wifi_attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(" Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        setStatusLED(0, 255, 0); // Green
        
        // Connect to Mycorrhizae WebSocket
        Serial.printf("[WS] Connecting to ws://%s:%d/api/stream/ws\n", 
                      MYCORRHIZAE_URL, MYCORRHIZAE_PORT);
        webSocket.begin(MYCORRHIZAE_URL, MYCORRHIZAE_PORT, "/api/stream/ws");
        webSocket.onEvent(webSocketEvent);
        webSocket.setReconnectInterval(WS_RECONNECT_DELAY_MS);
        
    } else {
        Serial.println(" FAILED (running in offline mode)");
        setStatusLED(255, 165, 0); // Orange
    }
    
    // Initialize sampling timer (128 Hz)
    Serial.print("[INIT] Sample Timer... ");
    uint32_t timer_period_us = 1000000 / ADC_SAMPLE_RATE; // ~7812 µs for 128 Hz
    sampleTimer = timerBegin(1000000); // 1 MHz timer
    timerAttachInterrupt(sampleTimer, &onSampleTimer);
    timerAlarm(sampleTimer, timer_period_us, true, 0);
    Serial.printf("OK (%d Hz)\n", ADC_SAMPLE_RATE);
    
    Serial.println("\n[READY] FCI initialized - listening to mycelium...\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t now = millis();
    
    // WebSocket maintenance
    webSocket.loop();
    
    // Check if sample buffer is ready for processing
    if (bufferReady) {
        bufferReady = false;
        
        // Copy samples to processing buffer
        portENTER_CRITICAL(&timerMux);
        for (size_t i = 0; i < FFT_SAMPLES; i++) {
            size_t idx = (sampleReadIndex + i) % ADC_BUFFER_SIZE;
            processingBuffer[i] = signalProcessor.rawToMicrovolts(sampleBuffer[idx]);
        }
        sampleReadIndex = (sampleReadIndex + FFT_SAMPLES) % ADC_BUFFER_SIZE;
        portEXIT_CRITICAL(&timerMux);
        
        // Process signal and extract features
        if (signalProcessor.processBuffer(&currentFeatures)) {
            // Detect pattern
            currentFeatures.pattern = detectSignalPattern(&currentFeatures);
            currentTelemetry.sample_count += FFT_SAMPLES;
            
            // Debug output
            Serial.printf("[BIO] Amp: %.2f µV | Freq: %.2f Hz | Pattern: %s (%.0f%%)\n",
                          currentFeatures.amplitude_uv,
                          currentFeatures.dominant_freq_hz,
                          patternToString(currentFeatures.pattern),
                          currentFeatures.pattern_confidence * 100);
        }
    }
    
    // Read environmental sensors periodically
    if (now - lastEnvReadTime >= ENV_SAMPLE_INTERVAL_MS) {
        lastEnvReadTime = now;
        
        if (bme.performReading()) {
            currentTelemetry.temperature_c = bme.temperature;
            currentTelemetry.humidity_pct = bme.humidity;
            currentTelemetry.pressure_hpa = bme.pressure / 100.0f;
            currentTelemetry.voc_index = bme.gas_resistance / 1000.0f;
        }
    }
    
    // Send telemetry
    if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryTime = now;
        sendTelemetry();
    }
    
    // Update stimulus generator
    stimulator.update();
    
    // Update status LED
    pulseStatusLED();
    
    // Small delay to prevent watchdog issues
    delay(1);
}
