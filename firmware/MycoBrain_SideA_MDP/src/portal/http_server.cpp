#include "http_server.h"
#include "websocket_server.h"
#include "wifi_manager.h"
#include "../config/config_manager.h"
#include "../config/config_schema.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <FS.h>

void* HTTPServer::asyncWebServer = nullptr;
void (*HTTPServer::telemetryCallback)(JsonObject&) = nullptr;
void (*HTTPServer::sensorsCallback)(JsonObject&) = nullptr;

unsigned long HTTPServer::lastRequestTime[10] = {0};
int HTTPServer::requestCount = 0;

bool HTTPServer::checkRateLimit() {
  unsigned long now = millis();
  
  // Remove old entries outside the window
  int validCount = 0;
  for (int i = 0; i < requestCount; i++) {
    if (now - lastRequestTime[i] < RATE_LIMIT_WINDOW) {
      lastRequestTime[validCount++] = lastRequestTime[i];
    }
  }
  requestCount = validCount;
  
  // Check if we're over the limit
  if (requestCount >= MAX_REQUESTS_PER_WINDOW) {
    return false;
  }
  
  // Add current request
  lastRequestTime[requestCount++] = now;
  return true;
}

bool HTTPServer::begin() {
  AsyncWebServer* server = new AsyncWebServer(80);
  asyncWebServer = server;
  
  // Serve static files from LittleFS
  server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // API routes
  server->on("/api/telemetry", HTTP_GET, handleGetTelemetry);
  server->on("/api/sensors", HTTP_GET, handleGetSensors);
  server->on("/api/wifi/status", HTTP_GET, handleGetWiFiStatus);
  server->on("/api/wifi/config", HTTP_POST, nullptr, nullptr, handlePostWiFiConfig);
  server->on("/api/config/calibration", HTTP_POST, nullptr, nullptr, handlePostCalibration);
  server->on("/api/config/pins", HTTP_POST, nullptr, nullptr, handlePostPins);
  server->on("/api/config/thresholds", HTTP_POST, nullptr, nullptr, handlePostThresholds);
  
  // WebSocket
  AsyncWebSocket* ws = new AsyncWebSocket("/ws");
  WebSocketServer::begin(ws);
  server->addHandler(ws);
  
  // 404 handler
  server->onNotFound(handleNotFound);
  
  server->begin();
  return true;
}

void HTTPServer::loop() {
  // AsyncWebServer handles requests asynchronously
  // No blocking operations needed
}

void HTTPServer::stop() {
  if (asyncWebServer) {
    AsyncWebServer* server = (AsyncWebServer*)asyncWebServer;
    server->end();
    delete server;
    asyncWebServer = nullptr;
  }
}

void HTTPServer::setTelemetryCallback(void (*callback)(JsonObject&)) {
  telemetryCallback = callback;
}

void HTTPServer::setSensorsCallback(void (*callback)(JsonObject&)) {
  sensorsCallback = callback;
}

void HTTPServer::handleNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

