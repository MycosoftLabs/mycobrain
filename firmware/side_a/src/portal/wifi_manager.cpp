#include "wifi_manager.h"
#include <esp_wifi.h>

WiFiConfig WiFiManager::currentConfig;
bool WiFiManager::initialized = false;
unsigned long WiFiManager::lastSTAReconnectAttempt = 0;

bool WiFiManager::begin(const WiFiConfig& config) {
  currentConfig = config;
  
  // Configure WiFi mode
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  if (config.wifi_mode == WIFI_MODE_AP_ONLY) {
    WiFi.mode(WIFI_AP);
  } else if (config.wifi_mode == WIFI_MODE_STA_ONLY) {
    WiFi.mode(WIFI_STA);
  } else if (config.wifi_mode == WIFI_MODE_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
  }
  
  // Always start AP
  if (config.wifi_mode == WIFI_MODE_AP_ONLY || config.wifi_mode == WIFI_MODE_AP_STA) {
    if (!WiFi.softAP(config.ap_ssid, config.ap_password)) {
      return false;
    }
    delay(100);
  }
  
  // Start STA if enabled
  if ((config.wifi_mode == WIFI_MODE_STA_ONLY || config.wifi_mode == WIFI_MODE_AP_STA) && 
      config.sta_enabled && strlen(config.sta_ssid) > 0) {
    WiFi.begin(config.sta_ssid, config.sta_password);
    lastSTAReconnectAttempt = millis();
  }
  
  initialized = true;
  return true;
}

void WiFiManager::updateConfig(const WiFiConfig& config) {
  currentConfig = config;
  begin(config); // Reinitialize with new config
}

void WiFiManager::loop() {
  if (!initialized) return;
  
  // Handle STA reconnection if enabled
  if ((currentConfig.wifi_mode == WIFI_MODE_STA_ONLY || currentConfig.wifi_mode == WIFI_MODE_AP_STA) &&
      currentConfig.sta_enabled && strlen(currentConfig.sta_ssid) > 0) {
    
    if (WiFi.status() != WL_CONNECTED) {
      unsigned long now = millis();
      if (now - lastSTAReconnectAttempt >= STA_RECONNECT_INTERVAL) {
        WiFi.disconnect();
        delay(100);
        WiFi.begin(currentConfig.sta_ssid, currentConfig.sta_password);
        lastSTAReconnectAttempt = now;
      }
    }
  }
}

IPAddress WiFiManager::getAPIP() {
  return WiFi.softAPIP();
}

IPAddress WiFiManager::getSTAIP() {
  return WiFi.localIP();
}

bool WiFiManager::isAPConnected() {
  return WiFi.softAPgetStationNum() > 0;
}

bool WiFiManager::isSTAConnected() {
  return WiFi.status() == WL_CONNECTED;
}

int WiFiManager::getSTARSSI() {
  if (isSTAConnected()) {
    return WiFi.RSSI();
  }
  return 0;
}

WiFiConfig WiFiManager::getCurrentConfig() {
  return currentConfig;
}

