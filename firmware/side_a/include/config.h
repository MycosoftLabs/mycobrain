/**
 * MycoBrain Side A Firmware
 * Hardware Configuration
 * 
 * Pin definitions for peripherals and sensors.
 * Used by NeoPixel, Buzzer, and power management modules.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS
// ============================================================================

// NeoPixel (SK6805/WS2812) - Addressable RGB LED
#ifndef PIN_NEOPIXEL
#define PIN_NEOPIXEL        15
#endif

// Buzzer (MOSFET-driven)
#ifndef PIN_BUZZER
#define PIN_BUZZER          16
#endif

// I2C Bus (sensors)
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL         4
#endif
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA         5
#endif

// MOSFET Digital Outputs (for power control, fans, etc.)
#define PIN_OUT_1           12
#define PIN_OUT_2           13
#define PIN_OUT_3           14

// Analog Inputs (battery voltage, solar, external sensors)
#define PIN_AIN_1           6
#define PIN_AIN_2           7
#define PIN_AIN_3           10
#define PIN_AIN_4           11

// ============================================================================
// NEOPIXEL CONFIGURATION
// ============================================================================

#ifndef NEOPIXEL_COUNT
#define NEOPIXEL_COUNT      1       // Onboard pixel count
#endif
#define NEOPIXEL_BRIGHTNESS 128     // Default brightness (0-255)

// ============================================================================
// BUZZER CONFIGURATION
// ============================================================================

#define BUZZER_DEFAULT_FREQ 1000    // Default frequency Hz
#define BUZZER_DEFAULT_DUR  100     // Default duration ms
#define BUZZER_PWM_CHANNEL  0       // LEDC channel for buzzer
#define BUZZER_PWM_RESOLUTION 8     // 8-bit resolution

// ============================================================================
// POWER MANAGEMENT CONFIGURATION
// ============================================================================

// Battery voltage divider ratio (if using voltage divider)
#define BATTERY_VOLTAGE_DIVIDER 2.0
#define BATTERY_ADC_PIN         PIN_AIN_1
#define SOLAR_ADC_PIN           PIN_AIN_2

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================

#ifndef SERIAL_BAUD
#define SERIAL_BAUD         115200
#endif
#define JSON_DOC_SIZE       1024    // StaticJsonDocument size

#endif // CONFIG_H
