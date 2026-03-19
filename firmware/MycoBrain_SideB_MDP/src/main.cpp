#include <Arduino.h>
#include <ArduinoJson.h>
#include "mdp_codec.h"
#include "esp_task_wdt.h"

// Side-B sits between Jetson (gateway endpoint) and Side-A
// UART1: Jetson16 <-> SideB
// UART2: SideA <-> SideB
HardwareSerial JetsonUart(1);
HardwareSerial SideAUart(2);

static const int JETSON_RX = 16;
static const int JETSON_TX = 17;
static const int SIDEA_RX = 18;
static const int SIDEA_TX = 19;
static const int STATUS_LED = 2;

static const char* FW_VERSION = "side-b-mdp-2.1.0";

// OpenClaw JSON bridge: accepts newline-delimited JSON on JetsonUart
// alongside existing MDP binary frames. JSON lines are translated to
// MDP COMMAND frames and forwarded to Side A. Responses from Side A
// are echoed back as JSON on the Jetson UART.
static char json_line_buf[512];
static size_t json_line_len = 0;

#define HEARTBEAT_INTERVAL_MS 5000
#define MAX_QUEUE_DEPTH 32

uint32_t tx_seq = 1;
uint32_t last_heartbeat_ms = 0;
uint32_t last_heartbeat_sent_ms = 0;
bool lora_ready = false;
bool ble_ready = false;
bool wifi_ready = false;
bool sim_ready = false;
int queued_messages = 0;

uint8_t cobs_from_jetson[1024];
size_t cobs_from_jetson_len = 0;
uint8_t cobs_from_sidea[1024];
size_t cobs_from_sidea_len = 0;

