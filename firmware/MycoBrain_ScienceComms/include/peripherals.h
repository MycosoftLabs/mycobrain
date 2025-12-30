/**
 * MycoBrain Science Communications Firmware
 * Peripheral Discovery Module
 * 
 * I2C scanning, peripheral detection, and descriptor reporting.
 * Enables plug-and-play widgets on the dashboard.
 */

#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// ============================================================================
// PERIPHERAL TYPE DEFINITIONS
// ============================================================================

enum PeripheralType {
    PERIPH_UNKNOWN = 0,
    PERIPH_BME688,          // Environmental sensor
    PERIPH_SHT40,           // Temp/Humidity
    PERIPH_BH1750,          // Light sensor
    PERIPH_SGP40,           // VOC sensor
    PERIPH_SSD1306,         // OLED display
    PERIPH_ADS1115,         // ADC
    PERIPH_MCP23017,        // GPIO expander
    PERIPH_PCA9685,         // PWM driver
    PERIPH_EEPROM_ID,       // ID EEPROM for accessories
    PERIPH_PIXEL_ARRAY,     // Declared pixel array
    PERIPH_PHOTODIODE_RX,   // Optical receiver
    PERIPH_MIC_I2S,         // I2S microphone
    PERIPH_LIDAR,           // LiDAR sensor
    PERIPH_CAMERA_PROXY     // Camera interface
};

// ============================================================================
// PERIPHERAL CAPABILITY FLAGS
// ============================================================================

#define CAP_TELEMETRY       (1 << 0)
#define CAP_CONTROL         (1 << 1)
#define CAP_ACOUSTIC_RX     (1 << 2)
#define CAP_OPTICAL_RX      (1 << 3)
#define CAP_OPTICAL_TX      (1 << 4)
#define CAP_HAPTIC          (1 << 5)

// ============================================================================
// PERIPHERAL DESCRIPTOR
// ============================================================================

struct PeripheralDescriptor {
    uint8_t address;            // I2C address
    PeripheralType type;
    char uid[32];               // Unique ID
    char vendor[16];
    char product[24];
    char revision[8];
    uint8_t capabilities;       // Capability flags
    bool present;               // Currently detected
    uint32_t lastSeen;          // millis() when last detected
};

// ============================================================================
// PERIPHERAL REGISTRY (max devices)
// ============================================================================

#define MAX_PERIPHERALS     16

// ============================================================================
// PERIPHERAL MODULE INTERFACE
// ============================================================================

namespace Peripherals {
    // Initialization
    void init();
    
    // Scanning
    int scan();                             // Returns count of devices found
    bool isDevicePresent(uint8_t address);
    
    // Registry
    int getCount();
    PeripheralDescriptor* getByAddress(uint8_t address);
    PeripheralDescriptor* getByIndex(int index);
    
    // Hotplug monitoring
    void enableHotplug(bool enable);
    bool isHotplugEnabled();
    void updateHotplug();   // Call in loop()
    
    // Declared peripherals (for non-I2C devices like NeoPixel arrays)
    bool declarePeripheral(const char* type, uint8_t pin, uint16_t count);
    
    // JSON output
    void getDescriptorJson(const PeripheralDescriptor* desc, char* buffer, size_t bufSize);
    void getListJson(char* buffer, size_t bufSize);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

PeripheralType identifyI2CDevice(uint8_t address);
const char* peripheralTypeName(PeripheralType type);

#endif // PERIPHERALS_H

