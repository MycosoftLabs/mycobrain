#ifndef TELEMETRY_BROADCAST_H
#define TELEMETRY_BROADCAST_H

#include <ArduinoJson.h>

class TelemetryBroadcast {
public:
  static void begin();
  static void broadcastTelemetry(const JsonObject& telemetry);
  static void broadcastSensors(const JsonObject& sensors);
  
  static void setBroadcastRate(uint32_t minIntervalMs);
  
private:
  static uint32_t lastBroadcastTime;
  static uint32_t minBroadcastInterval;
  static const uint32_t DEFAULT_MIN_INTERVAL = 100; // 10 Hz max
};

#endif // TELEMETRY_BROADCAST_H

