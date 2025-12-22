#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>

class HTTPServer {
public:
  static bool begin();
  static void loop();
  static void stop();
  
  // Callbacks for telemetry data
  static void setTelemetryCallback(void (*callback)(JsonObject&));
  static void setSensorsCallback(void (*callback)(JsonObject&));
  
private:
  static void* asyncWebServer;
  static void (*telemetryCallback)(JsonObject&);
  static void (*sensorsCallback)(JsonObject&);
  
  static void setupRoutes();
  static void handleNotFound(AsyncWebServerRequest* request);
  static void handleGetTelemetry(AsyncWebServerRequest* request);
  static void handleGetSensors(AsyncWebServerRequest* request);
  static void handleGetWiFiStatus(AsyncWebServerRequest* request);
  static void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  static void handlePostCalibration(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  static void handlePostPins(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  static void handlePostThresholds(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  
  // Rate limiting
  static unsigned long lastRequestTime[10];
  static int requestCount;
  static const unsigned long RATE_LIMIT_WINDOW = 1000; // 1 second
  static const int MAX_REQUESTS_PER_WINDOW = 10;
  
  static bool checkRateLimit();
};

#endif // HTTP_SERVER_H

