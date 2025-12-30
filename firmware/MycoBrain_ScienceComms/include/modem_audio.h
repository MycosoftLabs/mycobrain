/**
 * MycoBrain Science Communications Firmware
 * Acoustic Modem Module (FSK TX - "gibberlink/ggwave vibe")
 * 
 * Transmits data via buzzer tones for microphone receivers.
 * Supports FSK encoding with preamble and CRC.
 */

#ifndef MODEM_AUDIO_H
#define MODEM_AUDIO_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// ACOUSTIC ENCODING PROFILES
// ============================================================================

enum AcousticProfile {
    AOTX_PROFILE_NONE = 0,
    AOTX_PROFILE_SIMPLE_FSK,    // 2-tone FSK with preamble + CRC
    AOTX_PROFILE_GGWAVE_LIKE,   // Multi-tone (future)
    AOTX_PROFILE_MORSE,         // Morse code via buzzer
    AOTX_PROFILE_DTMF           // DTMF-like tones (future)
};

// ============================================================================
// ACOUSTIC TX CONFIGURATION
// ============================================================================

struct AcousticTxConfig {
    AcousticProfile profile;
    uint16_t f0;                // Mark frequency (Hz)
    uint16_t f1;                // Space frequency (Hz)
    uint16_t symbol_ms;         // Symbol duration (ms)
    uint8_t* payload;           // Binary payload
    size_t payload_len;
    bool repeat;                // Loop transmission
    uint16_t crc16;             // CRC of payload
};

// ============================================================================
// PATTERN CONFIGURATION
// ============================================================================

struct AcousticPatternConfig {
    const char* pattern;        // "sweep", "chirp", "pulse_train", "morse"
    uint16_t from_hz;
    uint16_t to_hz;
    uint16_t duration_ms;
    const char* morse_text;     // For morse pattern
};

// ============================================================================
// ACOUSTIC MODEM INTERFACE
// ============================================================================

namespace AcousticModem {
    // Initialization
    void init();
    
    // Transmission control
    bool startTransmit(const AcousticTxConfig& config);
    bool startPattern(const AcousticPatternConfig& config);
    void stop();
    
    // State
    bool isTransmitting();
    AcousticProfile getCurrentProfile();
    uint32_t getSymbolsSent();
    uint32_t getBytesSent();
    
    // Non-blocking update (call in loop)
    void update();
    
    // Status (for JSON output)
    void getStatus(char* buffer, size_t bufSize);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

AcousticProfile acousticProfileFromName(const char* name);
const char* acousticProfileName(AcousticProfile profile);

#endif // MODEM_AUDIO_H

