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

static const char* FW_VERSION = "side-a-mdp-2.1.0";
static const uint32_t WDT_TIMEOUT_S = 30;
static const uint32_t SERIAL_BAUD = 115200;

// Garret morgio CLI state
enum OutputFormat : uint8_t { FMT_LINES = 0, FMT_NDJSON = 1 };
static OutputFormat g_fmt = FMT_LINES;
static bool g_live_on = false;
static uint32_t g_live_period_ms = 1000;
static uint32_t g_last_live_ms = 0;
static bool g_dbg = false;
static uint32_t g_last_dbg_ms = 0;
static const uint32_t g_dbg_period_ms = 3000;
enum LedMode : uint8_t { LEDMODE_OFF = 0, LEDMODE_STATE = 1, LEDMODE_MANUAL = 2 };
static LedMode g_led_mode = LEDMODE_STATE;
static uint8_t g_led_r = 0, g_led_g = 0, g_led_b = 0;

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

static bool slotInit(SensorSlot& s);  // forward decl for CLI i2c command

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
  if (g_dbg) {
    uint32_t now = millis();
    if (now - g_last_dbg_ms >= g_dbg_period_ms) {
      g_last_dbg_ms = now;
      Serial.printf("#dbg %s T=%.2f RH=%.2f P=%.2f gas=%.0f\n", s.name, s.r.tC, s.r.rh, s.r.p_hPa, s.r.gas_ohm);
    }
  }
}

static void cbAMB(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_AMB, data, outputs);
}

static void cbENV(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_ENV, data, outputs);
}

// -------------------------
// SuperMorgIO poster (from Garret's morgio firmware)
// -------------------------
static const char kPoster[] =
"====================================================================\n"
"  SuperMorgIO\n"
"  Mycosoft ESP32AB\n"
"====================================================================\n"
"   ###############################\n"
"   #                             #\n"
"   #      _   _  ____  ____      #\n"
"   #     | \\ | ||  _ \\|  _ \\     #\n"
"   #     |  \\| || |_) | |_) |    #\n"
"   #     | |\\  ||  __/|  __/     #\n"
"   #     |_| \\_||_|   |_|        #\n"
"   #                             #\n"
"   #   (blocky Morgan render)    #\n"
"   #      [=]   [=]              #\n"
"   #        \\___/                #\n"
"   #      __/|||\\__              #\n"
"   #     /__|||||__\\             #\n"
"   #                             #\n"
"   ###############################\n"
"--------------------------------------------------------------------\n"
"  Commands: help | poster | morgio | coin | bump | power | 1up\n"
"  LED: led mode off|state|manual  | led rgb <r> <g> <b>\n"
"--------------------------------------------------------------------\n";

static void printPoster() {
  Serial.println();
  Serial.print(kPoster);
  Serial.println();
}

// -------------------------
// I2C helpers (from Garret's morgio firmware)
// -------------------------
static bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* out, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  size_t got = Wire.requestFrom((int)addr, (int)n);
  if (got != n) return false;
  for (size_t i = 0; i < n; i++) out[i] = Wire.read();
  return true;
}

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t& val) {
  return i2cReadBytes(addr, reg, &val, 1);
}

static void printBmeIdentity(uint8_t addr, int repeats = 3) {
  Serial.printf("--- BME ID probe @ 0x%02X ---\n", addr);
  for (int i = 0; i < repeats; i++) {
    uint8_t chip = 0, var = 0;
    bool ok1 = i2cRead8(addr, 0xD0, chip);
    bool ok2 = i2cRead8(addr, 0xF0, var);
    Serial.printf("  #%d chip_id: %s 0x%02X | variant_id: %s 0x%02X\n",
                  i + 1,
                  ok1 ? "OK" : "FAIL", chip,
                  ok2 ? "OK" : "FAIL", var);
    delay(25);
  }
  Serial.println("------------------------");
}

// -------------------------
// I2C scan (from Garret's morgio firmware)
// -------------------------
static void printI2cScan() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  found: 0x%02X\n", a);
      found++;
    }
  }
  if (!found) Serial.println("  (none)");
}

