/**
 * MycoBrain Side A Firmware
 * Buzzer Module
 * 
 * Controls MOSFET-driven buzzer for tones and patterns.
 * Non-blocking pattern playback with named patterns.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// PATTERN DEFINITIONS
// ============================================================================

enum BuzzerPattern {
    PATTERN_NONE = 0,
    PATTERN_COIN,       // Mario coin
    PATTERN_BUMP,       // Mario bump
    PATTERN_POWER,      // Mario power-up
    PATTERN_1UP,        // Mario 1-UP
    PATTERN_MORGIO,     // Mycosoft jingle
    PATTERN_ALERT,      // Alert beeps
    PATTERN_WARNING,    // Warning tone
    PATTERN_SUCCESS,    // Success chime
    PATTERN_ERROR       // Error sound
};

// ============================================================================
// BUZZER MODULE INTERFACE
// ============================================================================

namespace Buzzer {
    // Initialization
    void init();
    
    // Basic tone control
    void tone(uint16_t frequency, uint16_t duration_ms = 0);
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
