/**
 * MycoBrain Science Communications Firmware
 * Buzzer Module Implementation
 * 
 * Uses LEDC for tone generation on GPIO16.
 */

#include "buzzer.h"

// ============================================================================
// STATE
// ============================================================================

static bool initialized = false;
static uint16_t currentFreq = 0;
static uint32_t toneEndTime = 0;
static BuzzerPattern activePattern = PATTERN_NONE;
static uint8_t patternStep = 0;
static uint32_t patternStepTime = 0;

// ============================================================================
// PATTERN DATA
// ============================================================================

struct ToneStep {
    uint16_t freq;
    uint16_t duration;
};

// Mario coin sound
static const ToneStep coinPattern[] = {
    {988, 100}, {1319, 400}, {0, 0}
};

// Mario bump
static const ToneStep bumpPattern[] = {
    {262, 50}, {196, 100}, {0, 0}
};

// Mario power-up
static const ToneStep powerPattern[] = {
    {523, 60}, {659, 60}, {784, 60}, {1047, 60}, {1319, 60}, {1568, 200}, {0, 0}
};

// Mario 1-UP
static const ToneStep oneUpPattern[] = {
    {1319, 100}, {1568, 100}, {2637, 100}, {2093, 100}, {2349, 100}, {3136, 300}, {0, 0}
};

// Morgio jingle (custom)
static const ToneStep morgioPattern[] = {
    {523, 150}, {659, 150}, {784, 150}, {1047, 300},
    {784, 150}, {659, 150}, {523, 300},
    {587, 150}, {698, 150}, {880, 150}, {1175, 400}, {0, 0}
};

// Alert
static const ToneStep alertPattern[] = {
    {2000, 100}, {0, 50}, {2000, 100}, {0, 50}, {2000, 100}, {0, 0}
};

// Warning
static const ToneStep warningPattern[] = {
    {800, 200}, {0, 100}, {800, 200}, {0, 0}
};

// Success
static const ToneStep successPattern[] = {
    {523, 100}, {659, 100}, {784, 100}, {1047, 200}, {0, 0}
};

// Error
static const ToneStep errorPattern[] = {
    {200, 200}, {0, 100}, {150, 300}, {0, 0}
};

static const ToneStep* patternData[] = {
    nullptr,        // PATTERN_NONE
    coinPattern,
    bumpPattern,
    powerPattern,
    oneUpPattern,
    morgioPattern,
    alertPattern,
    warningPattern,
    successPattern,
    errorPattern
};

// ============================================================================
// INITIALIZATION
// ============================================================================

void Buzzer::init() {
    // Configure LEDC for buzzer
    ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_DEFAULT_FREQ, BUZZER_PWM_RESOLUTION);
    ledcAttachPin(PIN_BUZZER, BUZZER_PWM_CHANNEL);
    ledcWrite(BUZZER_PWM_CHANNEL, 0);  // Start silent
    initialized = true;
}

// ============================================================================
// BASIC TONE CONTROL
// ============================================================================

void Buzzer::tone(uint16_t frequency, uint16_t duration_ms) {
    if (!initialized) return;
    
    if (frequency == 0) {
        stop();
        return;
    }
    
    currentFreq = frequency;
    ledcWriteTone(BUZZER_PWM_CHANNEL, frequency);
    ledcWrite(BUZZER_PWM_CHANNEL, 128);  // 50% duty cycle
    
    if (duration_ms > 0) {
        toneEndTime = millis() + duration_ms;
    } else {
        toneEndTime = 0;  // Continuous
    }
}

void Buzzer::stop() {
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
    currentFreq = 0;
    toneEndTime = 0;
    activePattern = PATTERN_NONE;
}

// ============================================================================
// PATTERN PLAYBACK
// ============================================================================

void Buzzer::playPattern(BuzzerPattern pattern) {
    if (pattern == PATTERN_NONE || pattern > PATTERN_ERROR) {
        stop();
        return;
    }
    
    activePattern = pattern;
    patternStep = 0;
    patternStepTime = millis();
    
    // Start first step
    const ToneStep* steps = patternData[pattern];
    if (steps && steps[0].freq > 0) {
        tone(steps[0].freq, 0);  // Don't auto-stop, pattern engine handles it
    }
}