// -------------------------
// SuperMorgIO retro buzzer SFX (ledcWriteTone; blocking in CLI OK)
// -------------------------
static void playTone(int hz, int ms, int gapMs = 8) {
  if (hz > 0) {
    ledcWriteTone(0, hz);
    delay(ms);
    ledcWriteTone(0, 0);
  } else {
    delay(ms);
  }
  if (gapMs > 0) delay(gapMs);
}

static int noteHz(const char* n) {
  if (!n) return 0;
  if (strcmp(n, "REST") == 0) return 0;
  if (strcmp(n, "C4") == 0) return 262;
  if (strcmp(n, "D4") == 0) return 294;
  if (strcmp(n, "E4") == 0) return 330;
  if (strcmp(n, "F4") == 0) return 349;
  if (strcmp(n, "G4") == 0) return 392;
  if (strcmp(n, "A4") == 0) return 440;
  if (strcmp(n, "B4") == 0) return 494;
  if (strcmp(n, "C5") == 0) return 523;
  if (strcmp(n, "D5") == 0) return 587;
  if (strcmp(n, "E5") == 0) return 659;
  if (strcmp(n, "F5") == 0) return 698;
  if (strcmp(n, "G5") == 0) return 784;
  if (strcmp(n, "A5") == 0) return 880;
  if (strcmp(n, "B5") == 0) return 988;
  if (strcmp(n, "C6") == 0) return 1047;
  if (strcmp(n, "D6") == 0) return 1175;
  if (strcmp(n, "E6") == 0) return 1319;
  return 0;
}

static void playNote(const char* n, int ms, int gapMs = 8) {
  playTone(noteHz(n), ms, gapMs);
}

static void sfxCoin() { playNote("E6", 35, 5); playNote("B5", 25, 0); }
static void sfxBump() { playNote("C5", 40, 0); playNote("REST", 10, 0); playNote("C5", 25, 0); }
static void sfxPowerUp() {
  playNote("C5", 60); playNote("E5", 60); playNote("G5", 80);
  playNote("C6", 120, 0);
}
static void sfx1Upish() {
  playNote("E5", 60); playNote("G5", 60); playNote("A5", 60);
  playNote("C6", 140, 0);
}
static void sfxSuperMorgIOBoot() {
  const int q = 120;
  const int e = q / 2;
  const int s = q / 4;
  playNote("C5", s); playNote("E5", s); playNote("G5", s); playNote("C6", e);
  playNote("REST", s);
  playNote("D5", s); playNote("F5", s); playNote("A5", s); playNote("D6", e);
  playNote("REST", s);
  playNote("E5", e); playNote("G5", e); playNote("B5", e);
  playNote("A5", s); playNote("G5", s); playNote("E5", s); playNote("C5", s);
  playNote("D5", e); playNote("G5", e); playNote("C5", q);
}

// -------------------------
// Live output / LED state (Garret morgio)
// -------------------------
static void printOneSensorLine(const SensorSlot& s) {
  if (!s.present || !s.r.valid) return;
  uint32_t age = millis() - s.r.t_ms;
  Serial.printf("#live %s addr=0x%02X age=%lums T=%.2fC RH=%.2f%% P=%.2fhPa Gas=%.0fOhm",
                s.name, s.addr, (unsigned long)age,
                s.r.tC, s.r.rh, s.r.p_hPa, s.r.gas_ohm);
  if (!isnan(s.r.iaq)) Serial.printf(" IAQ=%.2f CO2eq=%.2f VOC=%.2f", s.r.iaq, s.r.co2eq, s.r.voc);
  Serial.println();
}

static void printOneSensorNdjson(const SensorSlot& s) {
  if (!s.present || !s.r.valid) return;
  Serial.print("{\"ts_ms\":"); Serial.print(millis());
  Serial.print(",\"sensor\":\""); Serial.print(s.name); Serial.print("\"");
  Serial.print(",\"tC\":"); Serial.print(s.r.tC, 2);
  Serial.print(",\"rh\":"); Serial.print(s.r.rh, 2);
  Serial.print(",\"p_hPa\":"); Serial.print(s.r.p_hPa, 2);
  Serial.print(",\"gas\":"); Serial.print(s.r.gas_ohm, 0);
  if (!isnan(s.r.iaq)) Serial.print(",\"iaq\":"), Serial.print(s.r.iaq, 2), Serial.print(",\"co2eq\":"), Serial.print(s.r.co2eq, 2), Serial.print(",\"voc\":"), Serial.print(s.r.voc, 2);
  Serial.println("}");
}

