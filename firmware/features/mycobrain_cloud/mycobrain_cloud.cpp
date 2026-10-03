#include "mycobrain_cloud.h"

#include <PubSubClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>
#include <mbedtls/base64.h>

namespace mycocloud {
namespace {

constexpr const char* kNvsNamespace = "mycocloud";
constexpr uint16_t kKeepAliveS = 60;
constexpr uint32_t kBackoffMinMs = 1000;
constexpr uint32_t kBackoffMaxMs = 5UL * 60UL * 1000UL;
constexpr size_t kMaxMdp = 900;
constexpr size_t kRingSlots = 32;
constexpr size_t kMqttBuffer = 1536;  // base64(900) + JSON envelope
constexpr size_t kMaxPem = 2048;

struct Config {
  String endpoint;
  String thing;
  uint16_t port = 443;
  String ca;
  String cert;
  String key;
};

struct Slot {
  uint16_t len = 0;
  uint8_t data[kMaxMdp];
};

Config cfg;
bool provisioned = false;
CommandHandler commandHandler = nullptr;
const char* fwVersion = "unknown";

WiFiClientSecure tls;
PubSubClient mqtt(tls);

String topicBase;
String topicPresence;
String topicFrames;
String topicCmd;

uint32_t backoffMs = kBackoffMinMs;
uint32_t nextAttemptMs = 0;
bool wasConnected = false;

Slot ring[kRingSlots];
size_t ringHead = 0;   // next slot to pop
size_t ringCount = 0;

// Serial PEM capture state
String pemTarget;
String pemBuffer;

const char* kAlpn[] = {"x-amzn-mqtt-ca", nullptr};

void loadConfig() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, true);
  cfg.endpoint = prefs.getString("endpoint", "");
  cfg.thing = prefs.getString("thing", "");
  cfg.port = prefs.getUShort("port", 443);
  cfg.ca = prefs.getString("ca", "");
  cfg.cert = prefs.getString("cert", "");
  cfg.key = prefs.getString("key", "");
  prefs.end();
  provisioned = cfg.endpoint.length() && cfg.thing.length() && cfg.ca.length() &&
                cfg.cert.length() && cfg.key.length();
  topicBase = "mycosoft/devices/" + cfg.thing;
  topicPresence = topicBase + "/presence";
  topicFrames = topicBase + "/mdp/frames";
  topicCmd = topicBase + "/cmd";
}

void saveString(const char* key, const String& value) {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.putString(key, value);
  prefs.end();
}

bool b64Encode(const uint8_t* in, size_t len, String& out) {
  size_t olen = 0;
  static unsigned char buf[((kMaxMdp + 2) / 3) * 4 + 1];
  if (mbedtls_base64_encode(buf, sizeof(buf), &olen, in, len) != 0) return false;
  buf[olen] = 0;
  out = reinterpret_cast<const char*>(buf);
  return true;
}

void onMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (!commandHandler || String(topic) != topicCmd) return;
  // Expect {"frame_b64":"..."}; parse without a JSON dependency.
  String body(reinterpret_cast<const char*>(payload), length);
  int key = body.indexOf("\"frame_b64\"");
  if (key < 0) return;
  int start = body.indexOf('"', body.indexOf(':', key) + 1);
  int end = body.indexOf('"', start + 1);
  if (start < 0 || end <= start) return;
  String b64 = body.substring(start + 1, end);
  static uint8_t mdp[kMaxMdp];
  size_t olen = 0;
  if (mbedtls_base64_decode(mdp, sizeof(mdp), &olen,
                            reinterpret_cast<const unsigned char*>(b64.c_str()), b64.length()) != 0) {
    Serial.println("{\"cloud\":\"cmd_b64_invalid\"}");
    return;
  }
  commandHandler(mdp, static_cast<uint16_t>(olen));
}

void publishPresence() {
  String doc = "{\"status\":\"online\",\"device_id\":\"" + cfg.thing +
               "\",\"firmware_version\":\"" + fwVersion +
               "\",\"board_type\":\"esp32s3\",\"device_role\":\"side_b\",\"ip\":\"" +
               WiFi.localIP().toString() + "\",\"rssi_dbm\":" + String(WiFi.RSSI()) + "}";
  mqtt.publish(topicPresence.c_str(), doc.c_str(), true);
}

bool publishNow(const uint8_t* mdp, uint16_t len) {
  String b64;
  if (!b64Encode(mdp, len, b64)) return false;
  String doc = "{\"frame_b64\":\"" + b64 + "\"}";
  return mqtt.publish(topicFrames.c_str(), doc.c_str(), false);
}

void ringPush(const uint8_t* mdp, uint16_t len) {
  if (ringCount == kRingSlots) {  // drop oldest
    ringHead = (ringHead + 1) % kRingSlots;
    ringCount--;
  }
  Slot& slot = ring[(ringHead + ringCount) % kRingSlots];
  memcpy(slot.data, mdp, len);
  slot.len = len;
  ringCount++;
}

void ringFlush() {
  while (ringCount && mqtt.connected()) {
    Slot& slot = ring[ringHead];
    if (!publishNow(slot.data, slot.len)) return;
    ringHead = (ringHead + 1) % kRingSlots;
    ringCount--;
    mqtt.loop();
  }
}