void HTTPServer::handleGetTelemetry(AsyncWebServerRequest* request) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<2048> doc;
  JsonObject obj = doc.to<JsonObject>();
  
  if (telemetryCallback) {
    telemetryCallback(obj);
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void HTTPServer::handleGetSensors(AsyncWebServerRequest* request) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<512> doc;
  JsonObject obj = doc.to<JsonObject>();
  
  if (sensorsCallback) {
    sensorsCallback(obj);
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void HTTPServer::handleGetWiFiStatus(AsyncWebServerRequest* request) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  JsonObject obj = doc.to<JsonObject>();
  
  IPAddress apIP = WiFiManager::getAPIP();
  IPAddress staIP = WiFiManager::getSTAIP();
  
  obj["ap_ip"] = apIP.toString();
  obj["ap_connected"] = WiFiManager::isAPConnected();
  obj["sta_ip"] = staIP.toString();
  obj["sta_connected"] = WiFiManager::isSTAConnected();
  obj["sta_rssi"] = WiFiManager::getSTARSSI();
  
  WiFiConfig config = WiFiManager::getCurrentConfig();
  obj["wifi_mode"] = config.wifi_mode;
  obj["ap_ssid"] = config.ap_ssid;
  obj["sta_enabled"] = config.sta_enabled;
  if (config.sta_enabled) {
    obj["sta_ssid"] = config.sta_ssid;
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void HTTPServer::handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }
  
  WiFiConfig config;
  if (!ConfigManager::jsonToWiFiConfig(doc.as<JsonObject>(), config)) {
    request->send(400, "application/json", "{\"error\":\"invalid_config\"}");
    return;
  }
  
  // Validate WiFi mode
  if (config.wifi_mode > WIFI_MODE_AP_STA) {
    request->send(400, "application/json", "{\"error\":\"invalid_wifi_mode\"}");
    return;
  }
  
  // Validate SSID lengths
  if (strlen(config.ap_ssid) == 0 || strlen(config.ap_ssid) > 32) {
    request->send(400, "application/json", "{\"error\":\"invalid_ap_ssid\"}");
    return;
  }
  
  if (config.sta_enabled && (strlen(config.sta_ssid) == 0 || strlen(config.sta_ssid) > 32)) {
    request->send(400, "application/json", "{\"error\":\"invalid_sta_ssid\"}");
    return;
  }
  
  if (!ConfigManager::saveWiFiConfig(config)) {
    request->send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }
  
  WiFiManager::updateConfig(config);
  
  request->send(200, "application/json", "{\"status\":\"ok\",\"reboot_required\":true}");
}

void HTTPServer::handlePostCalibration(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }
  
  CalibrationConfig config;
  if (!ConfigManager::jsonToCalibration(doc.as<JsonObject>(), config)) {
    request->send(400, "application/json", "{\"error\":\"invalid_config\"}");
    return;
  }
  
  // Validate ranges
  for (int i = 0; i < 4; i++) {
    if (config.analog_gain[i] < 0.1f || config.analog_gain[i] > 10.0f) {
      request->send(400, "application/json", "{\"error\":\"invalid_gain\"}");
      return;
    }
  }
  
  if (config.adc_reference < 1.0f || config.adc_reference > 5.0f) {
    request->send(400, "application/json", "{\"error\":\"invalid_adc_ref\"}");
    return;
  }
  
  if (!ConfigManager::saveCalibration(config)) {
    request->send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }
  
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void HTTPServer::handlePostPins(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }
  
  PinConfig config;
  if (!ConfigManager::jsonToPinConfig(doc.as<JsonObject>(), config)) {
    request->send(400, "application/json", "{\"error\":\"invalid_config\"}");
    return;
  }
  
  // Validate GPIO pins (ESP32-S3 has GPIO 0-48, but some are reserved)
  // Basic validation: check if pins are in valid range
  for (int i = 0; i < 4; i++) {
    if (config.ai_pins[i] < 0 || config.ai_pins[i] > 48) {
      request->send(400, "application/json", "{\"error\":\"invalid_ai_pin\"}");
      return;
    }
  }
  
  for (int i = 0; i < 3; i++) {
    if (config.mos_pins[i] < 0 || config.mos_pins[i] > 48) {
      request->send(400, "application/json", "{\"error\":\"invalid_mos_pin\"}");
      return;
    }
  }
  
  if (config.i2c_sda < 0 || config.i2c_sda > 48 || config.i2c_scl < 0 || config.i2c_scl > 48) {
    request->send(400, "application/json", "{\"error\":\"invalid_i2c_pin\"}");
    return;
  }
  
  if (!ConfigManager::savePinConfig(config)) {
    request->send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }
  
  request->send(200, "application/json", "{\"status\":\"ok\",\"reboot_required\":true}");
}

void HTTPServer::handlePostThresholds(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!checkRateLimit()) {
    request->send(429, "application/json", "{\"error\":\"rate_limit\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }
  
  ThresholdConfig config;
  if (!ConfigManager::jsonToThresholds(doc.as<JsonObject>(), config)) {
    request->send(400, "application/json", "{\"error\":\"invalid_config\"}");
    return;
  }
  
  // Validate thresholds (high > low)
  for (int i = 0; i < 4; i++) {
    if (config.analog_high[i] <= config.analog_low[i]) {
      request->send(400, "application/json", "{\"error\":\"invalid_thresholds\"}");
      return;
    }
  }
  
  if (!ConfigManager::saveThresholds(config)) {
    request->send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }
  
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}

