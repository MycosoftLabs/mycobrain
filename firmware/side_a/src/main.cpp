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
#include <mbedtls/sha256.h>
#include <time.h>

// NeoPixel and Buzzer modules (Side A peripherals)
#include "config.h"
#include "pixel.h"
#include "buzzer.h"

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

// Device identity (role + display name)
static const char* NVS_DEVICE_ROLE_KEY = "dev_role";
static const char* NVS_DEVICE_DISPLAY_NAME_KEY = "dev_disp";

// Default device role (can be overridden by NVS or compile-time)
// Values: mushroom1, sporebase, hyphae1, alarm, gateway, mycodrone, standalone
#ifndef CONFIG_DEVICE_ROLE_DEFAULT
#define CONFIG_DEVICE_ROLE_DEFAULT "standalone"
#endif
#ifndef CONFIG_DEVICE_DISPLAY_NAME_DEFAULT
#define CONFIG_DEVICE_DISPLAY_NAME_DEFAULT ""
#endif

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
static constexpr uint16_t CMD_SET_DEVICE_ROLE = 0x000A; // data: null-terminated string (max 31 chars)
static constexpr uint16_t CMD_SET_DEVICE_DISPLAY_NAME = 0x000B; // data: null-terminated string (max 63 chars)
static constexpr uint16_t CMD_GET_DEVICE_IDENTITY = 0x000C; // response: device_role + device_display_name

// NeoPixel commands
static constexpr uint16_t CMD_PIXEL_SET_COLOR = 0x0010;    // data: r(u8), g(u8), b(u8)
static constexpr uint16_t CMD_PIXEL_SET_BRIGHTNESS = 0x0011; // data: brightness(u8)
static constexpr uint16_t CMD_PIXEL_PATTERN = 0x0012;       // data: pattern name (null-terminated)
static constexpr uint16_t CMD_PIXEL_OFF = 0x0013;          // no data

// Buzzer commands
static constexpr uint16_t CMD_BUZZER_TONE = 0x0020;        // data: freq(u16), duration_ms(u16)
static constexpr uint16_t CMD_BUZZER_PATTERN = 0x0021;     // data: pattern name (null-terminated)
static constexpr uint16_t CMD_BUZZER_STOP = 0x0022;        // no data

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
static Preferences durablePrefs;

// ==============================
//  Device Identity (role/config)
// ==============================
static char deviceRole[32] = CONFIG_DEVICE_ROLE_DEFAULT;
static char deviceDisplayName[64] = CONFIG_DEVICE_DISPLAY_NAME_DEFAULT;

static void loadDeviceIdentity() {
  if (!cfg::USE_NVS) return;
  if (!prefs.begin(cfg::NVS_NS, true)) return;
  
  String role = prefs.getString(cfg::NVS_DEVICE_ROLE_KEY, CONFIG_DEVICE_ROLE_DEFAULT);
  strncpy(deviceRole, role.c_str(), sizeof(deviceRole) - 1);
  deviceRole[sizeof(deviceRole) - 1] = '\0';
  
  String disp = prefs.getString(cfg::NVS_DEVICE_DISPLAY_NAME_KEY, CONFIG_DEVICE_DISPLAY_NAME_DEFAULT);
  strncpy(deviceDisplayName, disp.c_str(), sizeof(deviceDisplayName) - 1);
  deviceDisplayName[sizeof(deviceDisplayName) - 1] = '\0';
  
  prefs.end();
}

static void saveDeviceIdentity() {
  if (!cfg::USE_NVS) return;
  if (!prefs.begin(cfg::NVS_NS, false)) return;
  prefs.putString(cfg::NVS_DEVICE_ROLE_KEY, deviceRole);
  prefs.putString(cfg::NVS_DEVICE_DISPLAY_NAME_KEY, deviceDisplayName);
  prefs.end();
}

static void setDeviceRole(const char* role) {
  if (!role) return;
  strncpy(deviceRole, role, sizeof(deviceRole) - 1);
  deviceRole[sizeof(deviceRole) - 1] = '\0';
  saveDeviceIdentity();
}

