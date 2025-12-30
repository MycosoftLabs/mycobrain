/**
 * MycoBrain Science Communications Firmware
 * Peripheral Discovery Module Implementation
 * 
 * I2C scanning and peripheral identification.
 */

#include "peripherals.h"
#include <Wire.h>

// ============================================================================
// PERIPHERAL REGISTRY
// ============================================================================

static PeripheralDescriptor registry[MAX_PERIPHERALS];
static int registryCount = 0;
static bool hotplugEnabled = false;
static uint32_t lastScanTime = 0;
static const uint32_t HOTPLUG_INTERVAL_MS = 5000;

// ============================================================================
// KNOWN I2C ADDRESSES
// ============================================================================

struct KnownDevice {
    uint8_t address;
    PeripheralType type;
    const char* vendor;
    const char* product;
};

static const KnownDevice knownDevices[] = {
    // BME688 variants
    {0x76, PERIPH_BME688, "Bosch", "BME688"},
    {0x77, PERIPH_BME688, "Bosch", "BME688"},
    
    // SHT40
    {0x44, PERIPH_SHT40, "Sensirion", "SHT40"},
    {0x45, PERIPH_SHT40, "Sensirion", "SHT40"},
    
    // Light sensors
    {0x23, PERIPH_BH1750, "ROHM", "BH1750"},
    
    // VOC
    {0x59, PERIPH_SGP40, "Sensirion", "SGP40"},
    
    // Displays
    {0x3C, PERIPH_SSD1306, "Generic", "SSD1306 OLED"},
    {0x3D, PERIPH_SSD1306, "Generic", "SSD1306 OLED"},
    
    // ADC
    {0x48, PERIPH_ADS1115, "TI", "ADS1115"},
    {0x49, PERIPH_ADS1115, "TI", "ADS1115"},
    
    // GPIO expanders
    {0x20, PERIPH_MCP23017, "Microchip", "MCP23017"},
    {0x21, PERIPH_MCP23017, "Microchip", "MCP23017"},
    
    // PWM driver
    {0x40, PERIPH_PCA9685, "NXP", "PCA9685"},
    
    // ID EEPROM (common addresses)
    {0x50, PERIPH_EEPROM_ID, "Generic", "EEPROM"},
    {0x51, PERIPH_EEPROM_ID, "Generic", "EEPROM"},
};
static const int knownDeviceCount = sizeof(knownDevices) / sizeof(knownDevices[0]);

// ============================================================================
// INITIALIZATION
// ============================================================================

void Peripherals::init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);  // 100kHz I2C
    
    memset(registry, 0, sizeof(registry));
    registryCount = 0;
    
    // Initial scan
    scan();
}

// ============================================================================
// I2C SCANNING
// ============================================================================

int Peripherals::scan() {
    int found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            // Device found
            PeripheralDescriptor* existing = getByAddress(addr);
            
            if (existing) {
                // Update existing
                existing->present = true;
                existing->lastSeen = millis();
            } else if (registryCount < MAX_PERIPHERALS) {
                // Add new
                PeripheralDescriptor& desc = registry[registryCount];
                desc.address = addr;
                desc.type = identifyI2CDevice(addr);
                desc.present = true;
                desc.lastSeen = millis();
                desc.capabilities = CAP_TELEMETRY;
                
                // Lookup known device info
                for (int i = 0; i < knownDeviceCount; i++) {
                    if (knownDevices[i].address == addr) {
                        strncpy(desc.vendor, knownDevices[i].vendor, sizeof(desc.vendor) - 1);
                        strncpy(desc.product, knownDevices[i].product, sizeof(desc.product) - 1);
                        break;
                    }
                }
                
                // Generate UID
                snprintf(desc.uid, sizeof(desc.uid), "i2c0-%02x", addr);
                strcpy(desc.revision, "1.0");
                
                registryCount++;
            }
            found++;
        }
    }
    
    lastScanTime = millis();
    return found;
}

bool Peripherals::isDevicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

// ============================================================================
// REGISTRY ACCESS
// ============================================================================

int Peripherals::getCount() {
    return registryCount;
}

PeripheralDescriptor* Peripherals::getByAddress(uint8_t address) {
    for (int i = 0; i < registryCount; i++) {
        if (registry[i].address == address) {
            return &registry[i];
        }
    }
    return nullptr;
}

PeripheralDescriptor* Peripherals::getByIndex(int index) {
    if (index >= 0 && index < registryCount) {
        return &registry[index];
    }
    return nullptr;
}

// ============================================================================
// HOTPLUG MONITORING
// ============================================================================

void Peripherals::enableHotplug(bool enable) {
    hotplugEnabled = enable;
}

bool Peripherals::isHotplugEnabled() {
    return hotplugEnabled;
}

