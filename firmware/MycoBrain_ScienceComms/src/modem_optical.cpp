/**
 * MycoBrain Science Communications Firmware
 * Optical Modem Implementation (LiFi-ish TX)
 * 
 * Transmits data via NeoPixel blinking for camera/light-sensor receivers.
 */

#include "modem_optical.h"
#include "pixel.h"

// ============================================================================
// STATE
// ============================================================================

static bool transmitting = false;
static OpticalTxConfig txConfig;
static uint32_t bytesSent = 0;
static uint32_t bitsSent = 0;
static uint32_t lastSymbolTime = 0;
static uint8_t currentBit = 0;
static uint8_t currentByte = 0;
static uint8_t manchesterPhase = 0;
static bool preambleSending = true;
static uint8_t preambleCount = 0;
static uint8_t* payloadBuffer = nullptr;

// Pattern mode
static bool patternMode = false;
static OpticalPatternConfig patternConfig;
static uint8_t patternStep = 0;

// ============================================================================
// CRC16 IMPLEMENTATION
// ============================================================================

uint16_t computeCRC16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void OpticalModem::init() {
    // Pixel module handles hardware init
    transmitting = false;
    patternMode = false;
}

// ============================================================================
// TRANSMISSION CONTROL
// ============================================================================

bool OpticalModem::startTransmit(const OpticalTxConfig& config) {
    if (transmitting) stop();
    
    // Validate config
    if (config.rate_hz == 0 || config.rate_hz > OPTICAL_MAX_RATE_HZ) {
        return false;
    }
    if (config.payload_len == 0 || config.payload == nullptr) {
        return false;
    }
    
    // Copy config
    txConfig = config;
    txConfig.crc16 = computeCRC16(config.payload, config.payload_len);
    
    // Reset state
    transmitting = true;
    patternMode = false;
    bytesSent = 0;
    bitsSent = 0;
    currentBit = 0;
    currentByte = 0;
    manchesterPhase = 0;
    preambleSending = true;
    preambleCount = 0;
    lastSymbolTime = millis();
    
    return true;
}

bool OpticalModem::startPattern(const OpticalPatternConfig& config) {
    if (transmitting) stop();
    
    patternConfig = config;
    patternMode = true;
    transmitting = true;
    patternStep = 0;
    lastSymbolTime = millis();
    
    return true;
}

void OpticalModem::stop() {
    transmitting = false;
    patternMode = false;
    Pixel::off();
}

// ============================================================================
// STATE ACCESS
// ============================================================================

bool OpticalModem::isTransmitting() {
    return transmitting;
}

OpticalProfile OpticalModem::getCurrentProfile() {
    return transmitting ? txConfig.profile : OPTX_PROFILE_NONE;
}

uint32_t OpticalModem::getBytesSent() {
    return bytesSent;
}

uint32_t OpticalModem::getBitsSent() {
    return bitsSent;
}

// ============================================================================
// TRANSMISSION UPDATE (non-blocking)
// ============================================================================