static void liveOutput() {
  if (g_fmt == FMT_LINES) {
    Serial.println("#live =====");
    if (S_AMB.present) printOneSensorLine(S_AMB);
    if (S_ENV.present) printOneSensorLine(S_ENV);
  } else {
    if (S_AMB.present) printOneSensorNdjson(S_AMB);
    if (S_ENV.present) printOneSensorNdjson(S_ENV);
  }
}

static void ledStateUpdate() {
  if (g_led_mode == LEDMODE_OFF) {
    pixels.clear();
    pixels.show();
    return;
  }
  if (g_led_mode == LEDMODE_MANUAL) {
    pixels.setPixelColor(0, pixels.Color(g_led_r, g_led_g, g_led_b));
    pixels.show();
    return;
  }
  const bool ambPresent = S_AMB.present;
  const bool envPresent = S_ENV.present;
  const bool anyPresent = ambPresent || envPresent;
  const bool anyBeginFail = (ambPresent && !S_AMB.beginOk) || (envPresent && !S_ENV.beginOk);
  const bool anySubFail = (ambPresent && S_AMB.beginOk && !S_AMB.subOk) || (envPresent && S_ENV.beginOk && !S_ENV.subOk);
  const bool initPhase = anyPresent && ((ambPresent && !S_AMB.r.valid) || (envPresent && !S_ENV.r.valid));

  if (!anyPresent) {
    uint8_t v = (uint8_t)(20 + (millis() / 6) % 80);
    pixels.setPixelColor(0, pixels.Color(v, 0, 0));
    pixels.show();
    return;
  }
  if (anyBeginFail) {
    bool on = ((millis() / 250) % 2) == 0;
    pixels.setPixelColor(0, pixels.Color(on ? 120 : 0, 0, 0));
    pixels.show();
    return;
  }
  if (initPhase) {
    uint8_t v = (uint8_t)(30 + (millis() / 8) % 90);
    pixels.setPixelColor(0, pixels.Color(0, 0, v));
    pixels.show();
    return;
  }
  if (anySubFail) {
    pixels.setPixelColor(0, pixels.Color(80, 60, 0));
    pixels.show();
    return;
  }
  pixels.setPixelColor(0, pixels.Color(0, 90, 0));
  pixels.show();
}

// -------------------------
// Serial CLI (ASCII line input; MDP uses 0x00-delimited binary)
// -------------------------
static void printCliHelp() {
  Serial.println();
  Serial.println("Commands (Serial Monitor @ 115200):");
  Serial.println("  help         - this help");
  Serial.println("  scan         - I2C bus scan");
  Serial.println("  status       - I2C config + BME presence");
  Serial.println("  i2c <sda> <scl> [hz] - set I2C pins + optional clock, re-init");
  Serial.println("  poster       - SuperMorgIO ASCII art");
  Serial.println("  morgio|coin|bump|power|1up - retro SFX");
  Serial.println("  probe amb|env [n] - BME ID probe (default n=3)");
  Serial.println("  regs amb|env - BME regs 0xD0 0xF0");
  Serial.println("  led rgb <r> <g> <b> - set NeoPixel (0-255)");
  Serial.println("  led mode off|state|manual");
  Serial.println("  live on|off [period_ms]");
  Serial.println("  dbg on|off   - throttled debug print");
  Serial.println("  fmt lines|json");
  Serial.println();
}

static void printStatus() {
  Serial.printf("I2C: SDA=%d SCL=%d @ %lu Hz\n", I2C_SDA, I2C_SCL, (unsigned long)I2C_FREQ);
  Serial.printf("AMB: present=%s addr=0x77 begin=%s sub=%s\n",
                S_AMB.present ? "YES" : "NO",
                S_AMB.beginOk ? "OK" : "FAIL",
                S_AMB.subOk ? "OK" : "FAIL");
  Serial.printf("ENV: present=%s addr=0x76 begin=%s sub=%s\n",
                S_ENV.present ? "YES" : "NO",
                S_ENV.beginOk ? "OK" : "FAIL",
                S_ENV.subOk ? "OK" : "FAIL");
}

