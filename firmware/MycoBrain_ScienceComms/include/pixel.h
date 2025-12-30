/**
 * MycoBrain Science Communications Firmware
 * NeoPixel Module (NeoPixelBus-based)
 * 
 * Controls the onboard SK6805 addressable RGB LED on GPIO15.
 * Uses RMT-based driver for ESP32-S3 compatibility.
 */

#ifndef PIXEL_H
#define PIXEL_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// COLOR STRUCTURE
// ============================================================================

struct PixelColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
    PixelColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
        : r(red), g(green), b(blue) {}
};

// ============================================================================
// PIXEL MODULE INTERFACE
// ============================================================================

namespace Pixel {
    // Initialization
    void init();
    
    // Basic control
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setColor(const PixelColor& color);
    void off();
    void show();
    
    // State
    PixelColor getColor();
    uint8_t getBrightness();
    void setBrightness(uint8_t brightness);
    
    // Pattern engine (non-blocking)
    void startPattern(const char* patternName, uint32_t tempo_ms = 500);
    void stopPattern();
    bool isPatternRunning();
    void updatePattern();  // Call in loop()
    
    // Status (for JSON output)
    void getStatus(char* buffer, size_t bufSize);
}

#endif // PIXEL_H

