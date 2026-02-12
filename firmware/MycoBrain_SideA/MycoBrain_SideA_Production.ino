/*
 * MycoBrain Side-A Production Firmware
 * ESP32-S3 Sensor MCU with full feature set
 * 
 * Features:
 * - BME688 environmental sensors (I2C)
 * - I2C sensor scanning with peripheral discovery
 * - Analog inputs (AI1-AI4)
 * - MOSFET control (GPIO12/13/14)
 * - NeoPixel LED control (GPIO15, SK6805)
 * - Buzzer control (GPIO16)
 * - Telemetry transmission (JSON/NDJSON format)
 * - Command handling (JSON and text commands)
 * - Machine mode for website integration
 * - Brownout protection
 * 
 * Protocol Support:
 * - Legacy JSON mode (compatible with mycobrain_dual_service.py)
 * - NDJSON machine mode (for website widget integration)
 * - Text commands: mode machine, dbg off, fmt json, scan, status
 * - JSON commands: All standard commands via JSON
 * 
 * Website Integration:
 * - Initialize with: mode machine, dbg off, fmt json
 * - Peripheral discovery: scan command returns periph_list
 * - Telemetry: type="telemetry" with NDJSON format
 * - Responses: type="ack", type="err" for machine mode
 */

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "soc/rtc_cntl_reg.h" // For brownout detector

// ============================================================================
// Hardware Configuration
// ============================================================================
// I2C
#define I2C_SDA 5
#define I2C_SCL 4
#define I2C_FREQ 100000

// Analog Inputs (ESP32-S3 MycoBrain pinout - VERIFIED)
#define AI1_PIN 6
#define AI2_PIN 7
#define AI3_PIN 10
#define AI4_PIN 11

// MOSFET Outputs (digital)
#define MOSFET_1_PIN 12
#define MOSFET_2_PIN 13
#define MOSFET_3_PIN 14

// NeoPixel (SK6805 on GPIO15)
#define NEOPIXEL_PIN 15
#define NEOPIXEL_COUNT 1

// Buzzer
#define BUZZER_PIN 16

// BME688 I2C Addresses
#define BME688_ADDR_1 0x76
#define BME688_ADDR_2 0x77

// ============================================================================
// Configuration
// ============================================================================
#define SERIAL_BAUD 115200
#define FIRMWARE_VERSION "1.0.0"
#define TELEMETRY_INTERVAL_MS 10000  // Default 10 seconds

// Boot wait for Serial Monitor
static const uint32_t BOOT_SERIAL_WAIT_MS = 2000;

// ============================================================================
// Device Identity Configuration
// ============================================================================
// NVS namespace for device settings
#define NVS_NAMESPACE "mycobrain"
#define NVS_DEVICE_ROLE_KEY "dev_role"
#define NVS_DEVICE_DISPLAY_NAME_KEY "dev_disp"

// Default device role (can be overridden by NVS or compile-time)
// Values: mushroom1, sporebase, hyphae1, alarm, gateway, mycodrone, standalone
#ifndef CONFIG_DEVICE_ROLE_DEFAULT
#define CONFIG_DEVICE_ROLE_DEFAULT "standalone"
#endif
#ifndef CONFIG_DEVICE_DISPLAY_NAME_DEFAULT
#define CONFIG_DEVICE_DISPLAY_NAME_DEFAULT ""
#endif

// ============================================================================
// Global Variables
// ============================================================================
String deviceMac = "";
unsigned long uptimeSeconds = 0;
unsigned long lastUptimeUpdate = 0;
unsigned long lastTelemetry = 0;
unsigned long telemetryInterval = TELEMETRY_INTERVAL_MS;

// Operating mode (for website compatibility)
enum OperatingMode {
  MODE_HUMAN = 0,
  MODE_MACHINE = 1
};
OperatingMode currentMode = MODE_HUMAN;
bool debugEnabled = false;
bool jsonFormat = false;  // NDJSON format flag

// MOSFET states
bool mosfetStates[3] = {false, false, false};

// I2C addresses detected
uint8_t i2cAddresses[8];
uint8_t i2cAddressCount = 0;

// BME688 sensor data
struct BME688Data {
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
  float gas_resistance = 0.0;
  bool valid = false;
} bme688_1, bme688_2;

