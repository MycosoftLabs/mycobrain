#include "telemetry_json.h"

void TelemetryJSON::telemetryToJson(const TelemetryV1& telemetry, JsonObject& obj) {
  obj["magic"] = telemetry.magic;
  obj["proto"] = telemetry.proto;
  obj["msg_type"] = telemetry.msg_type;
  obj["seq"] = telemetry.seq;
  obj["uptime_ms"] = telemetry.uptime_ms;
  
  JsonArray aiCounts = obj.createNestedArray("ai_counts");
  JsonArray aiVolts = obj.createNestedArray("ai_volts");
  for (int i = 0; i < 4; i++) {
    aiCounts.add(telemetry.ai_counts[i]);
    aiVolts.add(telemetry.ai_volts[i]);
  }
  
  JsonArray mos = obj.createNestedArray("mos");
  for (int i = 0; i < 3; i++) {
    mos.add(telemetry.mos[i]);
  }
  
  JsonArray i2cBuses = obj.createNestedArray("i2c_buses");
  for (int b = 0; b < 4; b++) {
    JsonObject bus = i2cBuses.createNestedObject();
    bus["count"] = telemetry.i2c_count[b];
    
    JsonArray addrs = bus.createNestedArray("addrs");
    for (int i = 0; i < 16; i++) {
      if (i < telemetry.i2c_count[b]) {
        addrs.add(telemetry.i2c_addrs[b][i]);
      }
    }
    
    bus["bme_addr"] = telemetry.bme_addr[b];
    bus["bme_chip"] = telemetry.bme_chip[b];
    bus["sda"] = telemetry.i2c_sda[b];
    bus["scl"] = telemetry.i2c_scl[b];
  }
}

void TelemetryJSON::telemetryToSimplifiedJson(const TelemetryV1& telemetry, JsonObject& obj) {
  obj["uptime_ms"] = telemetry.uptime_ms;
  
  JsonArray aiVolts = obj.createNestedArray("ai_volts");
  for (int i = 0; i < 4; i++) {
    aiVolts.add(telemetry.ai_volts[i]);
  }
  
  JsonArray mos = obj.createNestedArray("mos");
  for (int i = 0; i < 3; i++) {
    mos.add(telemetry.mos[i]);
  }
  
  // Include BME if present
  bool hasBme = false;
  for (int b = 0; b < 4; b++) {
    if (telemetry.bme_addr[b] != 0) {
      hasBme = true;
      JsonObject bme = obj.createNestedObject("bme");
      bme["bus"] = b;
      bme["addr"] = telemetry.bme_addr[b];
      bme["chip"] = telemetry.bme_chip[b];
      break;
    }
  }
  if (!hasBme) {
    obj["bme"] = nullptr;
  }
}

