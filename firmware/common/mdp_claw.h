/**
 * Nemo Claw Controller - Servo gripper for MicoLatch payload interface.
 * Supports direct GPIO (LEDC PWM) and I2C PCA9685 servo driver.
 * MycoBrain firmware common module for mycodrone/claw device roles.
 *
 * Integration with Seeed OpenClaw AI agent framework:
 *   OpenClaw skills invoke claw commands via JSON->MDP bridge on Side B.
 *   Compatible with SO-Arm/LeRobot servo patterns.
 *
 * Created: March 19, 2026
 */

#ifndef MDP_CLAW_H
#define MDP_CLAW_H

#include <Arduino.h>
#include <Wire.h>

// Claw MDP command IDs (0x0030-0x003F range)
#define CMD_CLAW_GRIP       0x0030
#define CMD_CLAW_RELEASE    0x0031
#define CMD_CLAW_POSITION   0x0032
#define CMD_CLAW_STATUS     0x0033
#define CMD_CLAW_CALIBRATE  0x0034

// PCA9685 registers
#define PCA9685_ADDR        0x40
#define PCA9685_MODE1       0x00
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

// Servo pulse range (microseconds) for standard hobby servos
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2500
#define SERVO_FREQ_HZ 50

// LEDC channel for direct GPIO servo (channels 0-7 on ESP32-S3)
// Channel 0 is used by buzzer, so use channel 1
#define CLAW_LEDC_CHANNEL  1
#define CLAW_LEDC_TIMER    1

enum ClawMode : uint8_t {
    CLAW_MODE_NONE = 0,
    CLAW_MODE_GPIO = 1,    // Direct GPIO via LEDC PWM
    CLAW_MODE_PCA9685 = 2  // I2C PCA9685 16-channel PWM driver
};

struct ClawStatus {
    uint8_t  position;         // 0-180 degrees
    bool     is_closed;        // true if position >= grip_angle
    uint16_t force_adc;        // Force feedback ADC reading (0 if unavailable)
    ClawMode mode;             // Active control mode
    bool     calibrated;       // Whether calibration has been performed
};

class ClawController {
public:
    ClawController() = default;

    /**
     * Initialize claw with direct GPIO servo control (LEDC PWM).
     * @param servoPin GPIO pin connected to servo signal wire
     * @param forceAdcPin GPIO pin for force feedback (0 = none)
     */
    void initGpio(uint8_t servoPin, uint8_t forceAdcPin = 0) {
        mode_ = CLAW_MODE_GPIO;
        servoPin_ = servoPin;
        forceAdcPin_ = forceAdcPin;

        // Configure LEDC for 50Hz servo PWM
        ledcSetup(CLAW_LEDC_CHANNEL, SERVO_FREQ_HZ, 16); // 16-bit resolution
        ledcAttachPin(servoPin_, CLAW_LEDC_CHANNEL);

        if (forceAdcPin_ > 0) {
            pinMode(forceAdcPin_, INPUT);
        }

        setPosition(releaseAngle_);
        initialized_ = true;
    }

    /**
     * Initialize claw with PCA9685 I2C servo driver.
     * @param wire I2C bus reference
     * @param addr PCA9685 I2C address (default 0x40)
     * @param channel PCA9685 output channel (0-15)
     * @param forceAdcPin GPIO pin for force feedback (0 = none)
     */
    bool initPca9685(TwoWire& wire, uint8_t addr = PCA9685_ADDR,
                     uint8_t channel = 0, uint8_t forceAdcPin = 0) {
        wire_ = &wire;
        pcaAddr_ = addr;
        pcaChannel_ = channel;
        forceAdcPin_ = forceAdcPin;

        // Check PCA9685 is present
        wire_->beginTransmission(pcaAddr_);
        if (wire_->endTransmission() != 0) return false;

        mode_ = CLAW_MODE_PCA9685;

        // Reset PCA9685
        pcaWrite(PCA9685_MODE1, 0x00);
        delay(5);

        // Set PWM frequency to 50Hz for servos
        // prescale = round(25MHz / (4096 * freq)) - 1
        uint8_t prescale = (uint8_t)(25000000.0f / (4096.0f * SERVO_FREQ_HZ) - 0.5f);
        uint8_t oldMode = pcaRead(PCA9685_MODE1);
        pcaWrite(PCA9685_MODE1, (oldMode & 0x7F) | 0x10); // sleep
        pcaWrite(PCA9685_PRESCALE, prescale);
        pcaWrite(PCA9685_MODE1, oldMode);
        delay(5);
        pcaWrite(PCA9685_MODE1, oldMode | 0xA0); // auto-increment + restart

        if (forceAdcPin_ > 0) {
            pinMode(forceAdcPin_, INPUT);
        }

        setPosition(releaseAngle_);
        initialized_ = true;
        return true;
    }

