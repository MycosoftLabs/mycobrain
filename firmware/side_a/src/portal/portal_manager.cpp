#include "portal_manager.h"
#include "wifi_manager.h"
#include "dns_server.h"
#include "http_server.h"
#include "websocket_server.h"
#include "telemetry_broadcast.h"
#include "../config/config_manager.h"
#include "../telemetry/telemetry_json.h"
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>

bool PortalManager::initialized = false;

bool PortalManager::begin() {
  if (initialized) return true;
  
  // Initialize LittleFS for web files
  if (!LittleFS.begin(true)) {
    return false;
  }
  
  // Load WiFi config
  WiFiConfig wifiConfig;
  ConfigManager::loadWiFiConfig(wifiConfig);
  
  // Initialize WiFi
  if (!WiFiManager::begin(wifiConfig)) {
    return false;
  }
  
  // Start DNS server (for captive portal)
  IPAddress apIP = WiFiManager::getAPIP();
  if (apIP != IPAddress(0, 0, 0, 0)) {
    DNSServerManager::begin(apIP);
  }
  
  // Setup HTTP server callbacks
  setupCallbacks();
  
  // Start HTTP server
  if (!HTTPServer::begin()) {
    return false;
  }
  
  // Initialize telemetry broadcast
  TelemetryBroadcast::begin();
  
  initialized = true;
  return true;
}

void PortalManager::loop() {
  if (!initialized) return;
  
  // Process DNS server (non-blocking)
  DNSServerManager::loop();
  
  // Process WiFi manager (handles STA reconnection)
  WiFiManager::loop();
  
  // HTTP server is async, no blocking operations needed
  HTTPServer::loop();
  
  // WebSocket server is async
  WebSocketServer::loop();
}

void PortalManager::stop() {
  if (!initialized) return;
  
  HTTPServer::stop();
  DNSServerManager::stop();
  WiFiManager::updateConfig(WiFiConfig()); // Reset WiFi
  
  initialized = false;
}

// Global telemetry storage for HTTP callbacks
// Note: TelemetryV1 is defined in telemetry_json.h
static const TelemetryV1* g_currentTelemetry = nullptr;

void PortalManager::setupCallbacks() {
  // Set up telemetry callback for HTTP endpoints
  HTTPServer::setTelemetryCallback([](JsonObject& obj) {
    if (g_currentTelemetry) {
      TelemetryJSON::telemetryToJson(*g_currentTelemetry, obj);
    }
  });
  
  HTTPServer::setSensorsCallback([](JsonObject& obj) {
    if (g_currentTelemetry) {
      TelemetryJSON::telemetryToSimplifiedJson(*g_currentTelemetry, obj);
    }
  });
}

void PortalManager::setCurrentTelemetry(const void* telemetryData) {
  g_currentTelemetry = (const TelemetryV1*)telemetryData;
}

void PortalManager::updateTelemetry(const void* telemetryData) {
  if (!initialized) return;
  
  // Store for HTTP callbacks
  setCurrentTelemetry(telemetryData);
  
  // Convert telemetry to JSON and broadcast via WebSocket
  const TelemetryV1* telemetry = (const TelemetryV1*)telemetryData;
  
  StaticJsonDocument<512> simpleDoc;
  JsonObject simpleObj = simpleDoc.to<JsonObject>();
  TelemetryJSON::telemetryToSimplifiedJson(*telemetry, simpleObj);
  TelemetryBroadcast::broadcastSensors(simpleObj);
}

