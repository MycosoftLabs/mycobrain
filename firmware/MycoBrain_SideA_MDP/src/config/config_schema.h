#ifndef CONFIG_SCHEMA_H
#define CONFIG_SCHEMA_H

#include <stdint.h>
#include <stdbool.h>

// Calibration configuration
struct CalibrationConfig {
  float analog_offset[4];      // Per-channel offset
  float analog_gain[4];         // Per-channel gain
  float adc_reference;          // ADC reference voltage
  float bme_temp_offset;        // BME temperature offset
  float bme_humidity_offset;    // BME humidity offset
};

// Pin configuration
struct PinConfig {
  int8_t ai_pins[4];            // Analog input pins
  int8_t mos_pins[3];            // MOSFET output pins
  int8_t i2c_sda;                // I2C SDA pin
  int8_t i2c_scl;                // I2C SCL pin
};

// Threshold configuration
struct ThresholdConfig {
  float analog_high[4];          // High thresholds per channel
  float analog_low[4];           // Low thresholds per channel
  // Future: sensor thresholds
};

// WiFi configuration
struct WiFiConfig {
  char ap_ssid[33];              // AP SSID (max 32 chars + null)
  char ap_password[65];          // AP password (max 64 chars + null)
  bool sta_enabled;              // Enable STA mode
  char sta_ssid[33];             // STA SSID
  char sta_password[65];          // STA password
  uint8_t wifi_mode;             // 0=AP only, 1=STA only, 2=AP+STA
};

// WiFi mode constants
#define WIFI_MODE_AP_ONLY  0
#define WIFI_MODE_STA_ONLY 1
#define WIFI_MODE_AP_STA   2

#endif // CONFIG_SCHEMA_H