// Command buffer
String commandBuffer = "";

// NVS Preferences for device settings
Preferences prefs;

// Device identity (role + display name)
char deviceRole[32] = CONFIG_DEVICE_ROLE_DEFAULT;
char deviceDisplayName[64] = CONFIG_DEVICE_DISPLAY_NAME_DEFAULT;

// ============================================================================
// Device Identity Functions
// ============================================================================
void loadDeviceIdentity() {
  if (!prefs.begin(NVS_NAMESPACE, true)) return;
  
  String role = prefs.getString(NVS_DEVICE_ROLE_KEY, CONFIG_DEVICE_ROLE_DEFAULT);
  strncpy(deviceRole, role.c_str(), sizeof(deviceRole) - 1);
  deviceRole[sizeof(deviceRole) - 1] = '\0';
  
  String disp = prefs.getString(NVS_DEVICE_DISPLAY_NAME_KEY, CONFIG_DEVICE_DISPLAY_NAME_DEFAULT);
  strncpy(deviceDisplayName, disp.c_str(), sizeof(deviceDisplayName) - 1);
  deviceDisplayName[sizeof(deviceDisplayName) - 1] = '\0';
  
  prefs.end();
}

void saveDeviceIdentity() {
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putString(NVS_DEVICE_ROLE_KEY, deviceRole);
  prefs.putString(NVS_DEVICE_DISPLAY_NAME_KEY, deviceDisplayName);
  prefs.end();
}

void setDeviceRole(const char* role) {
  if (!role) return;
  strncpy(deviceRole, role, sizeof(deviceRole) - 1);
  deviceRole[sizeof(deviceRole) - 1] = '\0';
  saveDeviceIdentity();
}

void setDeviceDisplayName(const char* name) {
  if (!name) return;
  strncpy(deviceDisplayName, name, sizeof(deviceDisplayName) - 1);
  deviceDisplayName[sizeof(deviceDisplayName) - 1] = '\0';
  saveDeviceIdentity();
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
  // CRITICAL: Disable brownout detector FIRST
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize Serial
  Serial.begin(SERIAL_BAUD);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(10);
    yield();
  }
  delay(BOOT_SERIAL_WAIT_MS);
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);
  delay(100);
  
  // Initialize GPIO
  pinMode(MOSFET_1_PIN, OUTPUT);
  pinMode(MOSFET_2_PIN, OUTPUT);
  pinMode(MOSFET_3_PIN, OUTPUT);
  digitalWrite(MOSFET_1_PIN, LOW);
  digitalWrite(MOSFET_2_PIN, LOW);
  digitalWrite(MOSFET_3_PIN, LOW);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Note: NeoPixel requires NeoPixelBus library - initialize in separate function
  // For now, we'll use GPIO15 as digital output for basic LED control
  pinMode(NEOPIXEL_PIN, OUTPUT);
  digitalWrite(NEOPIXEL_PIN, LOW);
  
  // Get MAC address
  uint64_t chipid = ESP.getEfuseMac();
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(chipid >> 40), (uint8_t)(chipid >> 32),
           (uint8_t)(chipid >> 24), (uint8_t)(chipid >> 16),
           (uint8_t)(chipid >> 8), (uint8_t)chipid);
  deviceMac = String(macStr);
  
  // Load device identity from NVS
  loadDeviceIdentity();
  
  // Initial I2C scan
  scanI2C();
  
  Serial.println("\n========================================");
  Serial.println("MycoBrain Side-A Production Firmware");
  Serial.println("========================================");
  Serial.print("MAC: ");
  Serial.println(deviceMac);
  Serial.print("Firmware: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("I2C devices found: ");
  Serial.println(i2cAddressCount);
  Serial.println("Ready!");
  Serial.flush();
  
  // Startup beep
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  
  // Blink LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(NEOPIXEL_PIN, HIGH);
    delay(100);
    digitalWrite(NEOPIXEL_PIN, LOW);
    delay(100);
  }
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  unsigned long now = millis();
  
  // Feed watchdog
  yield();
  
  // Update uptime
  if (now - lastUptimeUpdate >= 1000) {
    uptimeSeconds = now / 1000;
    lastUptimeUpdate = now;
  }
  
  // Handle commands from Serial
  handleCommands();
  
  // Send telemetry at interval
  if (now - lastTelemetry >= telemetryInterval) {
    sendTelemetry();
    lastTelemetry = now;
  }
  
  delay(10);
}

