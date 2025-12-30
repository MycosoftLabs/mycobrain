/**
 * MycoBrain Science Communications Firmware
 * Digital Outputs Module Implementation
 * 
 * Controls MOSFET outputs on GPIO12, 13, 14.
 */

#include "outputs.h"

// ============================================================================
// CHANNEL DATA
// ============================================================================

static OutputChannel channels[3] = {
    {PIN_OUT_1, false, 0, 0, false},
    {PIN_OUT_2, false, 0, 0, false},
    {PIN_OUT_3, false, 0, 0, false}
};

// PWM channels for outputs
static const uint8_t pwmChannels[3] = {1, 2, 3};  // LEDC channels (0 is for buzzer)

// ============================================================================
// INITIALIZATION
// ============================================================================

void Outputs::init() {
    for (int i = 0; i < 3; i++) {
        pinMode(channels[i].pin, OUTPUT);
        digitalWrite(channels[i].pin, LOW);
    }
}

// ============================================================================
// DIGITAL CONTROL
// ============================================================================

void Outputs::set(uint8_t channel, bool state) {
    if (channel < 1 || channel > 3) return;
    
    OutputChannel& ch = channels[channel - 1];
    
    // Disable PWM if it was enabled
    if (ch.isPwmEnabled) {
        ledcDetachPin(ch.pin);
        ch.isPwmEnabled = false;
        pinMode(ch.pin, OUTPUT);
    }
    
    ch.state = state;
    digitalWrite(ch.pin, state ? HIGH : LOW);
}

bool Outputs::get(uint8_t channel) {
    if (channel < 1 || channel > 3) return false;
    return channels[channel - 1].state;
}

// ============================================================================
// PWM CONTROL
// ============================================================================

void Outputs::setPwm(uint8_t channel, uint8_t value, uint16_t freq) {
    if (channel < 1 || channel > 3) return;
    
    OutputChannel& ch = channels[channel - 1];
    uint8_t pwmCh = pwmChannels[channel - 1];
    
    if (!ch.isPwmEnabled) {
        ledcSetup(pwmCh, freq, 8);  // 8-bit resolution
        ledcAttachPin(ch.pin, pwmCh);
        ch.isPwmEnabled = true;
    } else if (ch.pwmFreq != freq) {
        ledcSetup(pwmCh, freq, 8);
    }
    
    ch.pwmValue = value;
    ch.pwmFreq = freq;
    ch.state = (value > 0);
    ledcWrite(pwmCh, value);
}

void Outputs::disablePwm(uint8_t channel) {
    if (channel < 1 || channel > 3) return;
    
    OutputChannel& ch = channels[channel - 1];
    
    if (ch.isPwmEnabled) {
        ledcDetachPin(ch.pin);
        ch.isPwmEnabled = false;
        pinMode(ch.pin, OUTPUT);
        digitalWrite(ch.pin, ch.state ? HIGH : LOW);
    }
}

// ============================================================================
// STATE ACCESS
// ============================================================================

OutputChannel* Outputs::getChannel(uint8_t channel) {
    if (channel < 1 || channel > 3) return nullptr;
    return &channels[channel - 1];
}

// ============================================================================
// STATUS
// ============================================================================

void Outputs::getStatus(char* buffer, size_t bufSize) {
    snprintf(buffer, bufSize,
        "{\"out1\":{\"state\":%s,\"pwm\":%d,\"freq\":%d},"
        "\"out2\":{\"state\":%s,\"pwm\":%d,\"freq\":%d},"
        "\"out3\":{\"state\":%s,\"pwm\":%d,\"freq\":%d}}",
        channels[0].state ? "true" : "false", channels[0].pwmValue, channels[0].pwmFreq,
        channels[1].state ? "true" : "false", channels[1].pwmValue, channels[1].pwmFreq,
        channels[2].state ? "true" : "false", channels[2].pwmValue, channels[2].pwmFreq
    );
}