static void setDeviceDisplayName(const char* name) {
  if (!name) return;
  strncpy(deviceDisplayName, name, sizeof(deviceDisplayName) - 1);
  deviceDisplayName[sizeof(deviceDisplayName) - 1] = '\0';
  saveDeviceIdentity();
}

static inline float adcCountsToVolts(uint16_t c) { return (float)c * (cfg::ADC_VREF / (float)cfg::ADC_MAX); }

// ==============================
//  Envelope + durable replay
// ==============================
namespace durable_cfg {
constexpr int QUEUE_CAPACITY = 8;
constexpr size_t SLOT_BYTES = cfg::MAX_PAYLOAD;
static const char* NVS_NS = "myco_a_q";
static const char* KEY_HEAD = "head";
static const char* KEY_TAIL = "tail";
static const char* KEY_COUNT = "count";
static const char* KEY_TXSEQ = "txseq";
}

static uint8_t durableHead = 0;
static uint8_t durableTail = 0;
static uint8_t durableCount = 0;
static bool durableReady = false;

static void toHex(const uint8_t* data, size_t len, char* outHex, size_t outHexCap) {
  size_t pos = 0;
  for (size_t i = 0; i < len && (pos + 2) < outHexCap; i++) {
    pos += (size_t)snprintf(outHex + pos, outHexCap - pos, "%02x", data[i]);
  }
  if (pos < outHexCap) outHex[pos] = '\0';
}

static bool base64Encode(const uint8_t* src, size_t len, char* out, size_t outCap) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t outLen = 4 * ((len + 2) / 3);
  if (outLen + 1 > outCap) return false;
  size_t j = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t a = src[i];
    uint32_t b = (i + 1 < len) ? src[i + 1] : 0;
    uint32_t c = (i + 2 < len) ? src[i + 2] : 0;
    uint32_t triple = (a << 16) | (b << 8) | c;

    out[j++] = table[(triple >> 18) & 0x3F];
    out[j++] = table[(triple >> 12) & 0x3F];
    out[j++] = (i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=';
    out[j++] = (i + 2 < len) ? table[triple & 0x3F] : '=';
  }
  out[j] = '\0';
  return true;
}

