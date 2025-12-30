/**
 * MycoBrain Science Communications Firmware
 * Main Entry Point
 * 
 * ESP32-S3 firmware for science communication capabilities:
 * - Optical modem (LiFi) via NeoPixel
 * - Acoustic modem (FSK) via buzzer
 * - Stimulus patterns for experiments
 * - Peripheral discovery and reporting
 * - JSON-CLI / NDJSON protocol
 * 
 * Hardware:
 * - NeoPixel (SK6805) on GPIO15
 * - Buzzer on GPIO16
 * - I2C on GPIO4 (SCL) / GPIO5 (SDA)
 * - MOSFET outputs on GPIO12, 13, 14
 * 
 * (c) 2025 Mycosoft
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "jsonio.h"
#include "cli.h"
#include "pixel.h"
#include "buzzer.h"
#include "modem_optical.h"
#include "modem_audio.h"
#include "peripherals.h"
#include "outputs.h"
#include "stimulus.h"

// ============================================================================
// TIMING
// ============================================================================

static uint32_t lastTelemetryTime = 0;
static const uint32_t TELEMETRY_INTERVAL_MS = 1000;  // 1 Hz telemetry in machine mode

// ============================================================================
// BOOT SEQUENCE
// ============================================================================

static void bootSequence() {
    // Visual boot indicator - quick color flash
    Pixel::setColor(0, 0, 255);  // Blue
    delay(100);
    Pixel::setColor(0, 255, 0);  // Green
    delay(100);
    Pixel::setColor(255, 255, 0);  // Yellow
    delay(100);
    Pixel::off();
    
    // Audio boot indicator
    Buzzer::playPattern(PATTERN_COIN);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize serial
    Serial.begin(SERIAL_BAUD);
    
    // Wait for USB CDC connection (if using USB)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime) < 3000) {
        delay(10);
    }
    
    // Initialize modules
    Pixel::init();
    Buzzer::init();
    OpticalModem::init();
    AcousticModem::init();
    Peripherals::init();
    Outputs::init();
    Stimulus::init();
    CLI::init();
    
    // Run boot sequence
    bootSequence();
    
    // Wait for boot sequence to finish
    while (Buzzer::isPatternPlaying()) {
        Buzzer::updatePattern();
        delay(10);
    }
    
    // Print banner (human mode default)
    CLI::printBanner();
    
    // Initial peripheral scan
    Peripherals::scan();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t now = millis();
    
    // Process CLI input
    CLI::update();
    
    // Update non-blocking modules
    Pixel::updatePattern();
    Buzzer::updatePattern();
    OpticalModem::update();
    AcousticModem::update();
    Stimulus::update();
    Peripherals::updateHotplug();
    
    // Emit telemetry in machine mode
    if (CLI::isMachineMode()) {
        if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryTime = now;
            
            // Build telemetry JSON
            JsonDoc doc;
            doc["type"] = "telemetry";
            doc["ts"] = now;
            
            // Board ID
            char boardId[20];
            uint64_t mac = ESP.getEfuseMac();
            snprintf(boardId, sizeof(boardId), "%012llX", mac);
            doc["board_id"] = boardId;
            
            // LED state
            PixelColor c = Pixel::getColor();
            JsonObject led = doc["led"].to<JsonObject>();
            led["r"] = c.r;
            led["g"] = c.g;
            led["b"] = c.b;
            
            // Modem states
            doc["optx_active"] = OpticalModem::isTransmitting();
            doc["aotx_active"] = AcousticModem::isTransmitting();
            doc["stim_light"] = Stimulus::isLightRunning();
            doc["stim_sound"] = Stimulus::isSoundRunning();
            
            // Peripheral count
            doc["peripherals"] = Peripherals::getCount();
            
            // Emit
            JsonIO::emit(doc);
        }
    }
    
    // Small delay to prevent tight loop
    delay(1);
}

