/**
 * MycoBrain Science Communications Firmware
 * Acoustic Modem Implementation (FSK TX - "gibberlink/ggwave vibe")
 * 
 * Transmits data via buzzer tones for microphone receivers.
 */

#include "modem_audio.h"
#include "buzzer.h"
#include "jsonio.h"  // For CRC16

// ============================================================================
// STATE
// ============================================================================

static bool transmitting = false;
static AcousticTxConfig txConfig;
static uint32_t symbolsSent = 0;
static uint32_t bytesSent = 0;
static uint32_t lastSymbolTime = 0;
static uint8_t currentBit = 0;
static uint8_t currentByte = 0;
static bool preambleSending = true;
static uint8_t preambleCount = 0;

// Pattern mode
static bool patternMode = false;
static AcousticPatternConfig patternConfig;
static uint32_t patternStartTime = 0;
static uint16_t patternCurrentFreq = 0;

// ============================================================================
// INITIALIZATION
// ============================================================================

void AcousticModem::init() {
    transmitting = false;
    patternMode = false;
}

// ============================================================================
// TRANSMISSION CONTROL
// ============================================================================

bool AcousticModem::startTransmit(const AcousticTxConfig& config) {
    if (transmitting) stop();
    
    // Validate
    if (config.payload_len == 0 || config.payload == nullptr) {
        return false;
    }
    if (config.f0 == 0 || config.f1 == 0) {
        return false;
    }
    
    // Copy config
    txConfig = config;
    txConfig.crc16 = JsonIO::crc16(config.payload, config.payload_len);
    
    // Reset state
    transmitting = true;
    patternMode = false;
    symbolsSent = 0;
    bytesSent = 0;
    currentBit = 0;
    currentByte = 0;
    preambleSending = true;
    preambleCount = 0;
    lastSymbolTime = millis();
    
    return true;
}

bool AcousticModem::startPattern(const AcousticPatternConfig& config) {
    if (transmitting) stop();
    
    patternConfig = config;
    patternMode = true;
    transmitting = true;
    patternStartTime = millis();
    patternCurrentFreq = config.from_hz;
    lastSymbolTime = millis();
    
    // Start first tone
    Buzzer::tone(config.from_hz, 0);
    
    return true;
}

void AcousticModem::stop() {
    transmitting = false;
    patternMode = false;
    Buzzer::stop();
}

// ============================================================================
// STATE ACCESS
// ============================================================================

bool AcousticModem::isTransmitting() {
    return transmitting;
}

AcousticProfile AcousticModem::getCurrentProfile() {
    return transmitting ? txConfig.profile : AOTX_PROFILE_NONE;
}

uint32_t AcousticModem::getSymbolsSent() {
    return symbolsSent;
}

uint32_t AcousticModem::getBytesSent() {
    return bytesSent;
}

// ============================================================================
// FSK TRANSMISSION
// ============================================================================

static void transmitFSK() {
    bool bitValue;
    
    if (preambleSending) {
        // Preamble: alternating tones
        bitValue = (preambleCount % 2) == 0;
        preambleCount++;
        if (preambleCount >= ACOUSTIC_PREAMBLE_SYMBOLS) {
            preambleSending = false;
        }
    } else if (currentByte < txConfig.payload_len) {
        // Data bits
        bitValue = (txConfig.payload[currentByte] >> (7 - currentBit)) & 1;
        currentBit++;
        if (currentBit >= 8) {
            currentBit = 0;
            currentByte++;
            bytesSent++;
        }
        symbolsSent++;
    } else if (currentByte < txConfig.payload_len + 2) {
        // CRC bits
        uint8_t crcByte = (currentByte == txConfig.payload_len) ?
            (txConfig.crc16 >> 8) : (txConfig.crc16 & 0xFF);
        bitValue = (crcByte >> (7 - currentBit)) & 1;
        currentBit++;
        if (currentBit >= 8) {
            currentBit = 0;
            currentByte++;
        }
        symbolsSent++;
    } else {
        // End of transmission
        if (txConfig.repeat) {
            currentByte = 0;
            currentBit = 0;
            preambleSending = true;
            preambleCount = 0;
        } else {
            AcousticModem::stop();
            return;
        }
    }
    
    // Output FSK tone
    uint16_t freq = bitValue ? txConfig.f1 : txConfig.f0;
    Buzzer::tone(freq, 0);  // Continuous, we handle timing
}