static void sha256Bytes(const uint8_t* data, size_t len, uint8_t out[32]) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, data, len);
  mbedtls_sha256_finish_ret(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

static bool buildTelemetryEnvelope(uint32_t now, uint32_t seq, uint8_t* out, uint16_t* outLen) {
  if (!out || !outLen) return false;
  // Unsigned envelope body (stable key order) with device identity.
  char unsignedBody[768];
  
  // Build display name JSON field (null if empty)
  char dispNameField[96] = "";
  if (deviceDisplayName[0] != '\0') {
    snprintf(dispNameField, sizeof(dispNameField), ",\"device_display_name\":\"%s\"", deviceDisplayName);
  }
  
  int unsignedN = snprintf(
    unsignedBody, sizeof(unsignedBody),
    "{\"hdr\":{\"deviceId\":\"mycobrain-side-a\",\"device_role\":\"%s\"%s,\"proto\":\"uart\",\"msgId\":\"%08lu\"},"
    "\"ts\":{\"utc\":\"%lu\",\"mono_ms\":%lu},"
    "\"seq\":%lu,"
    "\"pack\":["
      "{\"id\":\"ai1\",\"v\":%.4f,\"u\":\"V\"},"
      "{\"id\":\"ai2\",\"v\":%.4f,\"u\":\"V\"},"
      "{\"id\":\"ai3\",\"v\":%.4f,\"u\":\"V\"},"
      "{\"id\":\"ai4\",\"v\":%.4f,\"u\":\"V\"},"
      "{\"id\":\"mos1\",\"v\":%d,\"u\":\"bool\"},"
      "{\"id\":\"mos2\",\"v\":%d,\"u\":\"bool\"},"
      "{\"id\":\"mos3\",\"v\":%d,\"u\":\"bool\"}"
    "],"
    "\"meta\":{\"schema\":\"mycosoft.v1\",\"units\":\"si\"}}",
    deviceRole, dispNameField,
    (unsigned long)seq,
    (unsigned long)(time(nullptr)),
    (unsigned long)now,
    (unsigned long)seq,
    ai_volts[0], ai_volts[1], ai_volts[2], ai_volts[3],
    mos_state[0] ? 1 : 0, mos_state[1] ? 1 : 0, mos_state[2] ? 1 : 0
  );
  if (unsignedN <= 0 || (size_t)unsignedN >= sizeof(unsignedBody)) return false;

  uint8_t hashRaw[32];
  sha256Bytes((const uint8_t*)unsignedBody, (size_t)unsignedN, hashRaw);
  char hashHex[65];
  toHex(hashRaw, sizeof(hashRaw), hashHex, sizeof(hashHex));

  // Device-side placeholder signature for local bring-up:
  // Once secure key provisioning lands, replace with real ed25519 signature.
  char sigB64[96];
  if (!base64Encode(hashRaw, sizeof(hashRaw), sigB64, sizeof(sigB64))) return false;

  int finalN = snprintf(
    (char*)out, cfg::MAX_PAYLOAD,
    "%s,\"hash\":\"sha256:%s\",\"sig\":\"ed25519:%s\"}",
    unsignedBody, hashHex, sigB64
  );
  if (finalN <= 0 || finalN >= (int)cfg::MAX_PAYLOAD) return false;

  *outLen = (uint16_t)finalN;
  return true;
}

static void durableLoadMeta() {
  durableHead = durablePrefs.getUChar(durable_cfg::KEY_HEAD, 0);
  durableTail = durablePrefs.getUChar(durable_cfg::KEY_TAIL, 0);
  durableCount = durablePrefs.getUChar(durable_cfg::KEY_COUNT, 0);
}

static void durableSaveMeta() {
  if (!durableReady) return;
  durablePrefs.putUChar(durable_cfg::KEY_HEAD, durableHead);
  durablePrefs.putUChar(durable_cfg::KEY_TAIL, durableTail);
  durablePrefs.putUChar(durable_cfg::KEY_COUNT, durableCount);
}

static int durableEnqueue(const uint8_t* payload, uint16_t len, uint32_t seq) {
  if (!durableReady) return -1;
  if (!payload || len == 0 || len > durable_cfg::SLOT_BYTES) return -1;
  if (durableCount >= durable_cfg::QUEUE_CAPACITY) {
    durableTail = (uint8_t)((durableTail + 1) % durable_cfg::QUEUE_CAPACITY);
    durableCount--;
  }
  int slot = durableHead;
  char kSeq[8], kLen[8], kDat[8];
  snprintf(kSeq, sizeof(kSeq), "q%u_s", (unsigned)slot);
  snprintf(kLen, sizeof(kLen), "q%u_l", (unsigned)slot);
  snprintf(kDat, sizeof(kDat), "q%u_d", (unsigned)slot);
  durablePrefs.putULong(kSeq, seq);
  durablePrefs.putUShort(kLen, len);
  durablePrefs.putBytes(kDat, payload, len);
  durableHead = (uint8_t)((durableHead + 1) % durable_cfg::QUEUE_CAPACITY);
  durableCount++;
  durableSaveMeta();
  return slot;
}

static void durableAck(uint32_t ackSeq) {
  if (!durableReady) return;
  while (durableCount > 0) {
    char kSeq[8];
    snprintf(kSeq, sizeof(kSeq), "q%u_s", (unsigned)durableTail);
    uint32_t slotSeq = durablePrefs.getULong(kSeq, 0);
    if (slotSeq == 0 || slotSeq > ackSeq) break;
    durableTail = (uint8_t)((durableTail + 1) % durable_cfg::QUEUE_CAPACITY);
    durableCount--;
  }
  durableSaveMeta();
}

static void durableReplayInit() {
  if (!durableReady) return;
  // Re-enqueue any durable messages that weren't acked before reboot.
  for (uint8_t i = 0; i < durableCount; i++) {
    uint8_t slot = (uint8_t)((durableTail + i) % durable_cfg::QUEUE_CAPACITY);
    char kLen[8], kDat[8];
    snprintf(kLen, sizeof(kLen), "q%u_l", (unsigned)slot);
    snprintf(kDat, sizeof(kDat), "q%u_d", (unsigned)slot);
    uint16_t len = durablePrefs.getUShort(kLen, 0);
    if (len == 0 || len > durable_cfg::SLOT_BYTES) continue;
    uint8_t buf[cfg::MAX_PAYLOAD];
    size_t got = durablePrefs.getBytes(kDat, buf, len);
    if (got != len) continue;

    // Extract MDP header to recover seq.
    if (len < sizeof(mdp_hdr_v1_t)) continue;
    auto* hdr = (const mdp_hdr_v1_t*)buf;
    if (hdr->magic != cfg::MDP_MAGIC || hdr->version != cfg::MDP_VER) continue;

    txEnqueue(buf, len, hdr->seq, true);
    uartSendCOBS(buf, len);
  }
}

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
  ai_counts[0] = readAdcClamped(cfg::PIN_AI1);
  ai_counts[1] = readAdcClamped(cfg::PIN_AI2);
  ai_counts[2] = readAdcClamped(cfg::PIN_AI3);
  ai_counts[3] = readAdcClamped(cfg::PIN_AI4);
  for (int i=0;i<4;i++) ai_volts[i] = adcCountsToVolts(ai_counts[i]);
}
static void setMosfet(int idx, bool on) {
  if (idx<0 || idx>2) return;
  mos_state[idx]=on;
  int pin = (idx==0)?cfg::PIN_MOS1:(idx==1)?cfg::PIN_MOS2:cfg::PIN_MOS3;
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
  // Mirror delivery progress into the durable replay queue.
  durableAck(ackVal);
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

      case CMD_SET_DEVICE_ROLE: {
        if (cmdLen < 1) { status = -2; break; }
        char role[32] = {0};
        size_t copyLen = (cmdLen < sizeof(role)-1) ? cmdLen : sizeof(role)-1;
        memcpy(role, data, copyLen);
        role[copyLen] = '\0';
        setDeviceRole(role);
      } break;

      case CMD_SET_DEVICE_DISPLAY_NAME: {
        if (cmdLen < 1) { status = -2; break; }
        char name[64] = {0};
        size_t copyLen = (cmdLen < sizeof(name)-1) ? cmdLen : sizeof(name)-1;
        memcpy(name, data, copyLen);
        name[copyLen] = '\0';
        setDeviceDisplayName(name);
      } break;

      case CMD_GET_DEVICE_IDENTITY:
        // Response will include device_role and device_display_name in the event data
        // For now, just acknowledge success
        break;

      // NeoPixel commands
      case CMD_PIXEL_SET_COLOR: {
        if (cmdLen < 3) { status = -2; break; }
        Pixel::setColor(data[0], data[1], data[2]);
      } break;

      case CMD_PIXEL_SET_BRIGHTNESS: {
        if (cmdLen < 1) { status = -2; break; }
        Pixel::setBrightness(data[0]);
      } break;

      case CMD_PIXEL_PATTERN: {
        if (cmdLen < 1) { status = -2; break; }
        char pattern[32] = {0};
        size_t copyLen = (cmdLen < sizeof(pattern)-1) ? cmdLen : sizeof(pattern)-1;
        memcpy(pattern, data, copyLen);
        pattern[copyLen] = '\0';
        Pixel::startPattern(pattern);
      } break;

      case CMD_PIXEL_OFF:
        Pixel::off();
        break;

      // Buzzer commands
      case CMD_BUZZER_TONE: {
        if (cmdLen < 4) { status = -2; break; }
        uint16_t freq = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
        uint16_t dur = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
        Buzzer::tone(freq, dur);
      } break;

      case CMD_BUZZER_PATTERN: {
        if (cmdLen < 1) { status = -2; break; }
        char pattern[32] = {0};
        size_t copyLen = (cmdLen < sizeof(pattern)-1) ? cmdLen : sizeof(pattern)-1;
        memcpy(pattern, data, copyLen);
        pattern[copyLen] = '\0';
        Buzzer::playPattern(pattern);
      } break;

      case CMD_BUZZER_STOP:
        Buzzer::stop();
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
  // Build deterministic envelope payload (JSON for bring-up; CBOR in later revision)
  uint8_t out[cfg::MAX_PAYLOAD];
  uint16_t envLen = 0;

  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = cfg::MDP_MAGIC;
  h->version = cfg::MDP_VER;
  h->msg_type = MDP_TELEMETRY;
  h->seq = tx_seq++;
  if (durableReady) durablePrefs.putULong(durable_cfg::KEY_TXSEQ, tx_seq);
  h->ack = peer_last_inorder;
  h->flags = ACK_REQUESTED;     // request ACK so durability can advance
  h->src = cfg::EP_SIDE_A;
  h->dst = cfg::EP_SIDE_B;
  h->rsv = 0;

  if (!buildTelemetryEnvelope(now, h->seq, out + sizeof(mdp_hdr_v1_t), &envLen)) return;
  uint16_t total = (uint16_t)(sizeof(mdp_hdr_v1_t) + envLen);

  // Persist and enqueue for replay across reboot.
  (void)durableEnqueue(out, total, h->seq);
  txEnqueue(out, total, h->seq, true);
  uartSendCOBS(out, total);
}

