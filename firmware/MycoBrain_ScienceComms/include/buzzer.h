/**
 * MycoBrain Science Communications Firmware
 * Buzzer Module
 * 
 * Controls the MOSFET-driven buzzer on GPIO16.
 * Supports tones, patterns, and non-blocking playback.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// PATTERN DEFINITIONS
// ============================================================================

// Named patterns for compatibility with existing CLI
enum BuzzerPattern {
    PATTERN_NONE = 0,
    PATTERN_COIN,
    PATTERN_BUMP,
    PATTERN_POWER,
    PATTERN_1UP,
    PATTERN_MORGIO,
    PATTERN_ALERT,
    PATTERN_WARNING,
    PATTERN_SUCCESS,
    PATTERN_ERROR
};

// ============================================================================
// BUZZER MODULE INTERFACE
// ============================================================================

namespace Buzzer {
    // Initialization
    void init();
    
    // Basic tone control
    void tone(uint16_t frequency, uint16_t duration_ms);
    void stop();
    
    // Pattern playback (non-blocking)
    void playPattern(BuzzerPattern pattern);
    void playPattern(const char* patternName);
    void stopPattern();
    bool isPatternPlaying();
    void updatePattern();  // Call in loop()
    
    // State
    bool isBusy();
    uint16_t getCurrentFrequency();
    
    // Status (for JSON output)
    void getStatus(char* buffer, size_t bufSize);
}

// ============================================================================
// PATTERN NAME LOOKUP
// ============================================================================

BuzzerPattern buzzerPatternFromName(const char* name);
const char* buzzerPatternName(BuzzerPattern pattern);

#endif // BUZZER_H

