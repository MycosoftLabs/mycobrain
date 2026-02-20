/**
 * SporeBase Controller implementation.
 * Fan PWM, tape stepper, and 15–60 min sampling cycle.
 * Created: February 12, 2026
 */

#include "sporebase_controller.h"

// ESP32 LEDC for fan PWM (if not defined in board)
#ifndef LEDC_CHANNEL_FAN
#define LEDC_CHANNEL_FAN  2
#endif
#ifndef LEDC_RESOLUTION
#define LEDC_RESOLUTION   8
#endif
#ifndef LEDC_FREQ
#define LEDC_FREQ         25000
#endif

void SporeBaseController::initFan(uint8_t pwmPin, uint8_t tachPin) {
    fanPwmPin_ = pwmPin;
    fanTachPin_ = tachPin;
    if (pwmPin) {
        pinMode(pwmPin, OUTPUT);
#if defined(ESP32)
        ledcSetup(LEDC_CHANNEL_FAN, LEDC_FREQ, LEDC_RESOLUTION);
        ledcAttachPin(pwmPin, LEDC_CHANNEL_FAN);
        ledcWrite(LEDC_CHANNEL_FAN, 0);
#endif
    }
    if (tachPin) {
        pinMode(tachPin, INPUT_PULLUP);
    }
}

void SporeBaseController::initTapeMotor(uint8_t stepPin, uint8_t dirPin) {
    stepPin_ = stepPin;
    dirPin_ = dirPin;
    if (stepPin) pinMode(stepPin, OUTPUT);
    if (dirPin) pinMode(dirPin, OUTPUT);
}

void SporeBaseController::setFlowRate(float litersPerMinute) {
    flowRateLpm_ = litersPerMinute;
    if (flowRateLpm_ < 0.0f) flowRateLpm_ = 0.0f;
    if (flowRateLpm_ > 15.0f) flowRateLpm_ = 15.0f;
    // Map ~0–15 L/min to PWM 0–255 (linear approximation)
    if (fanPwmPin_) {
        uint8_t duty = (uint8_t)((flowRateLpm_ / 15.0f) * 255.0f);
#if defined(ESP32)
        ledcWrite(LEDC_CHANNEL_FAN, duty);
#else
        analogWrite(fanPwmPin_, duty);
#endif
    }
}

void SporeBaseController::advanceTape(uint16_t steps) {
    if (!stepPin_ || !dirPin_) return;
    digitalWrite(dirPin_, HIGH);
    for (uint16_t i = 0; i < steps; i++) {
        digitalWrite(stepPin_, HIGH);
        delayMicroseconds(200);
        digitalWrite(stepPin_, LOW);
        delayMicroseconds(200);
    }
}

void SporeBaseController::startSamplingCycle(uint16_t intervalMinutes) {
    if (intervalMinutes < 15) intervalMinutes = 15;
    if (intervalMinutes > 60) intervalMinutes = 60;
    cycleIntervalMinutes_ = intervalMinutes;
    samplingActive_ = true;
    segmentStartTime_ = millis() / 1000;
    lastCycleCheck_ = millis();
    setFlowRate(flowRateLpm_);
}

void SporeBaseController::stopSamplingCycle() {
    samplingActive_ = false;
    setFlowRate(0.0f);
}

float SporeBaseController::getSporeCount() {
    return sporeCount_;
}

void SporeBaseController::tick() {
    if (!samplingActive_) return;
    uint32_t now = millis();
    uint32_t elapsedMs = now - lastCycleCheck_;
    lastCycleCheck_ = now;
    uint32_t segmentElapsedSec = (now / 1000) - segmentStartTime_;
    uint32_t intervalSec = (uint32_t)cycleIntervalMinutes_ * 60;
    if (intervalSec > 0 && segmentElapsedSec >= intervalSec) {
        advanceTape(200);
        segmentStartTime_ = now / 1000;
    }
}