    /** Set servo to angle (0-180 degrees). */
    void setPosition(uint8_t angle) {
        if (!initialized_) return;
        if (angle > 180) angle = 180;
        position_ = angle;

        uint32_t pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);

        if (mode_ == CLAW_MODE_GPIO) {
            // LEDC: 16-bit resolution at 50Hz = 20000us period
            // duty = pulseUs / 20000 * 65536
            uint32_t duty = (pulseUs * 65536UL) / 20000UL;
            ledcWrite(CLAW_LEDC_CHANNEL, duty);
        } else if (mode_ == CLAW_MODE_PCA9685) {
            // PCA9685: 4096 ticks per 20ms period
            uint16_t onTick = 0;
            uint16_t offTick = (uint16_t)((pulseUs * 4096UL) / 20000UL);
            pcaSetChannel(pcaChannel_, onTick, offTick);
        }
    }

    /** Close the gripper to grip angle. */
    void grip() {
        if (!initialized_) return;
        setPosition(gripAngle_);
    }

    /** Open the gripper to release angle. */
    void release() {
        if (!initialized_) return;
        setPosition(releaseAngle_);
    }

    /** Get current position (0-180). */
    uint8_t getPosition() const { return position_; }

    /** Whether claw is considered closed (at or beyond grip angle). */
    bool isClosed() const { return position_ >= gripAngle_; }

    /** Read force feedback ADC (0 if no sensor). */
    uint16_t readForce() const {
        if (forceAdcPin_ == 0) return 0;
        return analogRead(forceAdcPin_);
    }

    /** Get full status struct. */
    ClawStatus getStatus() const {
        ClawStatus s;
        s.position = position_;
        s.is_closed = isClosed();
        s.force_adc = readForce();
        s.mode = mode_;
        s.calibrated = calibrated_;
        return s;
    }

    /** Set grip/release angles (for calibration). */
    void setGripAngle(uint8_t angle) { gripAngle_ = angle; }
    void setReleaseAngle(uint8_t angle) { releaseAngle_ = angle; }
    void setCalibrated(bool cal) { calibrated_ = cal; }

    /** Whether the controller has been initialized. */
    bool isInitialized() const { return initialized_; }
    ClawMode getMode() const { return mode_; }

    /**
     * Non-blocking tick for state machine updates (e.g., slow open/close).
     * Call from loop().
     */
    void tick() {
        // Reserved for future: animated open/close, force-limited grip, etc.
    }

private:
    ClawMode mode_ = CLAW_MODE_NONE;
    bool initialized_ = false;
    bool calibrated_ = false;
    uint8_t position_ = 0;
    uint8_t gripAngle_ = 160;    // Default grip position
    uint8_t releaseAngle_ = 10;  // Default release position

    // GPIO mode
    uint8_t servoPin_ = 0;

    // PCA9685 mode
    TwoWire* wire_ = nullptr;
    uint8_t pcaAddr_ = PCA9685_ADDR;
    uint8_t pcaChannel_ = 0;

    // Force feedback
    uint8_t forceAdcPin_ = 0;

    void pcaWrite(uint8_t reg, uint8_t val) {
        wire_->beginTransmission(pcaAddr_);
        wire_->write(reg);
        wire_->write(val);
        wire_->endTransmission();
    }

    uint8_t pcaRead(uint8_t reg) {
        wire_->beginTransmission(pcaAddr_);
        wire_->write(reg);
        wire_->endTransmission();
        wire_->requestFrom((int)pcaAddr_, 1);
        return wire_->read();
    }

    void pcaSetChannel(uint8_t ch, uint16_t on, uint16_t off) {
        uint8_t reg = PCA9685_LED0_ON_L + 4 * ch;
        wire_->beginTransmission(pcaAddr_);
        wire_->write(reg);
        wire_->write((uint8_t)(on & 0xFF));
        wire_->write((uint8_t)(on >> 8));
        wire_->write((uint8_t)(off & 0xFF));
        wire_->write((uint8_t)(off >> 8));
        wire_->endTransmission();
    }
};

#endif // MDP_CLAW_H
