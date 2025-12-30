/**
 * MycoBrain Science Communications Firmware
 * Optical Modem Module (LiFi-ish TX)
 * 
 * Transmits data via NeoPixel blinking for camera/light-sensor receivers.
 * Supports multiple encoding profiles optimized for different receivers.
 */

#ifndef MODEM_OPTICAL_H
#define MODEM_OPTICAL_H

#include <Arduino.h>
#include "config.h"
#include "pixel.h"

// ============================================================================
// OPTICAL ENCODING PROFILES
// ============================================================================

enum OpticalProfile {
    OPTX_PROFILE_NONE = 0,
    OPTX_PROFILE_CAMERA_OOK,        // On-Off Keying for cameras (5-20 Hz)
    OPTX_PROFILE_CAMERA_MANCHESTER, // Manchester encoding for cameras
    OPTX_PROFILE_SPATIAL_SM,        // Spatial modulation (multi-LED arrays)
    OPTX_PROFILE_BEACON,            // Simple beacon pattern
    OPTX_PROFILE_MORSE              // Morse code
};

// ============================================================================
// OPTICAL TX CONFIGURATION
// ============================================================================

struct OpticalTxConfig {
    OpticalProfile profile;
    uint8_t rate_hz;            // Blink rate (symbols per second)
    uint8_t* payload;           // Binary payload
    size_t payload_len;
    bool repeat;                // Loop transmission
    PixelColor color_on;        // Color when "on"
    PixelColor color_off;       // Color when "off" (usually black)
    uint16_t crc16;             // CRC of payload
};

// ============================================================================
// PATTERN CONFIGURATION
// ============================================================================

struct OpticalPatternConfig {
    const char* pattern;        // "pulse", "sweep", "beacon", "morse"
    PixelColor color;
    uint32_t tempo_ms;
    const char* morse_text;     // For morse pattern
};

// ============================================================================
// OPTICAL MODEM INTERFACE
// ============================================================================

namespace OpticalModem {
    // Initialization
    void init();
    
    // Transmission control
    bool startTransmit(const OpticalTxConfig& config);
    bool startPattern(const OpticalPatternConfig& config);
    void stop();
    
    // State
    bool isTransmitting();
    OpticalProfile getCurrentProfile();
    uint32_t getBytesSent();
    uint32_t getBitsSent();
    
    // Non-blocking update (call in loop)
    void update();
    
    // Status (for JSON output)
    void getStatus(char* buffer, size_t bufSize);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

OpticalProfile opticalProfileFromName(const char* name);
const char* opticalProfileName(OpticalProfile profile);
uint16_t computeCRC16(const uint8_t* data, size_t len);

#endif // MODEM_OPTICAL_H