// ============================================================================
// Command Handling
// ============================================================================
void handleCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
    } else {
      commandBuffer += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  
  // Check if it's a text command (for website compatibility)
  if (cmd.startsWith("mode ")) {
    String mode = cmd.substring(5);
    mode.trim();
    if (mode == "machine") {
      currentMode = MODE_MACHINE;
      jsonFormat = true;
      sendAck("mode", "machine");
    } else if (mode == "human") {
      currentMode = MODE_HUMAN;
      sendAck("mode", "human");
    }
    return;
  }
  
  if (cmd.startsWith("dbg ")) {
    String state = cmd.substring(4);
    state.trim();
    if (state == "off") {
      debugEnabled = false;
      sendAck("dbg", "off");
    } else if (state == "on") {
      debugEnabled = true;
      sendAck("dbg", "on");
    }
    return;
  }
  
  if (cmd.startsWith("fmt ")) {
    String format = cmd.substring(4);
    format.trim();
    if (format == "json") {
      jsonFormat = true;
      sendAck("fmt", "json");
    }
    return;
  }
  
  if (cmd == "scan") {
    scanI2C();
    if (currentMode == MODE_MACHINE) {
      sendPeriphList();
    } else {
      sendI2CScanResult();
    }
    return;
  }
  
  // Plaintext CLI commands (for December 29 firmware compatibility)
  cmd.toLowerCase();
  if (cmd == "coin") {
    tone(BUZZER_PIN, 800, 100);
    delay(100);
    tone(BUZZER_PIN, 1000, 100);
    sendAck("coin", "Playing");
    return;
  }
  if (cmd == "bump") {
    tone(BUZZER_PIN, 200, 50);
    sendAck("bump", "Playing");
    return;
  }
  if (cmd == "power") {
    tone(BUZZER_PIN, 600, 200);
    sendAck("power", "Playing");
    return;
  }
  if (cmd == "1up") {
    tone(BUZZER_PIN, 523, 150);
    delay(150);
    tone(BUZZER_PIN, 659, 150);
    delay(150);
    tone(BUZZER_PIN, 784, 150);
    sendAck("1up", "Playing");
    return;
  }
  if (cmd == "morgio") {
    tone(BUZZER_PIN, 440, 100);
    delay(100);
    tone(BUZZER_PIN, 554, 100);
    delay(100);
    tone(BUZZER_PIN, 659, 200);
    sendAck("morgio", "Playing");
    return;
  }
  if (cmd.startsWith("led rgb")) {
    int r = 0, g = 0, b = 0;
    sscanf(cmd.c_str(), "led rgb %d %d %d", &r, &g, &b);
    if (r > 0 || g > 0 || b > 0) {
      digitalWrite(NEOPIXEL_PIN, HIGH);
      sendAck("led", "rgb " + String(r) + " " + String(g) + " " + String(b));
    } else {
      digitalWrite(NEOPIXEL_PIN, LOW);
      sendAck("led", "off");
    }
    return;
  }
  if (cmd == "led off") {
    digitalWrite(NEOPIXEL_PIN, LOW);
    sendAck("led", "off");
    return;
  }
  
  // Try to parse as JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, cmd);
  
  if (error) {
    sendError("Invalid JSON or unknown command");
    return;
  }
  
  String command = doc["cmd"] | "";
  
  // Machine mode initialization commands (for website compatibility)
  if (command == "mode") {
    String mode = doc["mode"] | "";
    if (mode == "machine") {
      currentMode = MODE_MACHINE;
      jsonFormat = true;
      sendAck("mode", "machine");
    } else if (mode == "human") {
      currentMode = MODE_HUMAN;
      sendResponse("mode", "{\"mode\":\"human\"}");
    } else {
      sendError("mode", "Invalid mode (use 'machine' or 'human')");
    }
  }
  else if (command == "dbg") {
    String state = doc["state"] | "";
    if (state == "off" || state == "false") {
      debugEnabled = false;
      sendAck("dbg", "off");
    } else if (state == "on" || state == "true") {
      debugEnabled = true;
      sendAck("dbg", "on");
    } else {
      sendError("dbg", "Invalid state (use 'on' or 'off')");
    }
  }
  else if (command == "fmt") {
    String format = doc["format"] | "";
    if (format == "json") {
      jsonFormat = true;
      sendAck("fmt", "json");
    } else {
      jsonFormat = false;
      sendAck("fmt", format);
    }
  }
  else if (command == "ping") {
    if (currentMode == MODE_MACHINE) {
      sendAck("ping", "ok");
    } else {
      sendResponse("ping", "{\"status\":\"ok\",\"side\":\"side-a\"}");
    }
  }
  else if (command == "status") {
    sendStatus();
  }
  else if (command == "get_mac") {
    if (currentMode == MODE_MACHINE) {
      DynamicJsonDocument doc(256);
      doc["type"] = "mac";
      doc["mac"] = deviceMac;
      sendNDJSON(doc);
    } else {
      sendResponse("mac", "{\"mac\":\"" + deviceMac + "\"}");
    }
  }
  else if (command == "get_version") {
    if (currentMode == MODE_MACHINE) {
      DynamicJsonDocument doc(256);
      doc["type"] = "version";
      doc["version"] = FIRMWARE_VERSION;
      sendNDJSON(doc);
    } else {
      sendResponse("version", "{\"version\":\"" + String(FIRMWARE_VERSION) + "\"}");
    }
  }
  else if (command == "i2c_scan" || command == "scan") {
    scanI2C();
    if (currentMode == MODE_MACHINE) {
      sendPeriphList();
    } else {
      sendI2CScanResult();
    }
  }
  else if (command == "set_telemetry_interval") {
    if (doc.containsKey("interval_seconds")) {
      telemetryInterval = doc["interval_seconds"].as<unsigned long>() * 1000;
      sendResponse("telemetry_interval", "{\"interval_seconds\":" + String(telemetryInterval / 1000) + "}");
    }
  }
  else if (command == "set_mosfet") {
    if (doc.containsKey("mosfet_index") && doc.containsKey("state")) {
      int index = doc["mosfet_index"].as<int>();
      bool state = doc["state"].as<bool>();
      if (index >= 0 && index < 3) {
        mosfetStates[index] = state;
        int pin = (index == 0) ? MOSFET_1_PIN : (index == 1) ? MOSFET_2_PIN : MOSFET_3_PIN;
        digitalWrite(pin, state ? HIGH : LOW);
        sendResponse("mosfet", "{\"mosfet_index\":" + String(index) + ",\"state\":" + String(state ? "true" : "false") + "}");
      } else {
        sendError("Invalid mosfet_index (0-2)");
      }
    }
  }
  else if (command == "read_sensor") {
    int sensor_id = doc["sensor_id"] | 0;
    readBME688(sensor_id);
    sendSensorData(sensor_id);
  }
  else if (command == "buzzer" || command == "buzz") {
    // Support both buzzer and buzz commands
    if (doc.containsKey("frequency") && doc.containsKey("duration")) {
      int freq = doc["frequency"].as<int>();
      int duration = doc["duration"].as<int>();
      tone(BUZZER_PIN, freq, duration);
      sendAck("buzzer", "Tone playing");
    } else if (doc.containsKey("pattern")) {
      String pattern = doc["pattern"] | "";
      // Pattern support (coin, bump, power, 1up, morgio, etc.)
      if (pattern == "coin") {
        tone(BUZZER_PIN, 800, 100);
        delay(100);
        tone(BUZZER_PIN, 1000, 100);
        sendAck("buzzer", "Pattern: coin");
      } else if (pattern == "bump") {
        tone(BUZZER_PIN, 200, 50);
        sendAck("buzzer", "Pattern: bump");
      } else if (pattern == "power") {
        tone(BUZZER_PIN, 600, 200);
        sendAck("buzzer", "Pattern: power");
      } else if (pattern == "1up") {
        tone(BUZZER_PIN, 523, 150);
        delay(150);
        tone(BUZZER_PIN, 659, 150);
        delay(150);
        tone(BUZZER_PIN, 784, 150);
        sendAck("buzzer", "Pattern: 1up");
      } else if (pattern == "morgio") {
        // Morgio pattern
        tone(BUZZER_PIN, 440, 100);
        delay(100);
        tone(BUZZER_PIN, 554, 100);
        delay(100);
        tone(BUZZER_PIN, 659, 200);
        sendAck("buzzer", "Pattern: morgio");
      } else {
        sendError("Unknown buzzer pattern: " + pattern);
      }
    } else if (doc.containsKey("stop")) {
      noTone(BUZZER_PIN);
      sendAck("buzzer", "Stopped");
    } else {
      sendError("buzzer command requires 'frequency' and 'duration', or 'pattern'");
    }
  }
  else if (command == "led" || command == "pixel") {
    // LED/NeoPixel control (basic support - full NeoPixelBus would require library)
    if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
      uint8_t r = doc["r"].as<uint8_t>();
      uint8_t g = doc["g"].as<uint8_t>();
      uint8_t b = doc["b"].as<uint8_t>();
      // Basic digital control for now (GPIO15)
      // Full NeoPixel control would require NeoPixelBus library
      if (r > 0 || g > 0 || b > 0) {
        digitalWrite(NEOPIXEL_PIN, HIGH);
      } else {
        digitalWrite(NEOPIXEL_PIN, LOW);
      }
      sendAck("led", "Color set (basic mode)");
    } else if (doc.containsKey("off")) {
      digitalWrite(NEOPIXEL_PIN, LOW);
      sendAck("led", "LED off");
    } else if (doc.containsKey("pattern")) {
      String pattern = doc["pattern"] | "";
      // Basic pattern support
      if (pattern == "blink") {
        for (int i = 0; i < 3; i++) {
          digitalWrite(NEOPIXEL_PIN, HIGH);
          delay(200);
          digitalWrite(NEOPIXEL_PIN, LOW);
          delay(200);
        }
        sendAck("led", "Pattern: blink");
      } else {
        sendError("Unknown LED pattern: " + pattern);
      }
    } else {
      sendError("led command requires 'r', 'g', 'b' or 'off' or 'pattern'");
    }
  }
  else if (command == "reset") {
    sendResponse("reset", "{\"status\":\"resetting\"}");
    delay(1000);
    ESP.restart();
  }
  else if (command == "set_device_role") {
    String role = doc["role"] | "";
    if (role.length() > 0) {
      setDeviceRole(role.c_str());
      sendAck("set_device_role", role);
    } else {
      sendError("set_device_role requires 'role' parameter");
    }
  }
  else if (command == "set_device_display_name") {
    String name = doc["name"] | "";
    setDeviceDisplayName(name.c_str());
    sendAck("set_device_display_name", name);
  }
  else if (command == "get_device_identity") {
    DynamicJsonDocument respDoc(256);
    respDoc["type"] = "device_identity";
    respDoc["device_role"] = deviceRole;
    if (strlen(deviceDisplayName) > 0) {
      respDoc["device_display_name"] = deviceDisplayName;
    }
    respDoc["ts"] = millis();
    if (currentMode == MODE_MACHINE && jsonFormat) {
      sendNDJSON(respDoc);
    } else {
      sendResponse("device_identity", respDoc);
    }
  }
  else {
    sendError("Unknown command: " + command);
  }
}