// ============================================================================
// UPDATE (non-blocking)
// ============================================================================

void AcousticModem::update() {
    if (!transmitting) return;
    
    uint32_t now = millis();
    
    if (patternMode) {
        // Pattern mode
        uint32_t elapsed = now - patternStartTime;
        
        if (elapsed >= patternConfig.duration_ms) {
            stop();
            return;
        }
        
        if (strcmp(patternConfig.pattern, "sweep") == 0) {
            // Linear frequency sweep
            float progress = (float)elapsed / patternConfig.duration_ms;
            uint16_t freq = patternConfig.from_hz + 
                (uint16_t)((patternConfig.to_hz - patternConfig.from_hz) * progress);
            if (freq != patternCurrentFreq) {
                patternCurrentFreq = freq;
                Buzzer::tone(freq, 0);
            }
        } else if (strcmp(patternConfig.pattern, "chirp") == 0) {
            // Exponential chirp
            float progress = (float)elapsed / patternConfig.duration_ms;
            float logFrom = log((float)patternConfig.from_hz);
            float logTo = log((float)patternConfig.to_hz);
            uint16_t freq = (uint16_t)exp(logFrom + (logTo - logFrom) * progress);
            if (freq != patternCurrentFreq) {
                patternCurrentFreq = freq;
                Buzzer::tone(freq, 0);
            }
        } else if (strcmp(patternConfig.pattern, "pulse_train") == 0) {
            // On-off pulses
            uint32_t pulseLen = patternConfig.duration_ms / 20;
            bool isOn = (elapsed / pulseLen) % 2 == 0;
            if (isOn && patternCurrentFreq == 0) {
                patternCurrentFreq = patternConfig.from_hz;
                Buzzer::tone(patternConfig.from_hz, 0);
            } else if (!isOn && patternCurrentFreq != 0) {
                patternCurrentFreq = 0;
                Buzzer::stop();
            }
        }
    } else {
        // Data transmission mode
        if (now - lastSymbolTime < txConfig.symbol_ms) return;
        lastSymbolTime = now;
        
        transmitFSK();
    }
}

// ============================================================================
// STATUS
// ============================================================================

void AcousticModem::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"transmitting\":%s,\"profile\":\"%s\",\"symbols_sent\":%lu,\"bytes_sent\":%lu,\"f0\":%d,\"f1\":%d}",
        transmitting ? "true" : "false",
        acousticProfileName(getCurrentProfile()),
        symbolsSent,
        bytesSent,
        transmitting ? txConfig.f0 : 0,
        transmitting ? txConfig.f1 : 0
    );
}

// ============================================================================
// PROFILE NAME LOOKUP
// ============================================================================

AcousticProfile acousticProfileFromName(const char* name) {
    if (!name) return AOTX_PROFILE_NONE;
    if (strcasecmp(name, "simple_fsk") == 0) return AOTX_PROFILE_SIMPLE_FSK;
    if (strcasecmp(name, "ggwave_like") == 0) return AOTX_PROFILE_GGWAVE_LIKE;
    if (strcasecmp(name, "morse") == 0) return AOTX_PROFILE_MORSE;
    if (strcasecmp(name, "dtmf") == 0) return AOTX_PROFILE_DTMF;
    return AOTX_PROFILE_NONE;
}

const char* acousticProfileName(AcousticProfile profile) {
    switch (profile) {
        case AOTX_PROFILE_SIMPLE_FSK: return "simple_fsk";
        case AOTX_PROFILE_GGWAVE_LIKE: return "ggwave_like";
        case AOTX_PROFILE_MORSE: return "morse";
        case AOTX_PROFILE_DTMF: return "dtmf";
        default: return "none";
    }
}

