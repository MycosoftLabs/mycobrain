/**
 * MycoBrain Science Communications Firmware
 * Stimulus Engine Implementation
 * 
 * Generates repeatable light/sound patterns for experiments.
 */

#include "stimulus.h"
#include "pixel.h"
#include "buzzer.h"

// ============================================================================
// STATE
// ============================================================================

static bool lightRunning = false;
static LightStimulusConfig lightConfig;
static uint32_t lightStartTime = 0;
static uint32_t lightCycleCount = 0;
static uint32_t lightPhaseTime = 0;
static bool lightPhaseOn = false;

static bool soundRunning = false;
static SoundStimulusConfig soundConfig;
static uint32_t soundStartTime = 0;
static uint32_t soundCycleCount = 0;
static uint32_t soundPhaseTime = 0;
static bool soundPhaseOn = false;

// ============================================================================
// INITIALIZATION
// ============================================================================

void Stimulus::init() {
    lightRunning = false;
    soundRunning = false;
}

// ============================================================================
// LIGHT STIMULI
// ============================================================================

bool Stimulus::startLight(const LightStimulusConfig& config) {
    if (!config.pattern) return false;
    
    lightConfig = config;
    lightStartTime = millis();
    lightCycleCount = 0;
    lightPhaseTime = millis();
    lightPhaseOn = true;
    lightRunning = true;
    
    // Start with light on
    Pixel::setColor(config.color);
    
    return true;
}

void Stimulus::stopLight() {
    lightRunning = false;
    Pixel::off();
}

bool Stimulus::isLightRunning() {
    return lightRunning;
}

// ============================================================================
// SOUND STIMULI
// ============================================================================

bool Stimulus::startSound(const SoundStimulusConfig& config) {
    if (!config.pattern) return false;
    
    soundConfig = config;
    soundStartTime = millis();
    soundCycleCount = 0;
    soundPhaseTime = millis();
    soundPhaseOn = true;
    soundRunning = true;
    
    // Start with sound on
    Buzzer::tone(config.freq_hz, 0);
    
    return true;
}

void Stimulus::stopSound() {
    soundRunning = false;
    Buzzer::stop();
}

bool Stimulus::isSoundRunning() {
    return soundRunning;
}

// ============================================================================
// COMBINED CONTROL
// ============================================================================

void Stimulus::stopAll() {
    stopLight();
    stopSound();
}

// ============================================================================
// UPDATE (non-blocking)
// ============================================================================

void Stimulus::update() {
    uint32_t now = millis();
    
    // Update light stimulus
    if (lightRunning) {
        uint32_t elapsed = now - lightPhaseTime;
        
        if (strcmp(lightConfig.pattern, "pulse") == 0 || 
            strcmp(lightConfig.pattern, "flash") == 0) {
            // Simple on/off pattern
            if (lightPhaseOn && elapsed >= lightConfig.on_ms) {
                lightPhaseOn = false;
                lightPhaseTime = now;
                Pixel::off();
            } else if (!lightPhaseOn && elapsed >= lightConfig.off_ms) {
                lightPhaseOn = true;
                lightPhaseTime = now;
                lightCycleCount++;
                
                if (lightConfig.cycles > 0 && lightCycleCount >= lightConfig.cycles) {
                    stopLight();
                } else {
                    Pixel::setColor(lightConfig.color);
                }
            }
        } else if (strcmp(lightConfig.pattern, "ramp") == 0) {
            // Brightness ramp
            uint32_t cycleTime = lightConfig.ramp_ms * 2;  // Up + down
            uint32_t cycleElapsed = (now - lightStartTime) % cycleTime;
            
            float brightness;
            if (cycleElapsed < lightConfig.ramp_ms) {
                brightness = (float)cycleElapsed / lightConfig.ramp_ms;
            } else {
                brightness = 1.0f - (float)(cycleElapsed - lightConfig.ramp_ms) / lightConfig.ramp_ms;
            }
            
            Pixel::setBrightness((uint8_t)(brightness * 255));
            Pixel::setColor(lightConfig.color);
            
            // Check cycle count
            uint32_t totalCycles = (now - lightStartTime) / cycleTime;
            if (lightConfig.cycles > 0 && totalCycles >= lightConfig.cycles) {
                stopLight();
            }
        } else if (strcmp(lightConfig.pattern, "strobe") == 0) {
            // Fast strobe (10ms on/off)
            uint32_t strobeTime = 10;
            if (elapsed >= strobeTime) {
                lightPhaseOn = !lightPhaseOn;
                lightPhaseTime = now;
                if (lightPhaseOn) {
                    Pixel::setColor(lightConfig.color);
                } else {
                    Pixel::off();
                }
            }
        }
    }
    
    // Update sound stimulus
    if (soundRunning) {
        uint32_t elapsed = now - soundPhaseTime;
        
        if (strcmp(soundConfig.pattern, "tone") == 0 || 
            strcmp(soundConfig.pattern, "pulse") == 0) {
            // Simple on/off pattern
            if (soundPhaseOn && elapsed >= soundConfig.on_ms) {
                soundPhaseOn = false;
                soundPhaseTime = now;
                Buzzer::stop();
            } else if (!soundPhaseOn && elapsed >= soundConfig.off_ms) {
                soundPhaseOn = true;
                soundPhaseTime = now;
                soundCycleCount++;
                
                if (soundConfig.cycles > 0 && soundCycleCount >= soundConfig.cycles) {
                    stopSound();
                } else {
                    Buzzer::tone(soundConfig.freq_hz, 0);
                }
            }
        } else if (strcmp(soundConfig.pattern, "sweep") == 0) {
            // Frequency sweep
            uint32_t sweepElapsed = now - soundStartTime;
            float progress = (float)(sweepElapsed % soundConfig.on_ms) / soundConfig.on_ms;
            
            uint16_t freq = soundConfig.freq_hz + 
                (uint16_t)((soundConfig.freq_end_hz - soundConfig.freq_hz) * progress);
            Buzzer::tone(freq, 0);
            
            // Check cycle count
            uint32_t totalCycles = sweepElapsed / soundConfig.on_ms;
            if (soundConfig.cycles > 0 && totalCycles >= soundConfig.cycles) {
                stopSound();
            }
        } else if (strcmp(soundConfig.pattern, "chirp") == 0) {
            // Exponential chirp
            uint32_t chirpElapsed = now - soundStartTime;
            float progress = (float)(chirpElapsed % soundConfig.on_ms) / soundConfig.on_ms;
            
            float logFrom = log((float)soundConfig.freq_hz);
            float logTo = log((float)soundConfig.freq_end_hz);
            uint16_t freq = (uint16_t)exp(logFrom + (logTo - logFrom) * progress);
            Buzzer::tone(freq, 0);
            
            uint32_t totalCycles = chirpElapsed / soundConfig.on_ms;
            if (soundConfig.cycles > 0 && totalCycles >= soundConfig.cycles) {
                stopSound();
            }
        }
    }
}

// ============================================================================
// STATUS
// ============================================================================

void Stimulus::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"light\":{\"running\":%s,\"pattern\":\"%s\",\"cycles\":%lu},"
        "\"sound\":{\"running\":%s,\"pattern\":\"%s\",\"cycles\":%lu}}",
        lightRunning ? "true" : "false",
        lightRunning ? lightConfig.pattern : "none",
        lightCycleCount,
        soundRunning ? "true" : "false",
        soundRunning ? soundConfig.pattern : "none",
        soundCycleCount
    );
}

