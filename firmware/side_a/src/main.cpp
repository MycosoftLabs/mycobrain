/***************************************************
  MycoBrain V1 — Side-A Firmware
  MDP v1 + COBS + CRC16 + ACK/Retry + Command Channel

  Transport: UART Serial2 (A <-> B)
  Frame: COBS( payload || crc16 ) + 0x00 delimiter

  Toolchain: Arduino-ESP32 core 2.0.13+
***************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include "portal/portal_manager.h"
#include "portal/wifi_manager.h"
#include "config/config_manager.h"
#include "config/config_schema.h"
#include "config/calibration.h"
#include "telemetry/telemetry_json.h"
#include <ArduinoJson.h>

#define USE_SOFT_I2C 0
#if USE_SOFT_I2C
  #include <SoftwareWire.h>
#endif

// ==============================
//        COBS + CRC16
// ==============================
static size_t cobsEncode(const uint8_t* input, size_t length, uint8_t* output) {
  size_t read_index = 0, write_index = 1, code_index = 0;
  uint8_t code = 1;
  while (read_index < length) {
    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  output[code_index] = code;
  return write_index;
}

static bool cobsDecode(const uint8_t* input, size_t length, uint8_t* output, size_t* outLen) {
  size_t read_index = 0, write_index = 0;
  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code == 0 || read_index + code > length + 1) return false;
    read_index++;
    for (uint8_t i = 1; i < code; i++) output[write_index++] = input[read_index++];
    if (code != 0xFF && read_index < length) output[write_index++] = 0;
  }
  *outLen = write_index;
  return true;
}

static uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

// ==============================
//          CONFIG
// ==============================
namespace cfg {
constexpr uint32_t USB_BAUD  = 115200;

// UART link A<->B
constexpr uint32_t LINK_BAUD = 115200;
// TODO: set to MycoBrain V1 routing
constexpr int PIN_TX2 = 8;
constexpr int PIN_RX2 = 9;

// A-side analog inputs
constexpr int PIN_AI1 = 6;
constexpr int PIN_AI2 = 7;
constexpr int PIN_AI3 = 10;
constexpr int PIN_AI4 = 11;

constexpr float ADC_VREF = 3.3f;
constexpr int ADC_MAX = 4095;

// A-side MOSFET controls (AO pins)
constexpr int PIN_MOS1 = 12;
constexpr int PIN_MOS2 = 13;
constexpr int PIN_MOS3 = 14;

// I2C0 defaults (note: you observed SDA=4 SCL=5 in one run; set accordingly)
constexpr int I2C0_SDA = 4;
constexpr int I2C0_SCL = 5;
constexpr uint32_t I2C_HW_FREQ_HZ = 100000;

// Periods
constexpr uint32_t TELEMETRY_PERIOD_MS = 1000;
constexpr uint32_t I2C_RESCAN_MS       = 5000;

// NVS
constexpr bool USE_NVS = true;
static const char* NVS_NS = "mycobrain_a";

// MDP
constexpr uint16_t MDP_MAGIC = 0xA15A;
constexpr uint8_t  MDP_VER   = 1;

// Endpoints
constexpr uint8_t EP_SIDE_A  = 0xA1;
constexpr uint8_t EP_SIDE_B  = 0xB1;
constexpr uint8_t EP_BCAST   = 0xFF;

// Limits
constexpr size_t MAX_PAYLOAD = 768;     // UART can handle this
constexpr size_t MAX_FRAME   = 1024;    // encoded + crc

// Reliability
constexpr uint32_t RTO_MS = 120;        // UART local link
constexpr uint8_t  MAX_RETRIES = 8;
} // namespace cfg

// ==============================
//        MDP definitions
// ==============================
enum MdpMsgType : uint8_t {
  MDP_TELEMETRY = 0x01,
  MDP_COMMAND   = 0x02,
  MDP_ACK       = 0x03,
  MDP_EVENT     = 0x05,
  MDP_HELLO     = 0x06
};

enum MdpFlags : uint8_t {
  ACK_REQUESTED = 0x01,
  IS_ACK        = 0x02,
  IS_NACK       = 0x04
};

#pragma pack(push,1)
struct mdp_hdr_v1_t {
  uint16_t magic;   // 0xA15A
  uint8_t  version; // 1
  uint8_t  msg_type;
  uint32_t seq;     // sender seq
  uint32_t ack;     // cumulative ack for peer seq
  uint8_t  flags;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  rsv;
};

// Command message
struct mdp_cmd_v1_t {
  mdp_hdr_v1_t hdr;
  uint16_t cmd_id;
  uint16_t cmd_len;
  uint8_t  cmd_data[];
};

// Event: command result
struct mdp_evt_cmd_result_v1_t {
  mdp_hdr_v1_t hdr;
  uint16_t evt_type;   // 0x0001 = CMD_RESULT
  uint16_t evt_len;
  uint16_t cmd_id;
  int16_t  status;     // 0 OK, negative error
  uint8_t  data[];     // optional
};
#pragma pack(pop)

static constexpr uint16_t EVT_CMD_RESULT = 0x0001;

// Commands
static constexpr uint16_t CMD_SET_I2C        = 0x0001; // data: sda(u8), scl(u8), hz(u32 optional)
static constexpr uint16_t CMD_SCAN_I2C       = 0x0002;
static constexpr uint16_t CMD_SET_TELEM_MS   = 0x0003; // data: u32 ms
static constexpr uint16_t CMD_SET_MOS        = 0x0004; // data: idx(u8 1..3), val(u8 0/1)
static constexpr uint16_t CMD_SAVE_NVS       = 0x0007;
static constexpr uint16_t CMD_LOAD_NVS       = 0x0008;
static constexpr uint16_t CMD_REBOOT         = 0x0009;
static constexpr uint16_t CMD_SET_CALIBRATION = 0x000A; // JSON calibration config
static constexpr uint16_t CMD_SET_PINS        = 0x000B; // JSON pin config
static constexpr uint16_t CMD_SET_THRESHOLDS  = 0x000C; // JSON threshold config
static constexpr uint16_t CMD_FACTORY_RESET   = 0x000D; // Factory reset
static constexpr uint16_t CMD_SET_WIFI        = 0x000E; // JSON WiFi config

// ==============================
//      Telemetry body (yours)
// ==============================
#pragma pack(push,1)
struct TelemetryV1 {
  uint16_t magic;         // 0xA15A (kept for legacy / sanity)
  uint8_t  proto;         // 1
  uint8_t  msg_type;      // 1=telemetry

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

// ==============================
//      I2C scan state
// ==============================
struct BusFound { uint8_t addrs[16]; uint8_t count; };
struct BmeCandidate { bool present; uint8_t addr; uint8_t chip_id; };

static TwoWire I2C0 = TwoWire(0);
static BusFound found[4] = {};
static BmeCandidate bme_on_bus[4] = {};

static Preferences prefs;

// Configuration storage
static CalibrationConfig calibConfig;
static PinConfig pinConfig;
static ThresholdConfig thresholdConfig;

static inline float adcCountsToVolts(uint16_t c) { return (float)c * (cfg::ADC_VREF / (float)cfg::ADC_MAX); }

static bool i2cReadReg_HW(TwoWire& bus, uint8_t addr, uint8_t reg, uint8_t& outVal) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom((int)addr, 1) != 1) return false;
  outVal = (uint8_t)bus.read();
  return true;
}

static void clearFound(BusFound& f) { f.count = 0; memset(f.addrs, 0, sizeof(f.addrs)); }
static void addFound(BusFound& f, uint8_t addr) { if (f.count < 16) f.addrs[f.count++] = addr; }

static void scanBus_HW(TwoWire& bus, BusFound& out) {
  clearFound(out);
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) addFound(out, addr);
  }
}

static BmeCandidate findBmeCandidateOnBus_HW(TwoWire& bus, const BusFound& f) {
  constexpr uint8_t REG_CHIP_ID = 0xD0;
  constexpr uint8_t CHIP_ID_BME6XX = 0x61;
  BmeCandidate c{false,0,0};
  for (uint8_t i = 0; i < f.count; i++) {
    uint8_t chip = 0;
    if (i2cReadReg_HW(bus, f.addrs[i], REG_CHIP_ID, chip) && chip == CHIP_ID_BME6XX) {
      c.present = true; c.addr = f.addrs[i]; c.chip_id = chip; return c;
    }
  }
  return c;
}

static void saveScanToNVS() {
  if (!cfg::USE_NVS) return;
  if (!prefs.begin(cfg::NVS_NS, false)) return;
  for (int b = 0; b < 4; b++) {
    char keyCnt[16]; snprintf(keyCnt, sizeof(keyCnt), "b%d_cnt", b);
    prefs.putUChar(keyCnt, found[b].count);
    for (int i = 0; i < 16; i++) {
      char keyA[16]; snprintf(keyA, sizeof(keyA), "b%d_a%02d", b, i);
      prefs.putUChar(keyA, found[b].addrs[i]);
    }
    char kA[16], kC[16]; snprintf(kA, sizeof(kA), "b%d_bme_a", b); snprintf(kC, sizeof(kC), "b%d_bme_c", b);
    prefs.putUChar(kA, bme_on_bus[b].present ? bme_on_bus[b].addr : 0);
    prefs.putUChar(kC, bme_on_bus[b].present ? bme_on_bus[b].chip_id : 0);
  }
  prefs.end();
}

static void loadScanFromNVS() {
  if (!cfg::USE_NVS) return;
  if (!prefs.begin(cfg::NVS_NS, true)) return;
  for (int b = 0; b < 4; b++) {
    char keyCnt[16]; snprintf(keyCnt, sizeof(keyCnt), "b%d_cnt", b);
    found[b].count = prefs.getUChar(keyCnt, 0);
    for (int i = 0; i < 16; i++) {
      char keyA[16]; snprintf(keyA, sizeof(keyA), "b%d_a%02d", b, i);
      found[b].addrs[i] = prefs.getUChar(keyA, 0);
    }
    char kA[16], kC[16]; snprintf(kA, sizeof(kA), "b%d_bme_a", b); snprintf(kC, sizeof(kC), "b%d_bme_c", b);
    uint8_t a = prefs.getUChar(kA, 0), c = prefs.getUChar(kC, 0);
    bme_on_bus[b].present = (a != 0); bme_on_bus[b].addr = a; bme_on_bus[b].chip_id = c;
  }
  prefs.end();
}

static void scanAllI2C() {
  scanBus_HW(I2C0, found[0]);
  bme_on_bus[0] = findBmeCandidateOnBus_HW(I2C0, found[0]);
  // buses 1..3 not used in this minimal A build; keep empty unless you wire them
  for (int b=1;b<4;b++){ clearFound(found[b]); bme_on_bus[b]={false,0,0}; }
  saveScanToNVS();
}

// ==============================
//     Analog + MOSFET
// ==============================
static uint16_t ai_counts[4] = {0};
static float ai_volts[4] = {0};
static bool mos_state[3] = {false,false,false};

static uint16_t readAdcClamped(int pin) {
  int v = analogRead(pin);
  if (v < 0) v = 0;
  if (v > cfg::ADC_MAX) v = cfg::ADC_MAX;
  return (uint16_t)v;
}
static void updateAnalog() {
  // Use configured pins if available
  int ai_pins[4] = {
    pinConfig.ai_pins[0] > 0 ? pinConfig.ai_pins[0] : cfg::PIN_AI1,
    pinConfig.ai_pins[1] > 0 ? pinConfig.ai_pins[1] : cfg::PIN_AI2,
    pinConfig.ai_pins[2] > 0 ? pinConfig.ai_pins[2] : cfg::PIN_AI3,
    pinConfig.ai_pins[3] > 0 ? pinConfig.ai_pins[3] : cfg::PIN_AI4
  };
  
  ai_counts[0] = readAdcClamped(ai_pins[0]);
  ai_counts[1] = readAdcClamped(ai_pins[1]);
  ai_counts[2] = readAdcClamped(ai_pins[2]);
  ai_counts[3] = readAdcClamped(ai_pins[3]);
  
  // Apply calibration
  for (int i=0;i<4;i++) {
    Calibration::applyCalibration(calibConfig, ai_counts[i], i, ai_volts[i]);
  }
}
static void setMosfet(int idx, bool on) {
  if (idx<0 || idx>2) return;
  mos_state[idx]=on;
  // Use configured pins if available
  int mos_pins[3] = {
    pinConfig.mos_pins[0] > 0 ? pinConfig.mos_pins[0] : cfg::PIN_MOS1,
    pinConfig.mos_pins[1] > 0 ? pinConfig.mos_pins[1] : cfg::PIN_MOS2,
    pinConfig.mos_pins[2] > 0 ? pinConfig.mos_pins[2] : cfg::PIN_MOS3
  };
  int pin = mos_pins[idx];
  digitalWrite(pin, on?HIGH:LOW);
}

// ==============================
//       MDP TX queue (reliable)
// ==============================
struct TxItem {
  bool used=false;
  uint32_t seq=0;
  uint8_t  payload[cfg::MAX_PAYLOAD];
  uint16_t len=0;
  uint32_t lastSend=0;
  uint8_t retries=0;
  bool ackRequested=false;
};

static TxItem txq[6];
static uint32_t tx_seq = 1;                 // our seq space
static uint32_t peer_last_inorder = 0;      // last seq we have received from peer (Side-B)
static uint32_t peer_ackd_us = 0;           // last ack from peer acknowledging our seq
static uint32_t telemetryPeriod = cfg::TELEMETRY_PERIOD_MS;

static void uartSendCOBS(const uint8_t* payload, uint16_t len) {
  static uint8_t raw[cfg::MAX_FRAME];
  static uint8_t enc[cfg::MAX_FRAME];

  if (len + 2 > sizeof(raw)) return;
  memcpy(raw, payload, len);
  uint16_t crc = crc16_ccitt_false(payload, len);
  raw[len] = crc & 0xFF;
  raw[len+1] = (crc >> 8) & 0xFF;

  size_t encLen = cobsEncode(raw, len+2, enc);
  Serial2.write(enc, encLen);
  Serial2.write((uint8_t)0x00);
}

static TxItem* txAlloc() {
  for (auto &it : txq) if (!it.used) { it.used=true; it.retries=0; it.lastSend=0; return &it; }
  return nullptr;
}

static void txFreeAcked(uint32_t ackVal) {
  // cumulative ack: free all items with seq <= ackVal
  for (auto &it : txq) {
    if (it.used && it.seq != 0 && it.seq <= ackVal) it.used=false;
  }
}

static void txEnqueue(const uint8_t* payload, uint16_t len, uint32_t seq, bool ackReq) {
  TxItem* it = txAlloc();
  if (!it) return;
  it->seq = seq;
  it->len = len;
  it->ackRequested = ackReq;
  memcpy(it->payload, payload, len);
}

static void txTrySend(TxItem& it, uint32_t now, bool force=false) {
  if (!it.used) return;
  if (!force) {
    if (it.lastSend != 0 && (now - it.lastSend) < cfg::RTO_MS) return;
  }
  if (it.retries > cfg::MAX_RETRIES) { it.used=false; return; }
  uartSendCOBS(it.payload, it.len);
  it.lastSend = now;
  it.retries++;
}

static void txPump(uint32_t now) {
  // resend any unacked reliable messages
  for (auto &it : txq) {
    if (!it.used) continue;
    // If peer already acked this seq, free it.
    if (peer_ackd_us >= it.seq) { it.used=false; continue; }
    // only retransmit if ACK was requested (telemetry can be best-effort)
    if (it.ackRequested) txTrySend(it, now, false);
  }
}

// ==============================
//      RX (COBS) from Side-B
// ==============================
static uint8_t rxFrame[cfg::MAX_FRAME];
static size_t rxLen = 0;

static uint8_t decBuf[cfg::MAX_FRAME];
static size_t decLen = 0;

static void mdpSendAckOnly(uint32_t now);

static void handleMdpPayload(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* hdr = (const mdp_hdr_v1_t*)p;

  if (hdr->magic != cfg::MDP_MAGIC || hdr->version != cfg::MDP_VER) return;

  // Update our view of peer ack (acks our outbound seq space)
  peer_ackd_us = max(peer_ackd_us, hdr->ack);
  txFreeAcked(peer_ackd_us);

  // Sequence / in-order tracking (simple cumulative)
  if (hdr->seq == peer_last_inorder + 1) {
    peer_last_inorder = hdr->seq;
  } else if (hdr->seq <= peer_last_inorder) {
    // duplicate; ok
  } else {
    // out-of-order: accept but do not advance in-order (v1 keeps it simple)
  }

  // If peer requests ACK, respond quickly
  if (hdr->flags & ACK_REQUESTED) mdpSendAckOnly(millis());

  if (hdr->msg_type == MDP_COMMAND) {
    if (len < sizeof(mdp_cmd_v1_t)) return;
    auto* cmd = (const mdp_cmd_v1_t*)p;
    uint16_t cmdLen = cmd->cmd_len;
    const uint8_t* data = cmd->cmd_data;
    if (sizeof(mdp_cmd_v1_t) + cmdLen > len) return;

    int16_t status = 0;

    switch (cmd->cmd_id) {
      case CMD_SET_I2C: {
        if (cmdLen < 2) { status = -2; break; }
        uint8_t sda = data[0], scl = data[1];
        uint32_t hz = cfg::I2C_HW_FREQ_HZ;
        if (cmdLen >= 6) {
          hz = (uint32_t)data[2] | ((uint32_t)data[3]<<8) | ((uint32_t)data[4]<<16) | ((uint32_t)data[5]<<24);
        }
        I2C0.end();
        delay(5);
        I2C0.begin((int)sda, (int)scl, hz);
        scanAllI2C();
      } break;

      case CMD_SCAN_I2C:
        scanAllI2C();
        break;

      case CMD_SET_TELEM_MS: {
        if (cmdLen < 4) { status = -2; break; }
        uint32_t ms = (uint32_t)data[0] | ((uint32_t)data[1]<<8) | ((uint32_t)data[2]<<16) | ((uint32_t)data[3]<<24);
        if (ms < 100) ms = 100;
        if (ms > 60000) ms = 60000;
        telemetryPeriod = ms;
      } break;

      case CMD_SET_MOS: {
        if (cmdLen < 2) { status = -2; break; }
        int idx = (int)data[0] - 1;
        int val = (int)data[1];
        if (idx < 0 || idx > 2) { status = -3; break; }
        setMosfet(idx, val != 0);
      } break;

      case CMD_SAVE_NVS:
        saveScanToNVS();
        break;

      case CMD_LOAD_NVS:
        loadScanFromNVS();
        break;

      case CMD_REBOOT:
        ESP.restart();
        break;

      case CMD_SET_CALIBRATION: {
        // Expect JSON string in cmd_data
        if (cmdLen == 0) { status = -2; break; }
        String jsonStr((const char*)data, cmdLen);
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
          status = -4; break;
        }
        if (!ConfigManager::jsonToCalibration(doc.as<JsonObject>(), calibConfig)) {
          status = -5; break;
        }
        ConfigManager::saveCalibration(calibConfig);
      } break;

      case CMD_SET_PINS: {
        if (cmdLen == 0) { status = -2; break; }
        String jsonStr((const char*)data, cmdLen);
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
          status = -4; break;
        }
        if (!ConfigManager::jsonToPinConfig(doc.as<JsonObject>(), pinConfig)) {
          status = -5; break;
        }
        ConfigManager::savePinConfig(pinConfig);
        // Note: Pin changes require reboot to take effect
      } break;

      case CMD_SET_THRESHOLDS: {
        if (cmdLen == 0) { status = -2; break; }
        String jsonStr((const char*)data, cmdLen);
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
          status = -4; break;
        }
        if (!ConfigManager::jsonToThresholds(doc.as<JsonObject>(), thresholdConfig)) {
          status = -5; break;
        }
        ConfigManager::saveThresholds(thresholdConfig);
      } break;

      case CMD_SET_WIFI: {
        if (cmdLen == 0) { status = -2; break; }
        String jsonStr((const char*)data, cmdLen);
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
          status = -4; break;
        }
        WiFiConfig wifiConfig;
        if (!ConfigManager::jsonToWiFiConfig(doc.as<JsonObject>(), wifiConfig)) {
          status = -5; break;
        }
        ConfigManager::saveWiFiConfig(wifiConfig);
        WiFiManager::updateConfig(wifiConfig);
        // Note: WiFi changes may require reboot
      } break;

      case CMD_FACTORY_RESET:
        ConfigManager::factoryReset();
        // Reset local configs to defaults
        ConfigManager::getDefaultCalibration(calibConfig);
        ConfigManager::getDefaultPinConfig(pinConfig);
        ConfigManager::getDefaultThresholds(thresholdConfig);
        break;

      default:
        status = -1;
        break;
    }

    // Send CMD_RESULT event (reliable)
    uint8_t out[cfg::MAX_PAYLOAD];
    auto* e = (mdp_evt_cmd_result_v1_t*)out;
    memset(out, 0, sizeof(out));

    e->hdr.magic = cfg::MDP_MAGIC;
    e->hdr.version = cfg::MDP_VER;
    e->hdr.msg_type = MDP_EVENT;
    e->hdr.seq = tx_seq++;
    e->hdr.ack = peer_last_inorder;
    e->hdr.flags = ACK_REQUESTED;
    e->hdr.src = cfg::EP_SIDE_A;
    e->hdr.dst = cmd->hdr.src;

    e->evt_type = EVT_CMD_RESULT;
    e->cmd_id = cmd->cmd_id;
    e->status = status;
    e->evt_len = sizeof(uint16_t) + sizeof(int16_t); // cmd_id + status

    uint16_t total = sizeof(mdp_evt_cmd_result_v1_t);
    txEnqueue(out, total, e->hdr.seq, true);
    uartSendCOBS(out, total);
  }
}

static void rxPollCOBS() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();

    if (b == 0x00) {
      if (rxLen == 0) continue;

      if (!cobsDecode(rxFrame, rxLen, decBuf, &decLen)) { rxLen = 0; continue; }
      if (decLen < 2) { rxLen = 0; continue; }

      uint16_t recvCrc = decBuf[decLen-2] | ((uint16_t)decBuf[decLen-1] << 8);
      uint16_t calcCrc = crc16_ccitt_false(decBuf, decLen-2);

      if (recvCrc == calcCrc) handleMdpPayload(decBuf, (uint16_t)(decLen-2));

      rxLen = 0;
      continue;
    }

    if (rxLen < sizeof(rxFrame)) rxFrame[rxLen++] = b;
    else rxLen = 0;
  }
}

// ==============================
//   MDP message builders/senders
// ==============================
static void mdpSendAckOnly(uint32_t now) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = cfg::MDP_MAGIC;
  h->version = cfg::MDP_VER;
  h->msg_type = MDP_ACK;
  h->seq = tx_seq++;                 // ack-only still has seq in our space
  h->ack = peer_last_inorder;         // cumulative ack for peer
  h->flags = IS_ACK;
  h->src = cfg::EP_SIDE_A;
  h->dst = cfg::EP_SIDE_B;
  h->rsv = 0;

  // ACK-only is reliable-ish but low cost: request ack to keep both seq spaces tight
  h->flags |= ACK_REQUESTED;

  txEnqueue(out, sizeof(out), h->seq, true);
  uartSendCOBS(out, sizeof(out));
}

static void sendTelemetry(uint32_t now) {
  // Build TelemetryV1 body
  TelemetryV1 t{};
  t.magic = cfg::MDP_MAGIC;
  t.proto = 1;
  t.msg_type = 1;
  t.seq = 0; // legacy field unused for routing; we use MDP header seq
  t.uptime_ms = now;

  for (int i=0;i<4;i++) { t.ai_counts[i] = ai_counts[i]; t.ai_volts[i] = ai_volts[i]; }
  t.mos[0] = mos_state[0]?1:0;
  t.mos[1] = mos_state[1]?1:0;
  t.mos[2] = mos_state[2]?1:0;

  for (int b=0;b<4;b++) {
    t.i2c_count[b] = found[b].count;
    for (int i=0;i<16;i++) t.i2c_addrs[b][i] = found[b].addrs[i];
    t.bme_addr[b] = bme_on_bus[b].present ? bme_on_bus[b].addr : 0;
    t.bme_chip[b] = bme_on_bus[b].present ? bme_on_bus[b].chip_id : 0;
  }

  t.i2c_sda[0] = pinConfig.i2c_sda > 0 ? pinConfig.i2c_sda : (int8_t)cfg::I2C0_SDA;
  t.i2c_scl[0] = pinConfig.i2c_scl > 0 ? pinConfig.i2c_scl : (int8_t)cfg::I2C0_SCL;

  // Wrap in MDP payload: header + telemetry body
  uint8_t out[cfg::MAX_PAYLOAD];
  if (sizeof(mdp_hdr_v1_t) + sizeof(TelemetryV1) > sizeof(out)) return;

  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = cfg::MDP_MAGIC;
  h->version = cfg::MDP_VER;
  h->msg_type = MDP_TELEMETRY;
  h->seq = tx_seq++;
  h->ack = peer_last_inorder;
  h->flags = 0;                 // telemetry best-effort by default
  h->src = cfg::EP_SIDE_A;
  h->dst = cfg::EP_SIDE_B;
  h->rsv = 0;

  memcpy(out + sizeof(mdp_hdr_v1_t), &t, sizeof(TelemetryV1));
  uint16_t total = (uint16_t)(sizeof(mdp_hdr_v1_t) + sizeof(TelemetryV1));

  // If you want reliable telemetry on UART, set ACK_REQUESTED and enqueue.
  // For now: best-effort (no queue), but we still piggyback ack field.
  uartSendCOBS(out, total);
  
  // Broadcast to portal WebSocket
  PortalManager::updateTelemetry(&t);
}

// ==============================
//             Setup/Loop
// ==============================
static uint32_t lastTelem=0, lastScan=0;

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  Serial2.begin(cfg::LINK_BAUD, SERIAL_8N1, cfg::PIN_RX2, cfg::PIN_TX2);

  analogReadResolution(12);

  pinMode(cfg::PIN_MOS1, OUTPUT);
  pinMode(cfg::PIN_MOS2, OUTPUT);
  pinMode(cfg::PIN_MOS3, OUTPUT);
  setMosfet(0,false); setMosfet(1,false); setMosfet(2,false);

  // Load configurations
  ConfigManager::begin();
  ConfigManager::loadCalibration(calibConfig);
  ConfigManager::loadPinConfig(pinConfig);
  ConfigManager::loadThresholds(thresholdConfig);
  
  // Use configured I2C pins if available
  int i2c_sda = pinConfig.i2c_sda > 0 ? pinConfig.i2c_sda : cfg::I2C0_SDA;
  int i2c_scl = pinConfig.i2c_scl > 0 ? pinConfig.i2c_scl : cfg::I2C0_SCL;
  I2C0.begin(i2c_sda, i2c_scl, cfg::I2C_HW_FREQ_HZ);

  loadScanFromNVS();
  scanAllI2C();

  // Initialize portal
  if (!PortalManager::begin()) {
    Serial.println("{\"portal\":\"init_failed\"}");
  } else {
    Serial.println("{\"portal\":\"ready\"}");
  }

  lastTelem = millis();
  lastScan  = millis();

  Serial.println("{\"side\":\"A\",\"mdp\":\"v1\",\"status\":\"ready\"}");
}

void loop() {
  uint32_t now = millis();

  rxPollCOBS();
  updateAnalog();

  // Portal loop (non-blocking)
  PortalManager::loop();

  if (now - lastScan >= cfg::I2C_RESCAN_MS) {
    lastScan = now;
    scanAllI2C();
  }

  if (now - lastTelem >= telemetryPeriod) {
    lastTelem = now;
    sendTelemetry(now);
  }

  txPump(now);
}
