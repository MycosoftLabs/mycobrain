/**
 * MycoBrain Side A Firmware
 * NeoPixel Module Implementation
 * 
 * Uses NeoPixelBus with RMT driver for ESP32-S3.
 */

#include "pixel.h"
#include <NeoPixelBus.h>

// ============================================================================
// NEOPIXELBUS INSTANCE
// ============================================================================

// Use GRB color order (common for WS2812/SK6805)
// RMT channel 0 for ESP32-S3
static NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip(NEOPIXEL_COUNT, PIN_NEOPIXEL);

// ============================================================================
// STATE
// ============================================================================

static PixelColor currentColor = {0, 0, 0};
static uint8_t brightness = NEOPIXEL_BRIGHTNESS;
static bool patternRunning = false;
static const char* currentPattern = nullptr;
static uint32_t patternTempo = 500;
static uint32_t patternLastUpdate = 0;
static uint8_t patternStep = 0;

// ============================================================================
// PATTERN DEFINITIONS
// ============================================================================

static const PixelColor rainbowColors[] = {
    {255, 0, 0},     // Red
    {255, 127, 0},   // Orange
    {255, 255, 0},   // Yellow
    {0, 255, 0},     // Green
    {0, 0, 255},     // Blue
    {75, 0, 130},    // Indigo
    {148, 0, 211}    // Violet
};
static const int rainbowColorCount = sizeof(rainbowColors) / sizeof(rainbowColors[0]);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static RgbColor applyBrightness(uint8_t r, uint8_t g, uint8_t b) {
    float scale = brightness / 255.0f;
    return RgbColor((uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void Pixel::init() {
    strip.Begin();
    strip.ClearTo(RgbColor(0, 0, 0));
    strip.Show();
}

// ============================================================================
// BASIC CONTROL
// ============================================================================

void Pixel::setColor(uint8_t r, uint8_t g, uint8_t b) {
    currentColor.r = r;
    currentColor.g = g;
    currentColor.b = b;
    strip.SetPixelColor(0, applyBrightness(r, g, b));
    strip.Show();
}

void Pixel::setColor(const PixelColor& color) {
    setColor(color.r, color.g, color.b);
}

void Pixel::off() {
    setColor(0, 0, 0);
    patternRunning = false;
}

void Pixel::show() {
    strip.Show();
}

// ============================================================================
// STATE ACCESS
// ============================================================================

PixelColor Pixel::getColor() {
    return currentColor;
}

uint8_t Pixel::getBrightness() {
    return brightness;
}

void Pixel::setBrightness(uint8_t b) {
    brightness = b;
    strip.SetPixelColor(0, applyBrightness(currentColor.r, currentColor.g, currentColor.b));
    strip.Show();
}

// ============================================================================
// PATTERN ENGINE
// ============================================================================

void Pixel::startPattern(const char* patternName, uint32_t tempo_ms) {
    currentPattern = patternName;
    patternTempo = tempo_ms;
    patternRunning = true;
    patternStep = 0;
    patternLastUpdate = millis();
}

void Pixel::stopPattern() {
    patternRunning = false;
    currentPattern = nullptr;
}

bool Pixel::isPatternRunning() {
    return patternRunning;
}

void Pixel::updatePattern() {
    if (!patternRunning || !currentPattern) return;
    
    uint32_t now = millis();
    if (now - patternLastUpdate < patternTempo) return;
    
    patternLastUpdate = now;
    
    // Handle different patterns
    if (strcmp(currentPattern, "rainbow") == 0) {
        const PixelColor& c = rainbowColors[patternStep % rainbowColorCount];
        setColor(c.r, c.g, c.b);
        patternStep++;
    }
    else if (strcmp(currentPattern, "pulse") == 0) {
        if (patternStep % 2 == 0) {
            setColor(currentColor.r, currentColor.g, currentColor.b);
        } else {
            strip.SetPixelColor(0, RgbColor(0, 0, 0));
            strip.Show();
        }
        patternStep++;
    }
    else if (strcmp(currentPattern, "sweep") == 0) {
        float hue = (patternStep % 360) / 360.0f;
        uint8_t r, g, b;
        int h = (int)(hue * 6);
        float f = hue * 6 - h;
        uint8_t v = 255;
        uint8_t p = 0;
        uint8_t q = (uint8_t)(255 * (1 - f));
        uint8_t t = (uint8_t)(255 * f);
        switch (h % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
            default: r = g = b = 0; break;
        }
        setColor(r, g, b);
        patternStep += 10;
    }
    else if (strcmp(currentPattern, "beacon") == 0) {
        if (patternStep % 10 == 0) {
            setColor(255, 255, 255);
        } else {
            strip.SetPixelColor(0, RgbColor(0, 0, 0));
            strip.Show();
        }
        patternStep++;
    }
    else if (strcmp(currentPattern, "status_ok") == 0) {
        // Slow green pulse for healthy status
        if (patternStep % 2 == 0) {
            setColor(0, 64, 0);
        } else {
            setColor(0, 16, 0);
        }
        patternStep++;
    }
    else if (strcmp(currentPattern, "status_warn") == 0) {
        // Yellow pulse for warning
        if (patternStep % 2 == 0) {
            setColor(64, 48, 0);
        } else {
            setColor(16, 12, 0);
        }
        patternStep++;
    }
    else if (strcmp(currentPattern, "status_error") == 0) {
        // Fast red blink for error
        if (patternStep % 2 == 0) {
            setColor(64, 0, 0);
        } else {
            setColor(0, 0, 0);
        }
        patternStep++;
    }
}

// ============================================================================
// STATUS
// ============================================================================

void Pixel::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"pattern\":%s%s%s,\"pattern_running\":%s}",
        currentColor.r, currentColor.g, currentColor.b,
        brightness,
        currentPattern ? "\"" : "",
        currentPattern ? currentPattern : "null",
        currentPattern ? "\"" : "",
        patternRunning ? "true" : "false"
    );
}