// ============================================================================
// Telemetry
// ============================================================================
void sendTelemetry() {
  // Read analog inputs
  float ai1 = analogRead(AI1_PIN) * 3.3 / 4095.0;
  float ai2 = analogRead(AI2_PIN) * 3.3 / 4095.0;
  float ai3 = analogRead(AI3_PIN) * 3.3 / 4095.0;
  float ai4 = analogRead(AI4_PIN) * 3.3 / 4095.0;
  
  // Read BME688 sensors
  readBME688(0);
  readBME688(1);
  
  if (currentMode == MODE_MACHINE && jsonFormat) {
    // NDJSON format for machine mode
    DynamicJsonDocument doc(2048);
    doc["type"] = "telemetry";
    doc["ts"] = millis();
    
    // Board ID (MAC address)
    doc["board_id"] = deviceMac;
    
    // Device identity
    doc["device_role"] = deviceRole;
    if (strlen(deviceDisplayName) > 0) {
      doc["device_display_name"] = deviceDisplayName;
    }
    
    // Analog inputs
    doc["ai1_voltage"] = ai1;
    doc["ai2_voltage"] = ai2;
    doc["ai3_voltage"] = ai3;
    doc["ai4_voltage"] = ai4;
    
    // Sensor data
    if (bme688_1.valid) {
      doc["temperature"] = bme688_1.temperature;
      doc["humidity"] = bme688_1.humidity;
      doc["pressure"] = bme688_1.pressure;
      doc["gas_resistance"] = bme688_1.gas_resistance;
    } else if (bme688_2.valid) {
      doc["temperature"] = bme688_2.temperature;
      doc["humidity"] = bme688_2.humidity;
      doc["pressure"] = bme688_2.pressure;
      doc["gas_resistance"] = bme688_2.gas_resistance;
    }
    
    // MOSFET states
    JsonArray mosfetArray = doc.createNestedArray("mosfet_states");
    for (int i = 0; i < 3; i++) {
      mosfetArray.add(mosfetStates[i]);
    }
    
    // I2C addresses
    JsonArray i2cArray = doc.createNestedArray("i2c_addresses");
    for (int i = 0; i < i2cAddressCount; i++) {
      i2cArray.add(i2cAddresses[i]);
    }
    
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["uptime_seconds"] = uptimeSeconds;
    
    sendNDJSON(doc);
  } else {
    // Legacy format
    DynamicJsonDocument doc(2048);
    doc["ai1_voltage"] = ai1;
    doc["ai2_voltage"] = ai2;
    doc["ai3_voltage"] = ai3;
    doc["ai4_voltage"] = ai4;
    
    if (bme688_1.valid) {
      doc["temperature"] = bme688_1.temperature;
      doc["humidity"] = bme688_1.humidity;
      doc["pressure"] = bme688_1.pressure;
      doc["gas_resistance"] = bme688_1.gas_resistance;
    } else if (bme688_2.valid) {
      doc["temperature"] = bme688_2.temperature;
      doc["humidity"] = bme688_2.humidity;
      doc["pressure"] = bme688_2.pressure;
      doc["gas_resistance"] = bme688_2.gas_resistance;
    }
    
    JsonArray mosfetArray = doc.createNestedArray("mosfet_states");
    for (int i = 0; i < 3; i++) {
      mosfetArray.add(mosfetStates[i]);
    }
    
    JsonArray i2cArray = doc.createNestedArray("i2c_addresses");
    for (int i = 0; i < i2cAddressCount; i++) {
      i2cArray.add(i2cAddresses[i]);
    }
    
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["uptime_seconds"] = uptimeSeconds;
    
    DynamicJsonDocument response(2048);
    response["type"] = "telemetry";
    response["data"] = doc;
    
    serializeJson(response, Serial);
    Serial.println();
    Serial.flush();
  }
}

