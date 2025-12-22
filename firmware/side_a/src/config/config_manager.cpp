#include "config_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static const char* NVS_NAMESPACE = "mycobrain_a";

bool ConfigManager::begin() {
  return prefs.begin(NVS_NAMESPACE, false);
}

void ConfigManager::end() {
  prefs.end();
}

// Calibration
bool ConfigManager::loadCalibration(CalibrationConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  
  String jsonStr = prefs.getString("calib", "");
  prefs.end();
  
  if (jsonStr.length() == 0) {
    getDefaultCalibration(config);
    return false;
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
    getDefaultCalibration(config);
    return false;
  }

  return jsonToCalibration(doc.as<JsonObject>(), config);
}

bool ConfigManager::saveCalibration(const CalibrationConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  StaticJsonDocument<512> doc;
  JsonObject obj = doc.to<JsonObject>();
  calibrationToJson(config, obj);

  String jsonStr;
  serializeJson(doc, jsonStr);
  bool result = prefs.putString("calib", jsonStr);
  prefs.end();
  return result;
}

void ConfigManager::getDefaultCalibration(CalibrationConfig& config) {
  for (int i = 0; i < 4; i++) {
    config.analog_offset[i] = 0.0f;
    config.analog_gain[i] = 1.0f;
  }
  config.adc_reference = 3.3f;
  config.bme_temp_offset = 0.0f;
  config.bme_humidity_offset = 0.0f;
}

// Pins
bool ConfigManager::loadPinConfig(PinConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  
  String jsonStr = prefs.getString("pins", "");
  prefs.end();
  
  if (jsonStr.length() == 0) {
    getDefaultPinConfig(config);
    return false;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
    getDefaultPinConfig(config);
    return false;
  }

  return jsonToPinConfig(doc.as<JsonObject>(), config);
}

bool ConfigManager::savePinConfig(const PinConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  StaticJsonDocument<256> doc;
  JsonObject obj = doc.to<JsonObject>();
  pinConfigToJson(config, obj);

  String jsonStr;
  serializeJson(doc, jsonStr);
  bool result = prefs.putString("pins", jsonStr);
  prefs.end();
  return result;
}

void ConfigManager::getDefaultPinConfig(PinConfig& config) {
  config.ai_pins[0] = 6;
  config.ai_pins[1] = 7;
  config.ai_pins[2] = 10;
  config.ai_pins[3] = 11;
  config.mos_pins[0] = 12;
  config.mos_pins[1] = 13;
  config.mos_pins[2] = 14;
  config.i2c_sda = 4;
  config.i2c_scl = 5;
}

// Thresholds
bool ConfigManager::loadThresholds(ThresholdConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  
  String jsonStr = prefs.getString("thresholds", "");
  prefs.end();
  
  if (jsonStr.length() == 0) {
    getDefaultThresholds(config);
    return false;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
    getDefaultThresholds(config);
    return false;
  }

  return jsonToThresholds(doc.as<JsonObject>(), config);
}

bool ConfigManager::saveThresholds(const ThresholdConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  StaticJsonDocument<256> doc;
  JsonObject obj = doc.to<JsonObject>();
  thresholdsToJson(config, obj);

  String jsonStr;
  serializeJson(doc, jsonStr);
  bool result = prefs.putString("thresholds", jsonStr);
  prefs.end();
  return result;
}

void ConfigManager::getDefaultThresholds(ThresholdConfig& config) {
  for (int i = 0; i < 4; i++) {
    config.analog_high[i] = 3.0f;
    config.analog_low[i] = 0.1f;
  }
}

// WiFi
bool ConfigManager::loadWiFiConfig(WiFiConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  
  String jsonStr = prefs.getString("wifi", "");
  prefs.end();
  
  if (jsonStr.length() == 0) {
    getDefaultWiFiConfig(config);
    return false;
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
    getDefaultWiFiConfig(config);
    return false;
  }

  return jsonToWiFiConfig(doc.as<JsonObject>(), config);
}

bool ConfigManager::saveWiFiConfig(const WiFiConfig& config) {
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  StaticJsonDocument<512> doc;
  JsonObject obj = doc.to<JsonObject>();
  wifiConfigToJson(config, obj);

  String jsonStr;
  serializeJson(doc, jsonStr);
  bool result = prefs.putString("wifi", jsonStr);
  prefs.end();
  return result;
}

void ConfigManager::getDefaultWiFiConfig(WiFiConfig& config) {
  // Generate MAC-based SSID
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(config.ap_ssid, sizeof(config.ap_ssid), "MycoBrain-%02X%02X", mac[4], mac[5]);
  strncpy(config.ap_password, "mycobrain", sizeof(config.ap_password) - 1);
  config.ap_password[sizeof(config.ap_password) - 1] = '\0';
  
  config.sta_enabled = false;
  config.sta_ssid[0] = '\0';
  config.sta_password[0] = '\0';
  config.wifi_mode = WIFI_MODE_AP_ONLY;
}

// Factory reset
bool ConfigManager::factoryReset() {
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool result = prefs.clear();
  prefs.end();
  return result;
}

