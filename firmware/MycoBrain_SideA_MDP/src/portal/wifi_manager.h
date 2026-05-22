#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "../config/config_schema.h"
#include <WiFi.h>

class WiFiManager {
public:
  static bool begin(const WiFiConfig& config);
  static void updateConfig(const WiFiConfig& config);
  static void loop();
  
  static IPAddress getAPIP();
  static IPAddress getSTAIP();
  static bool isAPConnected();
  static bool isSTAConnected();
  static int getSTARSSI();
  
  static WiFiConfig getCurrentConfig();
  
private:
  static WiFiConfig currentConfig;
  static bool initialized;
  static unsigned long lastSTAReconnectAttempt;
  static const unsigned long STA_RECONNECT_INTERVAL = 30000; // 30 seconds
};

#endif // WIFI_MANAGER_H