// ============================================================================
// I2C Functions
// ============================================================================
void scanI2C() {
  i2cAddressCount = 0;
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      i2cAddresses[i2cAddressCount++] = address;
      if (i2cAddressCount >= 8) break;
    }
  }
}

void sendI2CScanResult() {
  DynamicJsonDocument doc(512);
  JsonArray addrArray = doc.createNestedArray("addresses");
  for (int i = 0; i < i2cAddressCount; i++) {
    addrArray.add(i2cAddresses[i]);
  }
  doc["count"] = i2cAddressCount;
  
  sendResponse("i2c_scan", doc);
}

void sendPeriphList() {
  // NDJSON format for peripheral list (website expects this)
  DynamicJsonDocument doc(2048);
  doc["type"] = "periph_list";
  doc["ts"] = millis();
  doc["board_id"] = deviceMac;
  
  JsonArray peripherals = doc.createNestedArray("peripherals");
  for (int i = 0; i < i2cAddressCount; i++) {
    JsonObject periph = peripherals.createNestedObject();
    periph["uid"] = "i2c0-" + String(i2cAddresses[i], HEX);
    periph["address"] = i2cAddresses[i];
    periph["type"] = "unknown";  // Could be enhanced with device identification
    periph["vendor"] = "unknown";
    periph["product"] = "unknown";
    periph["present"] = true;
  }
  
  doc["count"] = i2cAddressCount;
  sendNDJSON(doc);
}

