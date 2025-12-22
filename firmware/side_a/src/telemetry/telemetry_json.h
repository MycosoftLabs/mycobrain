#ifndef TELEMETRY_JSON_H
#define TELEMETRY_JSON_H

#include <ArduinoJson.h>

// TelemetryV1 structure (must match main.cpp)
#pragma pack(push,1)
struct TelemetryV1 {
  uint16_t magic;
  uint8_t  proto;
  uint8_t  msg_type;
  uint32_t seq;
  uint32_t uptime_ms;
  uint16_t ai_counts[4];
  float    ai_volts[4];
  uint8_t mos[3];
  uint8_t mos_rsv;
  uint8_t i2c_count[4];
  uint8_t i2c_addrs[4][16];
  uint8_t bme_addr[4];
  uint8_t bme_chip[4];
  int8_t i2c_sda[4];
  int8_t i2c_scl[4];
  uint8_t reserved[64];
};
#pragma pack(pop)

class TelemetryJSON {
public:
  static void telemetryToJson(const TelemetryV1& telemetry, JsonObject& obj);
  static void telemetryToSimplifiedJson(const TelemetryV1& telemetry, JsonObject& obj);
};

#endif // TELEMETRY_JSON_H

