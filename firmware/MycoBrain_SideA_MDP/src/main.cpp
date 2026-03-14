/*
 * MycoBrain Side A Production Firmware v2.0.0
 * MDP protocol, BSEC2 dual BME688, role-based config (mushroom1/hyphae1)
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>

#include "bsec2.h"
#include "config.h"
#include "mdp_codec.h"

#define USE_EXTERNAL_BLOB 1
#if USE_EXTERNAL_BLOB
  #include "bsec_selectivity.h"
  static const uint8_t* CFG_PTR = bsec_selectivity_config;
  static const uint32_t CFG_LEN = (uint32_t)bsec_selectivity_config_len;
#else
  static const uint8_t* CFG_PTR = nullptr;
  static const uint32_t CFG_LEN = 0;
#endif

#ifndef ARRAY_LEN
  #define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

static const char* FW_VERSION = "side-a-mdp-2.0.0";
static const uint32_t WDT_TIMEOUT_S = 30;
static const uint32_t SERIAL_BAUD = 115200;

// NeoPixel
#define PIXEL_COUNT 1
Adafruit_NeoPixel pixels(PIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// BSEC2 structures
struct AmbReading {
  bool valid = false;
  uint32_t t_ms = 0;
  float tC = NAN;
  float rh = NAN;
  float p_raw = NAN;
  float p_hPa = NAN;
  float gas_ohm = NAN;
  float iaq = NAN;
  float iaqAccuracy = NAN;
  float staticIaq = NAN;
  float co2eq = NAN;
  float voc = NAN;
  float gasClass = NAN;
  float gasProb = NAN;
};

struct SensorSlot {
  const char* name = nullptr;
  uint8_t addr = 0;
  bool present = false;
  Bsec2 bsec;
  uint8_t mem[BSEC_INSTANCE_SIZE];
  bool beginOk = false;
  bool subOk = false;
  AmbReading r;

  void init(const char* n, uint8_t a) {
    name = n;
    addr = a;
  }
};

static SensorSlot S_AMB;
static SensorSlot S_ENV;
static uint8_t bme688_count = 0;

// State
static bool estop_active = false;
static uint32_t tx_seq = 1;
static uint32_t last_stream_ms = 0;
static uint32_t stream_interval_ms = 10000;
static uint8_t cobs_buffer[1024];
static size_t cobs_len = 0;

static float pressureToHpa(float p) {
  if (!isfinite(p) || p <= 0) return NAN;
  if (p > 20000.0f) return p / 100.0f;
  if (p > 2000.0f) return p / 10.0f;
  if (p > 200.0f) return p;
  if (p > 20.0f) return p * 10.0f;
  return p * 1000.0f;
}

static void slotCallbackCommon(SensorSlot& s, const bme68xData data, const bsecOutputs outputs) {
  s.r.valid = true;
  s.r.t_ms = millis();
  s.r.tC = data.temperature;
  s.r.rh = data.humidity;
  s.r.p_raw = (float)data.pressure;
  s.r.p_hPa = pressureToHpa(s.r.p_raw);
  s.r.gas_ohm = (float)data.gas_resistance;
  s.r.iaq = NAN;
  s.r.iaqAccuracy = NAN;
  s.r.staticIaq = NAN;
  s.r.co2eq = NAN;
  s.r.voc = NAN;
  s.r.gasClass = NAN;
  s.r.gasProb = NAN;

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData& o = outputs.output[i];
    switch (o.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        s.r.iaq = o.signal;
        s.r.iaqAccuracy = (float)o.accuracy;
        break;
      case BSEC_OUTPUT_STATIC_IAQ:
        s.r.staticIaq = o.signal;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        s.r.co2eq = o.signal;
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        s.r.voc = o.signal;
        break;
      case BSEC_OUTPUT_GAS_ESTIMATE_1:
      case BSEC_OUTPUT_GAS_ESTIMATE_2:
      case BSEC_OUTPUT_GAS_ESTIMATE_3:
      case BSEC_OUTPUT_GAS_ESTIMATE_4:
        if (o.signal > 0.1f) {
          s.r.gasClass = (float)(o.sensor_id - BSEC_OUTPUT_GAS_ESTIMATE_1);
          s.r.gasProb = o.signal;
        }
        break;
      default:
        break;
    }
  }
}

static void cbAMB(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_AMB, data, outputs);
}

static void cbENV(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_ENV, data, outputs);
}

static bool slotInit(SensorSlot& s) {
  s.present = false;
  s.beginOk = s.subOk = false;
  s.r = AmbReading();

  Wire.beginTransmission(s.addr);
  if (Wire.endTransmission() != 0) return false;
  s.present = true;

  s.bsec.allocateMemory(s.mem);
  if (!s.bsec.begin(s.addr, Wire)) return false;
  s.beginOk = true;

  s.bsec.setTemperatureOffset(0.0f);
  if (CFG_PTR && CFG_LEN) s.bsec.setConfig(CFG_PTR);

  if (s.addr == 0x77) s.bsec.attachCallback(cbAMB);
  if (s.addr == 0x76) s.bsec.attachCallback(cbENV);

  bsecSensor list[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
#if USE_EXTERNAL_BLOB
    BSEC_OUTPUT_GAS_ESTIMATE_1,
    BSEC_OUTPUT_GAS_ESTIMATE_2,
    BSEC_OUTPUT_GAS_ESTIMATE_3,
    BSEC_OUTPUT_GAS_ESTIMATE_4
#endif
  };

  if (!s.bsec.updateSubscription(list, (uint8_t)ARRAY_LEN(list), BSEC_SAMPLE_RATE_LP)) return false;
  s.subOk = true;
  return true;
}

static void initSensors() {
  Wire.end();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);

  S_AMB.init("AMB", 0x77);
  S_ENV.init("ENV", 0x76);
  slotInit(S_AMB);
  slotInit(S_ENV);

  bme688_count = (S_AMB.present ? 1 : 0) + (S_ENV.present ? 1 : 0);
}

// --- MDP framing ---
void send_frame(uint8_t msg_type, uint32_t ack, uint8_t flags, const JsonDocument& payload) {
  uint8_t raw[1024];
  uint8_t encoded[1100];

  MdpHeader hdr{};
  hdr.magic = MDP_MAGIC;
  hdr.version = MDP_VERSION;
  hdr.msg_type = msg_type;
  hdr.seq = tx_seq++;
  hdr.ack = ack;
  hdr.flags = flags;
  hdr.src = EP_SIDE_A;
  hdr.dst = EP_GATEWAY;
  hdr.rsv = 0;

  memcpy(raw, &hdr, sizeof(MdpHeader));
  size_t payload_len = serializeJson(payload, raw + sizeof(MdpHeader), sizeof(raw) - sizeof(MdpHeader) - 2);
  size_t frame_len = sizeof(MdpHeader) + payload_len;

  uint16_t crc = mdp_crc16_ccitt_false(raw, frame_len);
  raw[frame_len] = (uint8_t)(crc & 0xFF);
  raw[frame_len + 1] = (uint8_t)((crc >> 8) & 0xFF);
  frame_len += 2;

  size_t enc_len = mdp_cobs_encode(raw, frame_len, encoded, sizeof(encoded));
  if (enc_len == 0) return;

  Serial.write(encoded, enc_len);
  Serial.write('\0');
  Serial.flush();
}

bool parse_frame(const uint8_t* encoded, size_t len, MdpHeader& hdr, DynamicJsonDocument& payload) {
  uint8_t decoded[1024];
  size_t dec_len = mdp_cobs_decode(encoded, len, decoded, sizeof(decoded));
  if (dec_len < sizeof(MdpHeader) + 2) return false;

  memcpy(&hdr, decoded, sizeof(MdpHeader));
  if (hdr.magic != MDP_MAGIC || hdr.version != MDP_VERSION) return false;

  uint16_t got_crc = (uint16_t)decoded[dec_len - 2] | ((uint16_t)decoded[dec_len - 1] << 8);
  uint16_t expected_crc = mdp_crc16_ccitt_false(decoded, dec_len - 2);
  if (got_crc != expected_crc) return false;

  size_t payload_len = dec_len - sizeof(MdpHeader) - 2;
  return !deserializeJson(payload, decoded + sizeof(MdpHeader), payload_len);
}

void send_ack(uint32_t ack_seq, bool success, const char* message) {
  StaticJsonDocument<192> doc;
  doc["success"] = success;
  doc["message"] = message;
  send_frame(MDP_ACK, ack_seq, success ? IS_ACK : IS_NACK, doc);
}

void send_hello(uint32_t ack_seq = 0) {
  StaticJsonDocument<512> doc;
  doc["device_id"] = String("mycobrain-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  doc["firmware_version"] = FW_VERSION;
  doc["role"] = MYCOBRAIN_DEVICE_ROLE;

  JsonArray sensors = doc.createNestedArray("sensors");
  sensors.add("bme688_a");
  sensors.add("bme688_b");
  sensors.add("ai1");
  sensors.add("ai2");
  sensors.add("ai3");
  sensors.add("ai4");
#if IS_HYPHAE1
  sensors.add("soil_moisture");
#endif

  JsonArray capabilities = doc.createNestedArray("capabilities");
  capabilities.add("i2c");
  capabilities.add("telemetry");
  capabilities.add("led");
  capabilities.add("buzzer");
  capabilities.add("output_control");
  capabilities.add("estop");

  send_frame(MDP_HELLO, ack_seq, 0, doc);
}

void set_outputs_safe() {
  for (int i = 0; i < 3; ++i) digitalWrite(MOSFET_PINS[i], LOW);
  pixels.clear();
  pixels.show();
  ledcWriteTone(0, 0);
}

void send_telemetry(uint32_t ack_seq = 0) {
  StaticJsonDocument<768> doc;
  doc["type"] = "telemetry";
  doc["uptime_s"] = millis() / 1000;
  doc["estop"] = estop_active;

  JsonObject ai = doc.createNestedObject("analog");
  ai["ai1"] = analogRead(AI_PINS[0]);
  ai["ai2"] = analogRead(AI_PINS[1]);
  ai["ai3"] = analogRead(AI_PINS[2]);
  ai["ai4"] = analogRead(AI_PINS[3]);

#if IS_HYPHAE1
  ai["soil_moisture"] = analogRead(SOIL_MOISTURE_ADC_PIN);
#endif

  JsonObject bme = doc.createNestedObject("bme688");
  if (S_AMB.present && S_AMB.r.valid) {
    JsonObject b1 = bme.createNestedObject("a");
    b1["temperature_c"] = S_AMB.r.tC;
    b1["humidity_pct"] = S_AMB.r.rh;
    b1["pressure_hpa"] = S_AMB.r.p_hPa;
    b1["gas_ohm"] = S_AMB.r.gas_ohm;
    if (!isnan(S_AMB.r.iaq)) b1["iaq"] = S_AMB.r.iaq;
    if (!isnan(S_AMB.r.co2eq)) b1["co2_equivalent"] = S_AMB.r.co2eq;
    if (!isnan(S_AMB.r.voc)) b1["voc_equivalent"] = S_AMB.r.voc;
  }
  if (S_ENV.present && S_ENV.r.valid) {
    JsonObject b2 = bme.createNestedObject("b");
    b2["temperature_c"] = S_ENV.r.tC;
    b2["humidity_pct"] = S_ENV.r.rh;
    b2["pressure_hpa"] = S_ENV.r.p_hPa;
    b2["gas_ohm"] = S_ENV.r.gas_ohm;
    if (!isnan(S_ENV.r.iaq)) b2["iaq"] = S_ENV.r.iaq;
    if (!isnan(S_ENV.r.co2eq)) b2["co2_equivalent"] = S_ENV.r.co2eq;
    if (!isnan(S_ENV.r.voc)) b2["voc_equivalent"] = S_ENV.r.voc;
  }

  send_frame(MDP_TELEMETRY, ack_seq, 0, doc);
}

// Buzzer uses LEDC
static bool buzzer_init = false;
static void initBuzzer() {
  if (!buzzer_init) {
    ledcSetup(0, 2000, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    buzzer_init = true;
  }
}

void handle_command(const MdpHeader& hdr, DynamicJsonDocument& payload) {
  const char* cmd = payload["cmd"] | "";
  JsonVariant params = payload["params"];

  if (strcmp(cmd, "hello") == 0) {
    send_hello(hdr.seq);
    return;
  }
  if (strcmp(cmd, "health") == 0 || strcmp(cmd, "read_sensors") == 0) {
    send_telemetry(hdr.seq);
    if (hdr.flags & ACK_REQUESTED) send_ack(hdr.seq, true, "telemetry_sent");
    return;
  }
  if (strcmp(cmd, "stream_sensors") == 0) {
    float rate_hz = params["rate_hz"] | 0.1f;
    if (rate_hz < 0.1f) rate_hz = 0.1f;
    stream_interval_ms = (uint32_t)(1000.0f / rate_hz);
    send_ack(hdr.seq, true, "stream_interval_updated");
    return;
  }
  if (strcmp(cmd, "output_control") == 0) {
    if (estop_active) {
      send_ack(hdr.seq, false, "estop_active");
      return;
    }
    const char* id = params["id"] | "";
    int value = params["value"] | 0;
    if (strncmp(id, "mosfet", 6) == 0) {
      int idx = atoi(id + 6) - 1;
      if (idx >= 0 && idx < 3) {
        digitalWrite(MOSFET_PINS[idx], value ? HIGH : LOW);
        send_ack(hdr.seq, true, "mosfet_updated");
        return;
      }
    } else if (strcmp(id, "buzzer") == 0) {
      initBuzzer();
      int freq = params["freq"] | 1200;
      int dur_ms = params["duration_ms"] | 200;
      ledcWriteTone(0, freq);
      // No blocking delay; ack immediately. Tone will play.
      send_ack(hdr.seq, true, "buzzer_played");
      return;
    } else if (strcmp(id, "neopixel") == 0) {
      if (value == 0) {
        pixels.clear();
      } else {
        uint8_t r = params["r"] | 255;
        uint8_t g = params["g"] | 255;
        uint8_t b = params["b"] | 255;
        pixels.setPixelColor(0, pixels.Color(r, g, b));
      }
      pixels.show();
      send_ack(hdr.seq, true, "neopixel_updated");
      return;
    }
    send_ack(hdr.seq, false, "invalid_output_id");
    return;
  }
  if (strcmp(cmd, "estop") == 0) {
    estop_active = true;
    set_outputs_safe();
    StaticJsonDocument<128> evt;
    evt["event"] = "estop_activated";
    send_frame(MDP_EVENT, hdr.seq, 0, evt);
    send_ack(hdr.seq, true, "estop_activated");
    return;
  }
  if (strcmp(cmd, "clear_estop") == 0) {
    estop_active = false;
    send_ack(hdr.seq, true, "estop_cleared");
    return;
  }
  if (strcmp(cmd, "enable_peripheral") == 0 || strcmp(cmd, "disable_peripheral") == 0) {
    send_ack(hdr.seq, true, "peripheral_state_recorded");
    return;
  }

  send_ack(hdr.seq, false, "unknown_command");
}

void setup() {
#if CONFIG_IDF_TARGET_ESP32
  // Disable brownout reset (ESP32 only; ESP32-S3 does not have this register)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

  Serial.begin(SERIAL_BAUD);
  delay(1200);

  // Watchdog
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  for (int i = 0; i < 3; ++i) {
    pinMode(MOSFET_PINS[i], OUTPUT);
    digitalWrite(MOSFET_PINS[i], LOW);
  }
#if IS_HYPHAE1
  pinMode(SOIL_MOISTURE_ADC_PIN, INPUT);
#endif

  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();

  initSensors();
  send_hello();
}

void loop() {
  esp_task_wdt_reset();

  while (Serial.available() > 0) {
    uint8_t b = (uint8_t)Serial.read();
    if (b == 0x00) {
      if (cobs_len > 0) {
        MdpHeader hdr{};
        DynamicJsonDocument payload(512);
        if (parse_frame(cobs_buffer, cobs_len, hdr, payload) && hdr.msg_type == MDP_COMMAND && hdr.dst == EP_SIDE_A) {
          handle_command(hdr, payload);
        }
      }
      cobs_len = 0;
    } else if (cobs_len < sizeof(cobs_buffer)) {
      cobs_buffer[cobs_len++] = b;
    } else {
      cobs_len = 0;
    }
  }

  // Run BSEC2
  if (S_AMB.present && S_AMB.beginOk) (void)S_AMB.bsec.run();
  if (S_ENV.present && S_ENV.beginOk) (void)S_ENV.bsec.run();

  if (millis() - last_stream_ms >= stream_interval_ms) {
    send_telemetry();
    last_stream_ms = millis();
  }

  delay(5);
}