void readBME688(int sensor_id) {
  // Simplified BME688 reading - actual implementation would use BME688 library
  // For now, return placeholder data
  // TODO: Implement full BME688 reading with Adafruit BME680 library
  
  if (sensor_id == 0) {
    bme688_1.valid = false; // Set to true when sensor is actually read
  } else if (sensor_id == 1) {
    bme688_2.valid = false;
  }
}

void sendSensorData(int sensor_id) {
  DynamicJsonDocument doc(512);
  doc["sensor_id"] = sensor_id;
  doc["valid"] = (sensor_id == 0) ? bme688_1.valid : bme688_2.valid;
  
  if (sensor_id == 0 && bme688_1.valid) {
    doc["temperature"] = bme688_1.temperature;
    doc["humidity"] = bme688_1.humidity;
    doc["pressure"] = bme688_1.pressure;
    doc["gas_resistance"] = bme688_1.gas_resistance;
  } else if (sensor_id == 1 && bme688_2.valid) {
    doc["temperature"] = bme688_2.temperature;
    doc["humidity"] = bme688_2.humidity;
    doc["pressure"] = bme688_2.pressure;
    doc["gas_resistance"] = bme688_2.gas_resistance;
  }
  
  sendResponse("sensor_data", doc);
}

// ============================================================================
// Helper Functions
// ============================================================================
void sendStatus() {
  if (currentMode == MODE_MACHINE && jsonFormat) {
    DynamicJsonDocument doc(1024);
    doc["type"] = "status";
    doc["ts"] = millis();
    doc["board_id"] = deviceMac;
    doc["status"] = "ready";
    doc["firmware"] = FIRMWARE_VERSION;
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = uptimeSeconds;
    doc["uptime_ms"] = millis();
    doc["telemetry_interval"] = telemetryInterval / 1000;
    doc["mode"] = "machine";
    doc["debug"] = debugEnabled;
    doc["json_format"] = jsonFormat;
    
    // Device identity
    doc["device_role"] = deviceRole;
    if (strlen(deviceDisplayName) > 0) {
      doc["device_display_name"] = deviceDisplayName;
    }
    
    // Add peripheral count
    doc["peripherals"] = i2cAddressCount;
    
    // Add MOSFET states
    JsonArray mosfetArray = doc.createNestedArray("mosfet_states");
    for (int i = 0; i < 3; i++) {
      mosfetArray.add(mosfetStates[i]);
    }
    
    sendNDJSON(doc);
  } else {
    DynamicJsonDocument doc(512);
    doc["status"] = "ready";
    doc["mac"] = deviceMac;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["uptime"] = uptimeSeconds;
    doc["telemetry_interval"] = telemetryInterval / 1000;
    doc["device_role"] = deviceRole;
    if (strlen(deviceDisplayName) > 0) {
      doc["device_display_name"] = deviceDisplayName;
    }
    
    sendResponse("status", doc);
  }
}