void Buzzer::playPattern(const char* patternName) {
    playPattern(buzzerPatternFromName(patternName));
}

void Buzzer::stopPattern() {
    activePattern = PATTERN_NONE;
    stop();
}

bool Buzzer::isPatternPlaying() {
    return activePattern != PATTERN_NONE;
}

void Buzzer::updatePattern() {
    // Handle single tone timeout
    if (toneEndTime > 0 && millis() >= toneEndTime) {
        stop();
        return;
    }
    
    // Handle pattern playback
    if (activePattern == PATTERN_NONE) return;
    
    const ToneStep* steps = patternData[activePattern];
    if (!steps) {
        activePattern = PATTERN_NONE;
        return;
    }
    
    // Check if current step is done
    const ToneStep& step = steps[patternStep];
    if (step.freq == 0 && step.duration == 0) {
        // End of pattern
        stop();
        return;
    }
    
    if (millis() - patternStepTime >= step.duration) {
        // Move to next step
        patternStep++;
        patternStepTime = millis();
        
        const ToneStep& nextStep = steps[patternStep];
        if (nextStep.freq == 0 && nextStep.duration == 0) {
            // End of pattern
            stop();
        } else if (nextStep.freq == 0) {
            // Silent pause
            ledcWrite(BUZZER_PWM_CHANNEL, 0);
            currentFreq = 0;
        } else {
            // Play next tone
            ledcWriteTone(BUZZER_PWM_CHANNEL, nextStep.freq);
            ledcWrite(BUZZER_PWM_CHANNEL, 128);
            currentFreq = nextStep.freq;
        }
    }
}

// ============================================================================
// STATE
// ============================================================================

bool Buzzer::isBusy() {
    return currentFreq > 0 || activePattern != PATTERN_NONE;
}

uint16_t Buzzer::getCurrentFrequency() {
    return currentFreq;
}

// ============================================================================
// STATUS
// ============================================================================

void Buzzer::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"frequency\":%d,\"pattern\":%s%s%s,\"busy\":%s}",
        currentFreq,
        activePattern != PATTERN_NONE ? "\"" : "",
        activePattern != PATTERN_NONE ? buzzerPatternName(activePattern) : "null",
        activePattern != PATTERN_NONE ? "\"" : "",
        isBusy() ? "true" : "false"
    );
}

// ============================================================================
// PATTERN NAME LOOKUP
// ============================================================================

BuzzerPattern buzzerPatternFromName(const char* name) {
    if (!name) return PATTERN_NONE;
    if (strcasecmp(name, "coin") == 0) return PATTERN_COIN;
    if (strcasecmp(name, "bump") == 0) return PATTERN_BUMP;
    if (strcasecmp(name, "power") == 0) return PATTERN_POWER;
    if (strcasecmp(name, "1up") == 0) return PATTERN_1UP;
    if (strcasecmp(name, "morgio") == 0) return PATTERN_MORGIO;
    if (strcasecmp(name, "alert") == 0) return PATTERN_ALERT;
    if (strcasecmp(name, "warning") == 0) return PATTERN_WARNING;
    if (strcasecmp(name, "success") == 0) return PATTERN_SUCCESS;
    if (strcasecmp(name, "error") == 0) return PATTERN_ERROR;
    return PATTERN_NONE;
}

const char* buzzerPatternName(BuzzerPattern pattern) {
    switch (pattern) {
        case PATTERN_COIN: return "coin";
        case PATTERN_BUMP: return "bump";
        case PATTERN_POWER: return "power";
        case PATTERN_1UP: return "1up";
        case PATTERN_MORGIO: return "morgio";
        case PATTERN_ALERT: return "alert";
        case PATTERN_WARNING: return "warning";
        case PATTERN_SUCCESS: return "success";
        case PATTERN_ERROR: return "error";
        default: return nullptr;
    }
}