void Peripherals::updateHotplug() {
    if (!hotplugEnabled) return;
    
    if (millis() - lastScanTime >= HOTPLUG_INTERVAL_MS) {
        // Check existing devices
        for (int i = 0; i < registryCount; i++) {
            registry[i].present = isDevicePresent(registry[i].address);
            if (registry[i].present) {
                registry[i].lastSeen = millis();
            }
        }
        
        // Scan for new devices
        scan();
    }
}

// ============================================================================
// DECLARED PERIPHERALS
// ============================================================================

bool Peripherals::declarePeripheral(const char* type, uint8_t pin, uint16_t count) {
    if (registryCount >= MAX_PERIPHERALS) return false;
    
    PeripheralDescriptor& desc = registry[registryCount];
    memset(&desc, 0, sizeof(desc));
    
    if (strcmp(type, "pixel_array") == 0) {
        desc.type = PERIPH_PIXEL_ARRAY;
        desc.capabilities = CAP_CONTROL | CAP_OPTICAL_TX;
        strncpy(desc.vendor, "Mycosoft", sizeof(desc.vendor) - 1);
        snprintf(desc.product, sizeof(desc.product), "NeoPixel x%d", count);
    } else if (strcmp(type, "photodiode_rx") == 0) {
        desc.type = PERIPH_PHOTODIODE_RX;
        desc.capabilities = CAP_TELEMETRY | CAP_OPTICAL_RX;
        strncpy(desc.vendor, "Generic", sizeof(desc.vendor) - 1);
        strncpy(desc.product, "Photodiode", sizeof(desc.product) - 1);
    } else {
        desc.type = PERIPH_UNKNOWN;
    }
    
    desc.address = pin;  // Store pin as "address" for declared peripherals
    desc.present = true;
    desc.lastSeen = millis();
    snprintf(desc.uid, sizeof(desc.uid), "gpio-%d-%s", pin, type);
    strcpy(desc.revision, "1.0");
    
    registryCount++;
    return true;
}

// ============================================================================
// JSON OUTPUT
// ============================================================================

void Peripherals::getDescriptorJson(const PeripheralDescriptor* desc, char* buffer, size_t bufSize) {
    if (!desc) {
        snprintf(buffer, bufSize, "null");
        return;
    }
    
    char boardId[20];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(boardId, sizeof(boardId), "%012llX", mac);
    
    snprintf(buffer, bufSize,
        "{\"type\":\"periph\",\"board_id\":\"%s\",\"bus\":\"i2c0\",\"address\":\"0x%02X\","
        "\"peripheral_uid\":\"%s\",\"peripheral_type\":\"%s\","
        "\"vendor\":\"%s\",\"product\":\"%s\",\"revision\":\"%s\","
        "\"present\":%s,\"last_seen\":%lu}",
        boardId,
        desc->address,
        desc->uid,
        peripheralTypeName(desc->type),
        desc->vendor,
        desc->product,
        desc->revision,
        desc->present ? "true" : "false",
        desc->lastSeen
    );
}

void Peripherals::getListJson(char* buffer, size_t bufSize) {
    char* ptr = buffer;
    size_t remaining = bufSize;
    
    int written = snprintf(ptr, remaining, "{\"type\":\"periph_list\",\"count\":%d,\"devices\":[", registryCount);
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < registryCount && remaining > 100; i++) {
        if (i > 0) {
            *ptr++ = ',';
            remaining--;
        }
        
        char descBuf[256];
        getDescriptorJson(&registry[i], descBuf, sizeof(descBuf));
        written = snprintf(ptr, remaining, "%s", descBuf);
        ptr += written;
        remaining -= written;
    }
    
    snprintf(ptr, remaining, "]}");
}

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================

PeripheralType identifyI2CDevice(uint8_t address) {
    for (int i = 0; i < knownDeviceCount; i++) {
        if (knownDevices[i].address == address) {
            return knownDevices[i].type;
        }
    }
    return PERIPH_UNKNOWN;
}

const char* peripheralTypeName(PeripheralType type) {
    switch (type) {
        case PERIPH_BME688: return "bme688";
        case PERIPH_SHT40: return "sht40";
        case PERIPH_BH1750: return "bh1750";
        case PERIPH_SGP40: return "sgp40";
        case PERIPH_SSD1306: return "ssd1306";
        case PERIPH_ADS1115: return "ads1115";
        case PERIPH_MCP23017: return "mcp23017";
        case PERIPH_PCA9685: return "pca9685";
        case PERIPH_EEPROM_ID: return "eeprom";
        case PERIPH_PIXEL_ARRAY: return "pixel_array";
        case PERIPH_PHOTODIODE_RX: return "photodiode_rx";
        case PERIPH_MIC_I2S: return "mic_i2s";
        case PERIPH_LIDAR: return "lidar";
        case PERIPH_CAMERA_PROXY: return "camera_proxy";
        default: return "unknown";
    }
}

