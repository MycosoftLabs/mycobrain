/**
 * MycoBrain Science Communications Firmware
 * Stimulus Engine Module
 * 
 * Separate from modem mode - generates repeatable light/sound patterns
 * for experiments with organisms (fungi, bacteria, plants, etc.)
 */

#ifndef STIMULUS_H
#define STIMULUS_H

#include <Arduino.h>
#include "config.h"
#include "pixel.h"

// ============================================================================
// STIMULUS TYPE
// ============================================================================

enum StimulusType {
    STIM_NONE = 0,
    STIM_LIGHT,     // NeoPixel-based
    STIM_SOUND,     // Buzzer-based
    STIM_COMBINED   // Both synchronized
};

// ============================================================================
// STIMULUS CONFIGURATION
// ============================================================================

struct LightStimulusConfig {
    const char* pattern;        // "pulse", "flash", "ramp", "strobe"
    PixelColor color;
    uint32_t on_ms;
    uint32_t off_ms;
    uint32_t ramp_ms;
    uint32_t cycles;            // 0 = infinite
};

struct SoundStimulusConfig {
    const char* pattern;        // "tone", "chirp", "pulse", "sweep"
    uint16_t freq_hz;
    uint16_t freq_end_hz;       // For sweep
    uint32_t on_ms;
    uint32_t off_ms;
    uint32_t cycles;            // 0 = infinite
};

// ============================================================================
// STIMULUS MODULE INTERFACE
// ============================================================================

namespace Stimulus {
    // Initialization
    void init();
    
    // Light stimuli
    bool startLight(const LightStimulusConfig& config);
    void stopLight();
    bool isLightRunning();
    
    // Sound stimuli
    bool startSound(const SoundStimulusConfig& config);
    void stopSound();
    bool isSoundRunning();
    
    // Combined
    void stopAll();
    
    // Non-blocking update (call in loop)
    void update();
    
    // Status
    void getStatus(char* buffer, size_t bufSize);
}

#endif // STIMULUS_H

