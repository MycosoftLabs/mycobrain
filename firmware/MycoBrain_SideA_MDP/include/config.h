#pragma once

#include <stdint.h>

// Device role: mushroom1 or hyphae1 (set via -DDEVICE_ROLE_HYPHAE1 in platformio.ini)
#if defined(DEVICE_ROLE_HYPHAE1)
  #define MYCOBRAIN_DEVICE_ROLE "hyphae1"
  #define IS_HYPHAE1 1
#else
  #define MYCOBRAIN_DEVICE_ROLE "mushroom1"
  #define IS_HYPHAE1 0
#endif

// Shared pins (ESP32-S3)
static const int I2C_SDA = 5;
static const int I2C_SCL = 4;
static const int AI_PINS[4] = {6, 7, 10, 11};
static const int MOSFET_PINS[3] = {12, 13, 14};
static const int NEOPIXEL_PIN = 15;
static const int BUZZER_PIN = 16;
static const uint32_t I2C_FREQ = 100000;

// BME688 I2C addresses
static const uint8_t BME_ADDRS[2] = {0x76, 0x77};

#if IS_HYPHAE1
// Hyphae 1: soil moisture ADC (capacitive sensor on GPIO 8)
static const int SOIL_MOISTURE_ADC_PIN = 8;
#endif

// Nemo Claw (MicoLatch servo gripper) configuration
// Mode 1: Direct GPIO servo via LEDC PWM (use CLAW_SERVO_PIN)
// Mode 2: I2C PCA9685 PWM driver at 0x40 (auto-detected via I2C scan)
static const int CLAW_SERVO_PIN = 13;       // MOSFET3 pin doubles as servo output
static const int CLAW_FORCE_ADC_PIN = 11;   // AI4 for optional force feedback
static const uint8_t CLAW_PCA9685_ADDR = 0x40;
static const uint8_t CLAW_PCA9685_CHANNEL = 0;
