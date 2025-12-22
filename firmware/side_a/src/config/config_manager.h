#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "config_schema.h"
#include <ArduinoJson.h>

class ConfigManager {
public:
  static bool begin();
  static void end();

  // Calibration
  static bool loadCalibration(CalibrationConfig& config);
  static bool saveCalibration(const CalibrationConfig& config);
  static void getDefaultCalibration(CalibrationConfig& config);

  // Pins
  static bool loadPinConfig(PinConfig& config);
  static bool savePinConfig(const PinConfig& config);
  static void getDefaultPinConfig(PinConfig& config);

  // Thresholds
  static bool loadThresholds(ThresholdConfig& config);
  static bool saveThresholds(const ThresholdConfig& config);
  static void getDefaultThresholds(ThresholdConfig& config);

  // WiFi
  static bool loadWiFiConfig(WiFiConfig& config);
  static bool saveWiFiConfig(const WiFiConfig& config);
  static void getDefaultWiFiConfig(WiFiConfig& config);

  // Factory reset
  static bool factoryReset();

  // JSON serialization helpers
  static void calibrationToJson(const CalibrationConfig& config, JsonObject& obj);
  static void pinConfigToJson(const PinConfig& config, JsonObject& obj);
  static void thresholdsToJson(const ThresholdConfig& config, JsonObject& obj);
  static void wifiConfigToJson(const WiFiConfig& config, JsonObject& obj);

  static bool jsonToCalibration(const JsonObject& obj, CalibrationConfig& config);
  static bool jsonToPinConfig(const JsonObject& obj, PinConfig& config);
  static bool jsonToThresholds(const JsonObject& obj, ThresholdConfig& config);
  static bool jsonToWiFiConfig(const JsonObject& obj, WiFiConfig& config);
};

#endif // CONFIG_MANAGER_H