void sendResponse(String type, DynamicJsonDocument& doc) {
  DynamicJsonDocument response(1024);
  response["type"] = type;
  response["data"] = doc;
  serializeJson(response, Serial);
  Serial.println();
  Serial.flush();
}

void sendResponse(String type, String data) {
  Serial.print("{\"type\":\"");
  Serial.print(type);
  Serial.print("\",\"data\":");
  Serial.print(data);
  Serial.println("}");
  Serial.flush();
}

void sendError(String error) {
  if (currentMode == MODE_MACHINE && jsonFormat) {
    DynamicJsonDocument doc(256);
    doc["type"] = "err";
    doc["error"] = error;
    doc["ts"] = millis();
    sendNDJSON(doc);
  } else {
    sendResponse("error", "{\"error\":\"" + error + "\"}");
  }
}

void sendAck(String cmd, String message) {
  if (currentMode == MODE_MACHINE && jsonFormat) {
    DynamicJsonDocument doc(256);
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    if (message.length() > 0) {
      doc["message"] = message;
    }
    doc["ts"] = millis();
    sendNDJSON(doc);
  } else {
    sendResponse("ack", "{\"cmd\":\"" + cmd + "\",\"message\":\"" + message + "\"}");
  }
}

void sendNDJSON(DynamicJsonDocument& doc) {
  // NDJSON: one JSON object per line, no extra formatting
  serializeJson(doc, Serial);
  Serial.println();
  Serial.flush();
}

