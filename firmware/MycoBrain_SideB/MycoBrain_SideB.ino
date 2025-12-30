/*
 * MycoBrain Side-B (Router MCU) Firmware
 * ESP32-S3 router for UART communication with Side-A
 * 
 * Features:
 * - UART communication with Side-A
 * - Command routing (PC -> Side-A)
 * - Telemetry forwarding (Side-A -> PC)
 * - Connection monitoring
 * - Status LED
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// ============================================================================
// Configuration
// ============================================================================
#define SERIAL_BAUD 115200
#define UART_TO_SIDEA Serial1
#define UART_BAUD 115200

// UART pins (adjust for your board - typical ESP32-S3)
#define UART_RX 16
#define UART_TX 17

// Status LED (built-in LED or external)
#define LED_PIN 2

// ============================================================================
// Global Variables
// ============================================================================
String deviceMac = "";
String firmwareVersion = "1.0.0-production";
unsigned long uptimeSeconds = 0;
unsigned long lastUptimeUpdate = 0;

String sideACommandBuffer = "";
String uartCommandBuffer = "";

bool sideAConnected = false;
unsigned long lastSideAHeartbeat = 0;
unsigned long sideATimeout = 5000; // 5 seconds

// Boot wait for Serial Monitor
static const uint32_t BOOT_SERIAL_WAIT_MS = 1800;

// ============================================================================
// Setup
// ============================================================================
void setup() {
  // Initialize serial for PC communication
  Serial.begin(SERIAL_BAUD);
  delay(BOOT_SERIAL_WAIT_MS);
  
  // Initialize UART to Side-A
  UART_TO_SIDEA.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  delay(100);
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Get MAC address
  uint64_t chipid = ESP.getEfuseMac();
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(chipid >> 40), (uint8_t)(chipid >> 32),
           (uint8_t)(chipid >> 24), (uint8_t)(chipid >> 16),
           (uint8_t)(chipid >> 8), (uint8_t)chipid);
  deviceMac = String(macStr);
  
  Serial.println("\n=== MycoBrain Side-B (Router) ===");
  Serial.print("MAC: ");
  Serial.println(deviceMac);
  Serial.print("Firmware: ");
  Serial.println(firmwareVersion);
  Serial.println("Waiting for Side-A connection...");
  
  // Blink LED to indicate ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Try to ping Side-A
  pingSideA();
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  // Update uptime
  unsigned long now = millis();
  if (now - lastUptimeUpdate >= 1000) {
    uptimeSeconds = now / 1000;
    lastUptimeUpdate = now;
  }
  
  // Check for commands from PC (Serial)
  handlePCCommands();
  
  // Check for data from Side-A (UART)
  handleSideAData();
  
  // Check Side-A connection
  if (now - lastSideAHeartbeat > sideATimeout) {
    if (sideAConnected) {
      sideAConnected = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("Side-A disconnected");
    }
  } else {
    if (!sideAConnected) {
      sideAConnected = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Side-A connected");
    }
  }
  
  delay(10);
}

// ============================================================================
// PC Command Handling (Serial)
// ============================================================================
void handlePCCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (uartCommandBuffer.length() > 0) {
        processPCCommand(uartCommandBuffer);
        uartCommandBuffer = "";
      }
    } else {
      uartCommandBuffer += c;
    }
  }
}

void processPCCommand(String cmd) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, cmd);
  
  if (error) {
    sendError("Invalid JSON");
    return;
  }
  
  String command = doc["cmd"] | "";
  
  if (command == "ping") {
    sendResponse("ping", "{\"status\":\"ok\",\"side\":\"side-b\"}");
  }
  else if (command == "status") {
    sendStatus("ready");
  }
  else if (command == "get_mac") {
    sendResponse("mac", "{\"mac\":\"" + deviceMac + "\"}");
  }
  else if (command == "get_version") {
    sendResponse("version", "{\"version\":\"" + firmwareVersion + "\"}");
  }
  else if (command == "side_a_status") {
    checkSideAStatus();
  }
  else {
    // Forward command to Side-A
    forwardToSideA(cmd);
  }
}

// ============================================================================
// Side-A Data Handling (UART)
// ============================================================================
void handleSideAData() {
  while (UART_TO_SIDEA.available() > 0) {
    char c = UART_TO_SIDEA.read();
    
    if (c == '\n' || c == '\r') {
      if (sideACommandBuffer.length() > 0) {
        processSideAData(sideACommandBuffer);
        sideACommandBuffer = "";
      }
    } else {
      sideACommandBuffer += c;
    }
  }
}

void processSideAData(String data) {
  // Update heartbeat
  lastSideAHeartbeat = millis();
  sideAConnected = true;
  
  // Forward telemetry/data to PC
  // Check if it's telemetry (contains temperature, humidity, etc.)
  if (data.indexOf("temperature") >= 0 || 
      data.indexOf("humidity") >= 0 ||
      data.indexOf("ai1_voltage") >= 0 ||
      data.indexOf("\"ai1_voltage\"") >= 0 ||
      data.indexOf("type") >= 0) {
    // This is telemetry or response, forward to PC
    Serial.println(data);
  } else {
    // This is a response, forward to PC
    Serial.println(data);
  }
}

// ============================================================================
// Side-A Communication
// ============================================================================
void forwardToSideA(String command) {
  if (sideAConnected) {
    UART_TO_SIDEA.println(command);
    UART_TO_SIDEA.flush();
  } else {
    sendError("Side-A not connected");
  }
}

void pingSideA() {
  UART_TO_SIDEA.println("{\"cmd\":\"ping\"}");
  UART_TO_SIDEA.flush();
  lastSideAHeartbeat = millis();
}

void checkSideAStatus() {
  DynamicJsonDocument doc(512);
  doc["side_a_connected"] = sideAConnected;
  doc["last_heartbeat_ms"] = millis() - lastSideAHeartbeat;
  sendResponse("side_a_status", doc);
}

// ============================================================================
// Helper Functions
// ============================================================================
void sendStatus(String status) {
  DynamicJsonDocument doc(512);
  doc["status"] = status;
  doc["mac"] = deviceMac;
  doc["firmware"] = firmwareVersion;
  doc["uptime"] = uptimeSeconds;
  doc["side_a_connected"] = sideAConnected;
  
  sendResponse("status", doc);
}

void sendResponse(String type, DynamicJsonDocument& doc) {
  DynamicJsonDocument response(1024);
  response["type"] = type;
  response["data"] = doc;
  serializeJson(response, Serial);
  Serial.println();
}

void sendResponse(String type, String data) {
  Serial.print("{\"type\":\"");
  Serial.print(type);
  Serial.print("\",\"data\":");
  Serial.print(data);
  Serial.println("}");
}

void sendError(String error) {
  sendResponse("error", "{\"error\":\"" + error + "\"}");
}