bool tryConnect() {
  tls.stop();
  tls.setCACert(cfg.ca.c_str());
  tls.setCertificate(cfg.cert.c_str());
  tls.setPrivateKey(cfg.key.c_str());
  tls.setHandshakeTimeout(15);
  if (cfg.port == 443) tls.setAlpnProtocols(kAlpn);
  mqtt.setServer(cfg.endpoint.c_str(), cfg.port);
  const char* lwt = "{\"status\":\"offline\"}";
  if (!mqtt.connect(cfg.thing.c_str(), nullptr, nullptr, topicPresence.c_str(), 1, true, lwt)) {
    Serial.print("{\"cloud\":\"connect_fail\",\"state\":");
    Serial.print(mqtt.state());
    Serial.println("}");
    return false;
  }
  mqtt.subscribe(topicCmd.c_str(), 1);
  publishPresence();
  Serial.print("{\"cloud\":\"connected\",\"port\":");
  Serial.print(cfg.port);
  Serial.println("}");
  return true;
}

void printStatus() {
  Serial.print("{\"cloud\":\"status\",\"provisioned\":");
  Serial.print(provisioned ? "true" : "false");
  Serial.print(",\"thing\":\"");
  Serial.print(cfg.thing);
  Serial.print("\",\"endpoint\":\"");
  Serial.print(cfg.endpoint);
  Serial.print("\",\"port\":");
  Serial.print(cfg.port);
  Serial.print(",\"connected\":");
  Serial.print(mqtt.connected() ? "true" : "false");
  Serial.print(",\"buffered\":");
  Serial.print(ringCount);
  Serial.println("}");
}

}  // namespace

bool begin(CommandHandler onCommand, const char* firmwareVersion) {
  commandHandler = onCommand;
  fwVersion = firmwareVersion;
  loadConfig();
  mqtt.setBufferSize(kMqttBuffer);
  mqtt.setKeepAlive(kKeepAliveS);
  mqtt.setSocketTimeout(15);
  mqtt.setCallback(onMessage);
  printStatus();
  return provisioned;
}

void loop(uint32_t nowMs) {
  if (!provisioned) return;
  if (mqtt.connected()) {
    mqtt.loop();
    ringFlush();
    return;
  }
  if (wasConnected) {
    wasConnected = false;
    Serial.println("{\"cloud\":\"disconnected\"}");
  }
  if (WiFi.status() != WL_CONNECTED) return;
  if (static_cast<int32_t>(nowMs - nextAttemptMs) < 0) return;
  if (tryConnect()) {
    wasConnected = true;
    backoffMs = kBackoffMinMs;
    ringFlush();
    return;
  }
  uint32_t jitter = esp_random() % (backoffMs / 2 + 1);
  nextAttemptMs = nowMs + backoffMs + jitter;
  backoffMs = min(backoffMs * 2, kBackoffMaxMs);
}

bool publishMdp(const uint8_t* mdp, uint16_t len) {
  if (!provisioned || len == 0 || len > kMaxMdp) return false;
  if (mqtt.connected() && ringCount == 0 && publishNow(mdp, len)) return true;
  ringPush(mdp, len);
  return false;
}

bool isProvisioned() { return provisioned; }
bool isConnected() { return mqtt.connected(); }

bool handleSerialLine(const char* rawLine) {
  String line(rawLine);
  line.trim();

  if (pemTarget.length()) {
    if (line == "cloud pem end") {
      saveString(pemTarget.c_str(), pemBuffer);
      Serial.print("{\"cloud\":\"pem_saved\",\"slot\":\"");
      Serial.print(pemTarget);
      Serial.println("\"}");
      pemTarget = "";
      pemBuffer = "";
      loadConfig();
      return true;
    }
    if (pemBuffer.length() + line.length() + 1 > kMaxPem) {
      Serial.println("{\"cloud\":\"pem_too_large\"}");
      pemTarget = "";
      pemBuffer = "";
      return true;
    }
    pemBuffer += line;
    pemBuffer += '\n';
    return true;
  }

  if (!line.startsWith("cloud")) return false;

  if (line == "cloud status") {
    printStatus();
  } else if (line.startsWith("cloud set endpoint ")) {
    saveString("endpoint", line.substring(19));
    loadConfig();
    printStatus();
  } else if (line.startsWith("cloud set thing ")) {
    saveString("thing", line.substring(16));
    loadConfig();
    printStatus();
  } else if (line.startsWith("cloud set port ")) {
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    prefs.putUShort("port", static_cast<uint16_t>(line.substring(15).toInt()));
    prefs.end();
    loadConfig();
    printStatus();
  } else if (line == "cloud pem ca" || line == "cloud pem cert" || line == "cloud pem key") {
    pemTarget = line.substring(10);
    pemBuffer = "";
    Serial.println("{\"cloud\":\"pem_ready\"}");
  } else if (line == "cloud clear") {
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    prefs.clear();
    prefs.end();
    mqtt.disconnect();
    loadConfig();
    printStatus();
  } else {
    Serial.println("{\"cloud\":\"unknown_command\"}");
  }
  return true;
}

}  // namespace mycocloud
