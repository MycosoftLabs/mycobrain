#include "telemetry_broadcast.h"
#include "websocket_server.h"

uint32_t TelemetryBroadcast::lastBroadcastTime = 0;
uint32_t TelemetryBroadcast::minBroadcastInterval = 100; // 10 Hz default

void TelemetryBroadcast::begin() {
  lastBroadcastTime = 0;
  minBroadcastInterval = DEFAULT_MIN_INTERVAL;
}

void TelemetryBroadcast::broadcastTelemetry(const JsonObject& telemetry) {
  uint32_t now = millis();
  if (now - lastBroadcastTime < minBroadcastInterval) {
    return; // Rate limit
  }
  
  String jsonStr;
  serializeJson(telemetry, jsonStr);
  WebSocketServer::broadcast(jsonStr);
  lastBroadcastTime = now;
}

void TelemetryBroadcast::broadcastSensors(const JsonObject& sensors) {
  uint32_t now = millis();
  if (now - lastBroadcastTime < minBroadcastInterval) {
    return; // Rate limit
  }
  
  String jsonStr;
  serializeJson(sensors, jsonStr);
  WebSocketServer::broadcast(jsonStr);
  lastBroadcastTime = now;
}

void TelemetryBroadcast::setBroadcastRate(uint32_t minIntervalMs) {
  minBroadcastInterval = minIntervalMs;
}

