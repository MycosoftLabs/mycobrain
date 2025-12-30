/**
 * MycoBrain Science Communications Firmware
 * Digital Outputs Module
 * 
 * Controls MOSFET outputs on GPIO12, 13, 14.
 * These are NOT LEDs - they are external load drivers.
 */

#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// OUTPUT CHANNEL STRUCTURE
// ============================================================================

struct OutputChannel {
    uint8_t pin;
    bool state;
    uint8_t pwmValue;       // 0-255
    uint16_t pwmFreq;       // Hz (0 = digital only)
    bool isPwmEnabled;
};

// ============================================================================
// OUTPUTS MODULE INTERFACE
// ============================================================================

namespace Outputs {
    // Initialization
    void init();
    
    // Digital control
    void set(uint8_t channel, bool state);      // channel: 1, 2, or 3
    bool get(uint8_t channel);
    
    // PWM control
    void setPwm(uint8_t channel, uint8_t value, uint16_t freq = 1000);
    void disablePwm(uint8_t channel);
    
    // State
    OutputChannel* getChannel(uint8_t channel);
    
    // Status (for JSON output)
    void getStatus(char* buffer, size_t bufSize);
}

#endif // OUTPUTS_H