// JSON serialization
void ConfigManager::calibrationToJson(const CalibrationConfig& config, JsonObject& obj) {
  JsonArray offsetArr = obj.createNestedArray("analog_offset");
  JsonArray gainArr = obj.createNestedArray("analog_gain");
  for (int i = 0; i < 4; i++) {
    offsetArr.add(config.analog_offset[i]);
    gainArr.add(config.analog_gain[i]);
  }
  obj["adc_reference"] = config.adc_reference;
  obj["bme_temp_offset"] = config.bme_temp_offset;
  obj["bme_humidity_offset"] = config.bme_humidity_offset;
}

void ConfigManager::pinConfigToJson(const PinConfig& config, JsonObject& obj) {
  JsonArray aiArr = obj.createNestedArray("ai_pins");
  JsonArray mosArr = obj.createNestedArray("mos_pins");
  for (int i = 0; i < 4; i++) aiArr.add(config.ai_pins[i]);
  for (int i = 0; i < 3; i++) mosArr.add(config.mos_pins[i]);
  obj["i2c_sda"] = config.i2c_sda;
  obj["i2c_scl"] = config.i2c_scl;
}

void ConfigManager::thresholdsToJson(const ThresholdConfig& config, JsonObject& obj) {
  JsonArray highArr = obj.createNestedArray("analog_high");
  JsonArray lowArr = obj.createNestedArray("analog_low");
  for (int i = 0; i < 4; i++) {
    highArr.add(config.analog_high[i]);
    lowArr.add(config.analog_low[i]);
  }
}

void ConfigManager::wifiConfigToJson(const WiFiConfig& config, JsonObject& obj) {
  obj["ap_ssid"] = config.ap_ssid;
  obj["ap_password"] = config.ap_password;
  obj["sta_enabled"] = config.sta_enabled;
  obj["sta_ssid"] = config.sta_ssid;
  obj["sta_password"] = config.sta_password;
  obj["wifi_mode"] = config.wifi_mode;
}

bool ConfigManager::jsonToCalibration(const JsonObject& obj, CalibrationConfig& config) {
  if (!obj.containsKey("analog_offset") || !obj.containsKey("analog_gain")) return false;
  
  JsonArray offsetArr = obj["analog_offset"];
  JsonArray gainArr = obj["analog_gain"];
  if (offsetArr.size() != 4 || gainArr.size() != 4) return false;

  for (int i = 0; i < 4; i++) {
    config.analog_offset[i] = offsetArr[i].as<float>();
    config.analog_gain[i] = gainArr[i].as<float>();
  }
  config.adc_reference = obj["adc_reference"] | 3.3f;
  config.bme_temp_offset = obj["bme_temp_offset"] | 0.0f;
  config.bme_humidity_offset = obj["bme_humidity_offset"] | 0.0f;
  return true;
}

bool ConfigManager::jsonToPinConfig(const JsonObject& obj, PinConfig& config) {
  if (!obj.containsKey("ai_pins") || !obj.containsKey("mos_pins")) return false;
  
  JsonArray aiArr = obj["ai_pins"];
  JsonArray mosArr = obj["mos_pins"];
  if (aiArr.size() != 4 || mosArr.size() != 3) return false;

  for (int i = 0; i < 4; i++) config.ai_pins[i] = aiArr[i].as<int8_t>();
  for (int i = 0; i < 3; i++) config.mos_pins[i] = mosArr[i].as<int8_t>();
  config.i2c_sda = obj["i2c_sda"] | 4;
  config.i2c_scl = obj["i2c_scl"] | 5;
  return true;
}

bool ConfigManager::jsonToThresholds(const JsonObject& obj, ThresholdConfig& config) {
  if (!obj.containsKey("analog_high") || !obj.containsKey("analog_low")) return false;
  
  JsonArray highArr = obj["analog_high"];
  JsonArray lowArr = obj["analog_low"];
  if (highArr.size() != 4 || lowArr.size() != 4) return false;

  for (int i = 0; i < 4; i++) {
    config.analog_high[i] = highArr[i].as<float>();
    config.analog_low[i] = lowArr[i].as<float>();
  }
  return true;
}

bool ConfigManager::jsonToWiFiConfig(const JsonObject& obj, WiFiConfig& config) {
  if (!obj.containsKey("ap_ssid")) return false;
  
  strncpy(config.ap_ssid, obj["ap_ssid"] | "MycoBrain", sizeof(config.ap_ssid) - 1);
  config.ap_ssid[sizeof(config.ap_ssid) - 1] = '\0';
  
  strncpy(config.ap_password, obj["ap_password"] | "mycobrain", sizeof(config.ap_password) - 1);
  config.ap_password[sizeof(config.ap_password) - 1] = '\0';
  
  config.sta_enabled = obj["sta_enabled"] | false;
  
  if (obj.containsKey("sta_ssid")) {
    strncpy(config.sta_ssid, obj["sta_ssid"], sizeof(config.sta_ssid) - 1);
    config.sta_ssid[sizeof(config.sta_ssid) - 1] = '\0';
  } else {
    config.sta_ssid[0] = '\0';
  }
  
  if (obj.containsKey("sta_password")) {
    strncpy(config.sta_password, obj["sta_password"], sizeof(config.sta_password) - 1);
    config.sta_password[sizeof(config.sta_password) - 1] = '\0';
  } else {
    config.sta_password[0] = '\0';
  }
  
  config.wifi_mode = obj["wifi_mode"] | WIFI_MODE_AP_ONLY;
  return true;
}

