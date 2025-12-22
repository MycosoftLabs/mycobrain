#ifndef PORTAL_MANAGER_H
#define PORTAL_MANAGER_H

#include "../config/config_schema.h"

class PortalManager {
public:
  static bool begin();
  static void loop();
  static void stop();
  
  // Telemetry callbacks
  static void updateTelemetry(const void* telemetryData);
  
  // Set current telemetry for HTTP callbacks
  static void setCurrentTelemetry(const void* telemetryData);
  
private:
  static bool initialized;
  static void setupCallbacks();
};

#endif // PORTAL_MANAGER_H