static void transmitOOK() {
    // Get current bit to transmit
    bool bitValue;
    
    if (preambleSending) {
        // Preamble: alternating 1-0-1-0...
        bitValue = (preambleCount % 2) == 0;
        preambleCount++;
        if (preambleCount >= OPTICAL_PREAMBLE_BITS * 2) {
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
        bitsSent++;
    } else if (currentByte < txConfig.payload_len + 2) {
        // CRC bits (2 bytes)
        uint8_t crcByte = (currentByte == txConfig.payload_len) ? 
            (txConfig.crc16 >> 8) : (txConfig.crc16 & 0xFF);
        bitValue = (crcByte >> (7 - currentBit)) & 1;
        currentBit++;
        if (currentBit >= 8) {
            currentBit = 0;
            currentByte++;
        }
        bitsSent++;
    } else {
        // End of transmission
        if (txConfig.repeat) {
            // Reset for repeat
            currentByte = 0;
            currentBit = 0;
            preambleSending = true;
            preambleCount = 0;
        } else {
            OpticalModem::stop();
            return;
        }
    }
    
    // Set LED state
    if (bitValue) {
        Pixel::setColor(txConfig.color_on);
    } else {
        Pixel::setColor(txConfig.color_off);
    }
}

static void transmitManchester() {
    // Manchester encoding: 0 = low-high, 1 = high-low
    // Each bit takes 2 phases
    
    bool bitValue;
    
    if (preambleSending) {
        bitValue = (preambleCount / 2) % 2 == 0;
        if (manchesterPhase == 0) {
            // First half of bit
        }
        manchesterPhase++;
        if (manchesterPhase >= 2) {
            manchesterPhase = 0;
            preambleCount++;
            if (preambleCount >= OPTICAL_PREAMBLE_BITS * 2) {
                preambleSending = false;
            }
        }
    } else if (currentByte < txConfig.payload_len + 2) {
        // Data + CRC
        uint8_t dataByte;
        if (currentByte < txConfig.payload_len) {
            dataByte = txConfig.payload[currentByte];
        } else {
            dataByte = (currentByte == txConfig.payload_len) ?
                (txConfig.crc16 >> 8) : (txConfig.crc16 & 0xFF);
        }
        
        bitValue = (dataByte >> (7 - currentBit)) & 1;
        
        manchesterPhase++;
        if (manchesterPhase >= 2) {
            manchesterPhase = 0;
            currentBit++;
            bitsSent++;
            if (currentBit >= 8) {
                currentBit = 0;
                currentByte++;
                if (currentByte <= txConfig.payload_len) bytesSent++;
            }
        }
    } else {
        if (txConfig.repeat) {
            currentByte = 0;
            currentBit = 0;
            manchesterPhase = 0;
            preambleSending = true;
            preambleCount = 0;
        } else {
            OpticalModem::stop();
            return;
        }
    }
    
    // Manchester encoding
    bool ledOn;
    if (bitValue) {
        ledOn = (manchesterPhase == 0);  // 1 = high-low
    } else {
        ledOn = (manchesterPhase == 1);  // 0 = low-high
    }
    
    if (ledOn) {
        Pixel::setColor(txConfig.color_on);
    } else {
        Pixel::setColor(txConfig.color_off);
    }
}

void OpticalModem::update() {
    if (!transmitting) return;
    
    uint32_t now = millis();
    uint32_t interval = 1000 / (patternMode ? 10 : txConfig.rate_hz);
    
    // For Manchester, we need 2x the symbol rate
    if (!patternMode && txConfig.profile == OPTX_PROFILE_CAMERA_MANCHESTER) {
        interval /= 2;
    }
    
    if (now - lastSymbolTime < interval) return;
    lastSymbolTime = now;
    
    if (patternMode) {
        // Pattern mode
        if (strcmp(patternConfig.pattern, "pulse") == 0) {
            if (patternStep % 2 == 0) {
                Pixel::setColor(patternConfig.color);
            } else {
                Pixel::off();
            }
            patternStep++;
        } else if (strcmp(patternConfig.pattern, "beacon") == 0) {
            if (patternStep % 10 == 0) {
                Pixel::setColor(255, 255, 255);
            } else {
                Pixel::off();
            }
            patternStep++;
        } else if (strcmp(patternConfig.pattern, "sweep") == 0) {
            // HSV sweep
            float hue = (patternStep % 360) / 360.0f;
            int h = (int)(hue * 6);
            float f = hue * 6 - h;
            uint8_t v = 255, p = 0;
            uint8_t q = (uint8_t)(255 * (1 - f));
            uint8_t t = (uint8_t)(255 * f);
            uint8_t r, g, b;
            switch (h % 6) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                case 5: r = v; g = p; b = q; break;
                default: r = g = b = 0;
            }
            Pixel::setColor(r, g, b);
            patternStep += 5;
        }
    } else {
        // Data transmission mode
        switch (txConfig.profile) {
            case OPTX_PROFILE_CAMERA_OOK:
            case OPTX_PROFILE_BEACON:
                transmitOOK();
                break;
            case OPTX_PROFILE_CAMERA_MANCHESTER:
                transmitManchester();
                break;
            default:
                transmitOOK();
                break;
        }
    }
}

// ============================================================================
// STATUS
// ============================================================================

void OpticalModem::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"transmitting\":%s,\"profile\":\"%s\",\"bytes_sent\":%lu,\"bits_sent\":%lu,\"rate_hz\":%d}",
        transmitting ? "true" : "false",
        opticalProfileName(getCurrentProfile()),
        bytesSent,
        bitsSent,
        transmitting ? txConfig.rate_hz : 0
    );
}

// ============================================================================
// PROFILE NAME LOOKUP
// ============================================================================

OpticalProfile opticalProfileFromName(const char* name) {
    if (!name) return OPTX_PROFILE_NONE;
    if (strcasecmp(name, "camera_ook") == 0) return OPTX_PROFILE_CAMERA_OOK;
    if (strcasecmp(name, "camera_manchester") == 0) return OPTX_PROFILE_CAMERA_MANCHESTER;
    if (strcasecmp(name, "spatial_sm") == 0) return OPTX_PROFILE_SPATIAL_SM;
    if (strcasecmp(name, "beacon") == 0) return OPTX_PROFILE_BEACON;
    if (strcasecmp(name, "morse") == 0) return OPTX_PROFILE_MORSE;
    return OPTX_PROFILE_NONE;
}

const char* opticalProfileName(OpticalProfile profile) {
    switch (profile) {
        case OPTX_PROFILE_CAMERA_OOK: return "camera_ook";
        case OPTX_PROFILE_CAMERA_MANCHESTER: return "camera_manchester";
        case OPTX_PROFILE_SPATIAL_SM: return "spatial_sm";
        case OPTX_PROFILE_BEACON: return "beacon";
        case OPTX_PROFILE_MORSE: return "morse";
        default: return "none";
    }
}