// ==============================
//             Setup/Loop
// ==============================
static uint32_t lastTelem=0, lastScan=0;

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  Serial2.begin(cfg::LINK_BAUD, SERIAL_8N1, cfg::PIN_RX2, cfg::PIN_TX2);

  // Durable queue NVS (survives reboot/power loss)
  if (durablePrefs.begin(durable_cfg::NVS_NS, false)) {
    durableReady = true;
    durableLoadMeta();
    tx_seq = durablePrefs.getULong(durable_cfg::KEY_TXSEQ, tx_seq);
    durableReplayInit();
  }

  // Load device identity (role, display name) from NVS
  loadDeviceIdentity();

  // Initialize NeoPixel and Buzzer (Side A peripherals)
  Pixel::init();
  Buzzer::init();
  
  // Brief startup indication
  Pixel::setColor(0, 32, 0);  // Green startup
  Buzzer::playPattern(PATTERN_SUCCESS);

  analogReadResolution(12);

  pinMode(cfg::PIN_MOS1, OUTPUT);
  pinMode(cfg::PIN_MOS2, OUTPUT);
  pinMode(cfg::PIN_MOS3, OUTPUT);
  setMosfet(0,false); setMosfet(1,false); setMosfet(2,false);

  I2C0.begin(cfg::I2C0_SDA, cfg::I2C0_SCL, cfg::I2C_HW_FREQ_HZ);

  loadScanFromNVS();
  scanAllI2C();

  lastTelem = millis();
  lastScan  = millis();

  // Status with device identity for service parsing
  char statusJson[256];
  if (deviceDisplayName[0] != '\0') {
    snprintf(statusJson, sizeof(statusJson),
      "{\"side\":\"A\",\"mdp\":\"v1\",\"device_role\":\"%s\",\"device_display_name\":\"%s\",\"status\":\"ready\"}",
      deviceRole, deviceDisplayName);
  } else {
    snprintf(statusJson, sizeof(statusJson),
      "{\"side\":\"A\",\"mdp\":\"v1\",\"device_role\":\"%s\",\"status\":\"ready\"}",
      deviceRole);
  }
  Serial.println(statusJson);
}

void loop() {
  uint32_t now = millis();

  rxPollCOBS();
  updateAnalog();

  // Update NeoPixel and Buzzer patterns (non-blocking)
  Pixel::updatePattern();
  Buzzer::updatePattern();

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
