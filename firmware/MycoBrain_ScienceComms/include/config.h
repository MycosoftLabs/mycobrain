/**
 * MycoBrain Science Communications Firmware
 * Hardware Configuration
 * 
 * IMPORTANT: These pin assignments are verified from the schematic.
 * DO NOT CHANGE without verifying against hardware.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS (VERIFIED FROM SCHEMATIC)
// ============================================================================

// NeoPixel (SK6805) - Onboard addressable RGB LED
// Net: NEO_1 → series resistor → D7 SK6805 DI
#ifdef PIN_NEOPIXEL
#undef PIN_NEOPIXEL
#endif
#define PIN_NEOPIXEL        15

// Buzzer (MOSFET-driven)
#define PIN_BUZZER          16

// I2C Bus (3.3V with pullups)
#define PIN_I2C_SCL         4
#define PIN_I2C_SDA         5

// MOSFET Digital Outputs (NOT LEDs!)
#define PIN_OUT_1           12
#define PIN_OUT_2           13
#define PIN_OUT_3           14

// Analog Inputs
#define PIN_AIN_1           6
#define PIN_AIN_2           7
#define PIN_AIN_3           10
#define PIN_AIN_4           11

// ============================================================================
// NEOPIXEL CONFIGURATION
// ============================================================================

#define NEOPIXEL_COUNT      1       // Onboard pixel count
#define NEOPIXEL_BRIGHTNESS 128     // Default brightness (0-255)

// ============================================================================
// BUZZER CONFIGURATION
// ============================================================================

#define BUZZER_DEFAULT_FREQ 1000    // Default frequency Hz
#define BUZZER_DEFAULT_DUR  100     // Default duration ms
#define BUZZER_PWM_CHANNEL  0       // LEDC channel for buzzer
#define BUZZER_PWM_RESOLUTION 8     // 8-bit resolution

// ============================================================================
// MODEM CONFIGURATION
// ============================================================================

// Optical modem (camera-friendly rates)
#define OPTICAL_DEFAULT_RATE_HZ     10      // 10 Hz for 30fps cameras
#define OPTICAL_MAX_RATE_HZ         30      // Max rate
#define OPTICAL_PREAMBLE_BITS       8       // Preamble length

// Acoustic modem (FSK)
#define ACOUSTIC_DEFAULT_F0         1800    // Mark frequency
#define ACOUSTIC_DEFAULT_F1         2400    // Space frequency
#define ACOUSTIC_DEFAULT_SYMBOL_MS  30      // Symbol duration
#define ACOUSTIC_PREAMBLE_SYMBOLS   16      // Preamble length

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================

#define SERIAL_BAUD         115200
#define JSON_DOC_SIZE       1024    // StaticJsonDocument size

// ============================================================================
// FIRMWARE VERSION
// ============================================================================

#define FIRMWARE_NAME       "MycoBrain-ScienceComms"
#define FIRMWARE_VERSION    "1.0.0"
#define FIRMWARE_BUILD_DATE __DATE__

#endif // CONFIG_H

