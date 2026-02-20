/**
 * SporeBase Controller - Fan, tape motor, and sampling cycle logic.
 * MycoBrain firmware common module for SporeBase device role.
 * Created: February 12, 2026
 */

#ifndef SPOREBASE_CONTROLLER_H
#define SPOREBASE_CONTROLLER_H

#include <Arduino.h>

class SporeBaseController {
public:
    SporeBaseController() = default;

    /** Initialize fan PWM and tachometer input. */
    void initFan(uint8_t pwmPin, uint8_t tachPin);

    /** Initialize tape advance stepper (step + direction). */
    void initTapeMotor(uint8_t stepPin, uint8_t dirPin);

    /** Set target flow rate in L/min (drives fan PWM). */
    void setFlowRate(float litersPerMinute);

    /** Advance tape by step count. */
    void advanceTape(uint16_t steps);

    /** Start timed sampling cycle; interval in minutes (15–60). */
    void startSamplingCycle(uint16_t intervalMinutes);

    /** Stop sampling cycle. */
    void stopSamplingCycle();

    /** Return current spore count (from sensor or 0 if not available). */
    float getSporeCount();

    /** Call from loop() to run cycle state machine. */
    void tick();

    /** Whether a sampling cycle is active. */
    bool isSampling() const { return samplingActive_; }

    /** Current segment start timestamp (seconds since boot or 0). */
    uint32_t getSegmentStartTime() const { return segmentStartTime_; }

private:
    uint8_t fanPwmPin_ = 0;
    uint8_t fanTachPin_ = 0;
    uint8_t stepPin_ = 0;
    uint8_t dirPin_ = 0;
    float flowRateLpm_ = 10.0f;
    uint16_t cycleIntervalMinutes_ = 15;
    bool samplingActive_ = false;
    uint32_t segmentStartTime_ = 0;
    uint32_t lastCycleCheck_ = 0;
    float sporeCount_ = 0.0f;
};

#endif // SPOREBASE_CONTROLLER_H