// Runtime I2C config (overrides config.h when set via CLI)
static int gSda = I2C_SDA;
static int gScl = I2C_SCL;
static uint32_t gI2cHz = I2C_FREQ;

static void handleCliCommand(const char* line) {
  // Parse first token
  int pos = 0;
  while (line[pos] && (line[pos] == ' ' || line[pos] == '\t')) pos++;
  int start = pos;
  while (line[pos] && line[pos] != ' ' && line[pos] != '\t') pos++;
  if (start >= pos) return;

  char cmd[32];
  int cmdLen = pos - start;
  if (cmdLen >= (int)sizeof(cmd)) cmdLen = (int)sizeof(cmd) - 1;
  memcpy(cmd, line + start, cmdLen);
  cmd[cmdLen] = '\0';
  for (int i = 0; cmd[i]; i++) if (cmd[i] >= 'A' && cmd[i] <= 'Z') cmd[i] += 32;

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    printCliHelp();
    return;
  }
  if (strcmp(cmd, "scan") == 0) {
    printI2cScan();
    return;
  }
  if (strcmp(cmd, "status") == 0) {
    printStatus();
    return;
  }
  if (strcmp(cmd, "i2c") == 0) {
    int sda = atoi(line + pos); while (line[pos] && line[pos] != ' ' && line[pos] != '\t') pos++; while (line[pos] == ' ' || line[pos] == '\t') pos++;
    int scl = atoi(line + pos); while (line[pos] && line[pos] != ' ' && line[pos] != '\t') pos++; while (line[pos] == ' ' || line[pos] == '\t') pos++;
    long hz = atol(line + pos);
    if (sda >= 0 && sda <= 255) gSda = sda;
    if (scl >= 0 && scl <= 255) gScl = scl;
    if (hz > 0 && hz <= 1000000) gI2cHz = (uint32_t)hz;
    Serial.printf("I2C: SDA=%d SCL=%d @ %lu Hz - re-init\n", gSda, gScl, (unsigned long)gI2cHz);
    Wire.end();
    Wire.begin((int)gSda, (int)gScl);
    Wire.setClock(gI2cHz);
    slotInit(S_AMB);
    slotInit(S_ENV);
    bme688_count = (S_AMB.present ? 1 : 0) + (S_ENV.present ? 1 : 0);
    printI2cScan();
    return;
  }
  if (strcmp(cmd, "poster") == 0) {
    printPoster();
    return;
  }
  if (strcmp(cmd, "morgio") == 0) {
    sfxSuperMorgIOBoot();
    return;
  }
  if (strcmp(cmd, "coin") == 0) { sfxCoin(); return; }
  if (strcmp(cmd, "bump") == 0) { sfxBump(); return; }
  if (strcmp(cmd, "power") == 0) { sfxPowerUp(); return; }
  if (strcmp(cmd, "1up") == 0) { sfx1Upish(); return; }
  if (strcmp(cmd, "probe") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    int a2 = pos;
    while (line[pos] && line[pos] != ' ' && line[pos] != '\t') pos++;
    char target[8];
    int tl = pos - a2;
    if (tl >= (int)sizeof(target)) tl = (int)sizeof(target) - 1;
    memcpy(target, line + a2, tl);
    target[tl] = '\0';
    int n = 3;
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    if (line[pos]) n = atoi(line + pos);
    if (n <= 0) n = 3;
    if (strcmp(target, "amb") == 0) printBmeIdentity(0x77, n);
    else if (strcmp(target, "env") == 0) printBmeIdentity(0x76, n);
    else Serial.println("probe amb|env [n]");
    return;
  }
  if (strcmp(cmd, "regs") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    if (strncmp(line + pos, "amb", 3) == 0) {
      uint8_t b[2];
      if (i2cReadBytes(0x77, 0xD0, b, 2)) Serial.printf("AMB 0xD0=0x%02X 0xF0=0x%02X\n", b[0], b[1]);
    } else if (strncmp(line + pos, "env", 3) == 0) {
      uint8_t b[2];
      if (i2cReadBytes(0x76, 0xD0, b, 2)) Serial.printf("ENV 0xD0=0x%02X 0xF0=0x%02X\n", b[0], b[1]);
    } else Serial.println("regs amb|env");
    return;
  }
  if (strcmp(cmd, "led") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    int m2 = pos;
    while (line[pos] && line[pos] != ' ' && line[pos] != '\t') pos++;
    char sub[16];
    int sl = pos - m2;
    if (sl >= (int)sizeof(sub)) sl = (int)sizeof(sub) - 1;
    memcpy(sub, line + m2, sl);
    sub[sl] = '\0';
    if (strcmp(sub, "rgb") == 0) {
      int r = atoi(line + pos); while (line[pos] && line[pos] != ' ') pos++; while (line[pos] == ' ') pos++;
      int g = atoi(line + pos); while (line[pos] && line[pos] != ' ') pos++; while (line[pos] == ' ') pos++;
      int b = atoi(line + pos);
      g_led_r = (r < 0) ? 0 : (r > 255 ? 255 : r);
      g_led_g = (g < 0) ? 0 : (g > 255 ? 255 : g);
      g_led_b = (b < 0) ? 0 : (b > 255 ? 255 : b);
      g_led_mode = LEDMODE_MANUAL;
      pixels.setPixelColor(0, pixels.Color(g_led_r, g_led_g, g_led_b));
      pixels.show();
    } else if (strcmp(sub, "mode") == 0) {
      while (line[pos] == ' ' || line[pos] == '\t') pos++;
      if (strncmp(line + pos, "off", 3) == 0) {
        g_led_mode = LEDMODE_OFF;
        pixels.clear();
        pixels.show();
      } else if (strncmp(line + pos, "state", 5) == 0) {
        g_led_mode = LEDMODE_STATE;
      } else if (strncmp(line + pos, "manual", 6) == 0) {
        g_led_mode = LEDMODE_MANUAL;
      }
    }
    return;
  }
  if (strcmp(cmd, "live") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    if (strncmp(line + pos, "on", 2) == 0) {
      g_live_on = true;
      while (line[pos] && line[pos] != ' ') pos++;
      while (line[pos] == ' ') pos++;
      if (line[pos]) { int p = atoi(line + pos); if (p > 0) g_live_period_ms = (uint32_t)p; }
      Serial.printf("live ON %lu ms\n", (unsigned long)g_live_period_ms);
    } else if (strncmp(line + pos, "off", 3) == 0) {
      g_live_on = false;
      Serial.println("live OFF");
    }
    return;
  }
  if (strcmp(cmd, "dbg") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    g_dbg = (strncmp(line + pos, "on", 2) == 0);
    Serial.printf("dbg %s\n", g_dbg ? "ON" : "OFF");
    return;
  }
  if (strcmp(cmd, "fmt") == 0) {
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    if (strncmp(line + pos, "json", 4) == 0) g_fmt = FMT_NDJSON;
    else g_fmt = FMT_LINES;
    Serial.printf("fmt %s\n", g_fmt == FMT_NDJSON ? "json" : "lines");
    return;
  }
  Serial.println("Unknown command. Type 'help' for list.");
}

static bool isPrintableAscii(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t b = buf[i];
    if (b != '\t' && (b < 0x20 || b > 0x7E)) return false;
  }
  return true;
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

  initBuzzer();
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
    } else if (b == '\n' || b == '\r') {
      if (cobs_len > 0 && isPrintableAscii(cobs_buffer, cobs_len)) {
        cobs_buffer[cobs_len] = '\0';
        handleCliCommand((const char*)cobs_buffer);
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

  if (g_live_on) {
    uint32_t now = millis();
    if (now - g_last_live_ms >= g_live_period_ms) {
      g_last_live_ms = now;
      liveOutput();
    }
  }
  if (g_led_mode == LEDMODE_STATE) ledStateUpdate();

  if (millis() - last_stream_ms >= stream_interval_ms) {
    send_telemetry();
    last_stream_ms = millis();
  }

  delay(5);
}