void send_frame_to(HardwareSerial& out, uint8_t msg_type, uint8_t src, uint8_t dst, uint32_t ack, uint8_t flags, const JsonDocument& payload) {
  uint8_t raw[1024];
  uint8_t encoded[1100];

  MdpHeader hdr{};
  hdr.magic = MDP_MAGIC;
  hdr.version = MDP_VERSION;
  hdr.msg_type = msg_type;
  hdr.seq = tx_seq++;
  hdr.ack = ack;
  hdr.flags = flags;
  hdr.src = src;
  hdr.dst = dst;

  memcpy(raw, &hdr, sizeof(MdpHeader));
  size_t payload_len = serializeJson(payload, raw + sizeof(MdpHeader), sizeof(raw) - sizeof(MdpHeader) - 2);
  size_t frame_len = sizeof(MdpHeader) + payload_len;
  uint16_t crc = mdp_crc16_ccitt_false(raw, frame_len);
  raw[frame_len] = static_cast<uint8_t>(crc & 0xFF);
  raw[frame_len + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  frame_len += 2;

  size_t enc_len = mdp_cobs_encode(raw, frame_len, encoded, sizeof(encoded));
  if (enc_len == 0) return;
  out.write(encoded, enc_len);
  out.write('\0');
  out.flush();
}

bool parse_frame(const uint8_t* encoded, size_t len, MdpHeader& hdr, DynamicJsonDocument& payload) {
  uint8_t decoded[1024];
  size_t dec_len = mdp_cobs_decode(encoded, len, decoded, sizeof(decoded));
  if (dec_len < sizeof(MdpHeader) + 2) return false;
  memcpy(&hdr, decoded, sizeof(MdpHeader));
  if (hdr.magic != MDP_MAGIC || hdr.version != MDP_VERSION) return false;

  uint16_t got_crc = static_cast<uint16_t>(decoded[dec_len - 2]) | (static_cast<uint16_t>(decoded[dec_len - 1]) << 8);
  uint16_t expected_crc = mdp_crc16_ccitt_false(decoded, dec_len - 2);
  if (got_crc != expected_crc) return false;

  size_t payload_len = dec_len - sizeof(MdpHeader) - 2;
  auto err = deserializeJson(payload, decoded + sizeof(MdpHeader), payload_len);
  return !err;
}

void send_ack_to_jetson(uint32_t ack_seq, bool success, const char* msg) {
  StaticJsonDocument<160> doc;
  doc["success"] = success;
  doc["message"] = msg;
  send_frame_to(JetsonUart, MDP_ACK, EP_SIDE_B, EP_GATEWAY, ack_seq, success ? IS_ACK : IS_NACK, doc);
}

void send_transport_status(uint32_t ack_seq = 0) {
  StaticJsonDocument<256> doc;
  doc["event"] = "transport_status";
  doc["lora_ready"] = lora_ready;
  doc["ble_ready"] = ble_ready;
  doc["wifi_ready"] = wifi_ready;
  doc["sim_ready"] = sim_ready;
  doc["queue_depth"] = queued_messages;
  doc["backpressure"] = (queued_messages >= MAX_QUEUE_DEPTH);
  send_frame_to(JetsonUart, MDP_EVENT, EP_SIDE_B, EP_GATEWAY, ack_seq, 0, doc);
}

void handle_transport_command(const MdpHeader& hdr, DynamicJsonDocument& payload) {
  const char* cmd = payload["cmd"] | "";
  JsonVariant params = payload["params"];

  if (strcmp(cmd, "transport_status") == 0) {
    send_transport_status(hdr.seq);
    if (hdr.flags & ACK_REQUESTED) send_ack_to_jetson(hdr.seq, true, "status_sent");
    return;
  }
  if (strcmp(cmd, "lora_send") == 0) {
    // TODO: integrate RadioLib SX1262 driver; queueing is tracked even before radio is integrated.
    if (queued_messages >= MAX_QUEUE_DEPTH) {
      send_ack_to_jetson(hdr.seq, false, "backpressure_queue_full");
      return;
    }
    if (!params.containsKey("payload")) {
      send_ack_to_jetson(hdr.seq, false, "missing_payload");
      return;
    }
    queued_messages++;
    lora_ready = true;
    queued_messages = max(0, queued_messages - 1);  // stub: simulate send, decrement
    send_ack_to_jetson(hdr.seq, true, "lora_payload_accepted");
    return;
  }
  if (strcmp(cmd, "ble_advertise") == 0) {
    // TODO: integrate RadioLib / ESP32 BLE stack for advertisement
    ble_ready = params["en"] | false;
    send_ack_to_jetson(hdr.seq, true, ble_ready ? "ble_advertising_on" : "ble_advertising_off");
    return;
  }
  if (strcmp(cmd, "wifi_connect") == 0) {
    // TODO: integrate ESP32 WiFi driver
    const char* ssid = params["ssid"] | "";
    wifi_ready = strlen(ssid) > 0;
    send_ack_to_jetson(hdr.seq, wifi_ready, wifi_ready ? "wifi_connect_started" : "missing_ssid");
    return;
  }
  if (strcmp(cmd, "sim_send") == 0) {
    // TODO: integrate SIM7000 cellular modem driver
    const char* payload_data = params["payload"] | "";
    sim_ready = true;
    send_ack_to_jetson(hdr.seq, strlen(payload_data) > 0, strlen(payload_data) > 0 ? "sim_payload_accepted" : "missing_payload");
    return;
  }

  send_ack_to_jetson(hdr.seq, false, "unknown_transport_command");
}

// ---- OpenClaw JSON->MDP Bridge ----
// Accepts: {"tool":"mycobrain","action":"<cmd>","params":{...}}\n
// Also accepts: {"cmd":"<cmd>","params":{...}}\n (direct MDP format)
// Translates to MDP COMMAND frame -> Side A, returns MDP responses as JSON.
void handle_openclaw_json(const char* line, size_t len) {
  DynamicJsonDocument jdoc(512);
  auto err = deserializeJson(jdoc, line, len);
  if (err) {
    JetsonUart.println("{\"error\":\"json_parse_error\"}");
    return;
  }

  // Extract command from OpenClaw tool-call format or direct MDP format
  const char* action = jdoc["action"] | (const char*)nullptr;
  const char* cmd_direct = jdoc["cmd"] | (const char*)nullptr;
  const char* cmd = action ? action : cmd_direct;
  if (!cmd) {
    JetsonUart.println("{\"error\":\"missing_action_or_cmd\"}");
    return;
  }

  // Build MDP command payload
  StaticJsonDocument<384> mdpPayload;
  mdpPayload["cmd"] = cmd;
  if (jdoc.containsKey("params")) {
    mdpPayload["params"] = jdoc["params"];
  } else {
    mdpPayload.createNestedObject("params");
  }

  // Check if this is a Side B transport command
  if (strcmp(cmd, "transport_status") == 0 || strcmp(cmd, "lora_send") == 0 ||
      strcmp(cmd, "ble_advertise") == 0 || strcmp(cmd, "wifi_connect") == 0 ||
      strcmp(cmd, "sim_send") == 0) {
    // Handle locally on Side B
    MdpHeader fakeHdr{};
    fakeHdr.magic = MDP_MAGIC;
    fakeHdr.version = MDP_VERSION;
    fakeHdr.msg_type = MDP_COMMAND;
    fakeHdr.seq = tx_seq++;
    fakeHdr.flags = ACK_REQUESTED;
    fakeHdr.src = EP_GATEWAY;
    fakeHdr.dst = EP_SIDE_B;
    DynamicJsonDocument dp(384);
    dp["cmd"] = cmd;
    if (jdoc.containsKey("params")) dp["params"] = jdoc["params"];
    handle_transport_command(fakeHdr, dp);
    return;
  }

  // Forward to Side A as MDP COMMAND frame
  send_frame_to(SideAUart, MDP_COMMAND, EP_GATEWAY, EP_SIDE_A, 0, ACK_REQUESTED, mdpPayload);

  // Respond with forwarded confirmation (actual response comes async from Side A)
  StaticJsonDocument<128> resp;
  resp["status"] = "forwarded";
  resp["cmd"] = cmd;
  serializeJson(resp, JetsonUart);
  JetsonUart.println();
}

static bool isJsonLine(const uint8_t* buf, size_t len) {
  // Quick check: does it start with '{' and end with '}'?
  if (len < 2) return false;
  size_t start = 0;
  while (start < len && (buf[start] == ' ' || buf[start] == '\t')) start++;
  if (start >= len || buf[start] != '{') return false;
  size_t end = len - 1;
  while (end > start && (buf[end] == ' ' || buf[end] == '\t' || buf[end] == '\r')) end--;
  return buf[end] == '}';
}

void setup() {
  Serial.begin(115200);
  JetsonUart.begin(115200, SERIAL_8N1, JETSON_RX, JETSON_TX);
  SideAUart.begin(115200, SERIAL_8N1, SIDEA_RX, SIDEA_TX);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  StaticJsonDocument<192> hello;
  hello["role"] = "side_b";
  hello["firmware_version"] = FW_VERSION;
  hello["supports"] = "transport_directives";
  send_frame_to(JetsonUart, MDP_HELLO, EP_SIDE_B, EP_GATEWAY, 0, 0, hello);
}

void loop() {
  // Jetson -> SideB frames (MDP binary via 0x00 delimiter + OpenClaw JSON via newline)
  while (JetsonUart.available() > 0) {
    uint8_t b = (uint8_t)JetsonUart.read();
    if (b == 0x00) {
      // MDP binary frame delimiter
      if (cobs_from_jetson_len > 0) {
        MdpHeader hdr{};
        DynamicJsonDocument payload(512);
        if (parse_frame(cobs_from_jetson, cobs_from_jetson_len, hdr, payload)) {
          last_heartbeat_ms = millis();
          if (hdr.msg_type == MDP_COMMAND && hdr.dst == EP_SIDE_B) {
            handle_transport_command(hdr, payload);
          } else if (hdr.msg_type == MDP_COMMAND && hdr.dst == EP_SIDE_A) {
            // Pass-through command to Side A
            SideAUart.write(cobs_from_jetson, cobs_from_jetson_len);
            SideAUart.write('\0');
            SideAUart.flush();
            if (hdr.flags & ACK_REQUESTED) send_ack_to_jetson(hdr.seq, true, "forwarded_to_side_a");
          }
        }
      }
      cobs_from_jetson_len = 0;
    } else if (b == '\n') {
      // Newline: check if accumulated bytes are a JSON line (OpenClaw bridge)
      if (cobs_from_jetson_len > 0 && isJsonLine(cobs_from_jetson, cobs_from_jetson_len)) {
        // OpenClaw JSON command
        cobs_from_jetson[cobs_from_jetson_len] = '\0';
        handle_openclaw_json((const char*)cobs_from_jetson, cobs_from_jetson_len);
        last_heartbeat_ms = millis();
      }
      // If not JSON, discard (could be debug text)
      cobs_from_jetson_len = 0;
    } else if (cobs_from_jetson_len < sizeof(cobs_from_jetson)) {
      cobs_from_jetson[cobs_from_jetson_len++] = b;
    } else {
      cobs_from_jetson_len = 0;
    }
  }

  // SideA -> Jetson frames (forward upstream)
  while (SideAUart.available() > 0) {
    uint8_t b = (uint8_t)SideAUart.read();
    if (b == 0x00) {
      if (cobs_from_sidea_len > 0) {
        JetsonUart.write(cobs_from_sidea, cobs_from_sidea_len);
        JetsonUart.write('\0');
        JetsonUart.flush();
      }
      cobs_from_sidea_len = 0;
    } else if (cobs_from_sidea_len < sizeof(cobs_from_sidea)) {
      cobs_from_sidea[cobs_from_sidea_len++] = b;
    } else {
      cobs_from_sidea_len = 0;
    }
  }

  if (millis() - last_heartbeat_sent_ms >= HEARTBEAT_INTERVAL_MS) {
    send_transport_status(0);
    last_heartbeat_sent_ms = millis();
  }

  digitalWrite(STATUS_LED, (millis() - last_heartbeat_ms) < 5000 ? HIGH : LOW);
  esp_task_wdt_reset();
  delay(5);
}
