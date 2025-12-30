#line 1 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
/*
  ESP32-S3 + Dual BME688 (AMB/ENV) using Bosch BSEC2 + Bosch BME68x library
  - No Adafruit libs required
  - CLI: help, scan, i2c, live, status, probe, regs, dbg, rate, fmt
         poster, morgio, coin, bump, power, 1up
         led mode off|state|manual, led rgb <r> <g> <b>

  - Auto-detects BME688 at 0x77 (AMB) and 0x76 (ENV)
  - Separate BSEC2 instances per sensor (separate memory/state)
  - Per-sensor output lines (no nulls); NDJSON option for Azure
  - Pressure scaling auto-normalized

  Boot reliability:
  - Adds a boot wait so Serial Monitor has time to attach (ESP32-S3 USB CDC quirks)
  - 'poster' command reprints the screen any time

  Indicator lights:
  - Uses AO_PINS[0..2] (12/13/14) as PWM RGB indicators (0-255)
  - Default: LED state machine enabled (led mode state)
    - GREEN  : both sensors begin ok + subscription ok
    - YELLOW : begin ok but subscription failed for any
    - BLUE   : initializing / waiting for first valid readings
    - RED    : no sensors or begin failed
*/

#include <Arduino.h>
#include <Wire.h>
#include "bsec2.h"
#include "soc/rtc_cntl_reg.h" // For brownout detector disable

#ifndef ARRAY_LEN
  #define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

// -------------------------
// OPTIONAL external blob header (sketch tab)
// -------------------------
#define USE_EXTERNAL_BLOB 0  // set to 1 ONLY if you're sure blob matches your BSEC2 lib

#if USE_EXTERNAL_BLOB
  #include "bsec_selectivity.h" // must define: bsec_selectivity_config[] and *_len
  static const uint8_t*  CFG_PTR = bsec_selectivity_config;
  static const uint32_t  CFG_LEN = (uint32_t)bsec_selectivity_config_len;
#else
  static const uint8_t*  CFG_PTR = nullptr;
  static const uint32_t  CFG_LEN = 0;
#endif

// -------------------------
// Pinout (your A-side notes)
// -------------------------
static const int PIN_SDA   = 5;
static const int PIN_SCL   = 4;

static const int AIN_PINS[4] = {6, 7, 10, 11};

// AO pins double as “indicator lights” (PWM RGB)
static const int AO_PINS[3]  = {12, 13, 14}; // R,G,B channels (0-255)

static const int BUZZER_PIN  = 16;

// -------------------------
// Boot / Serial behavior
// -------------------------
// If Serial Monitor connects late, you miss the first prints.
// This gives you a deterministic window to catch the banner.
static const uint32_t BOOT_SERIAL_WAIT_MS = 1800;

// -------------------------
// I2C defaults
// -------------------------
static uint8_t  gSda   = PIN_SDA;
static uint8_t  gScl   = PIN_SCL;
static uint32_t gI2cHz = 100000;

// -------------------------
// Output format
// -------------------------
enum OutputFormat : uint8_t {
  FMT_LINES = 0,
  FMT_NDJSON = 1
};
static OutputFormat gFmt = FMT_LINES;

// -------------------------
// Debug toggles
// -------------------------
static bool     gDebug = true;
static uint32_t gLastDbgPrintMs = 0;
static uint32_t gDbgPeriodMs = 3000;

// -------------------------
// UI state
// -------------------------
static bool     gLive = true;
static uint32_t gLastLiveMs = 0;
static uint32_t gLivePeriodMs = 1000;

// -------------------------
// Data structs (MUST be above functions that use them)
// -------------------------
struct AmbReading {
  bool     valid = false;
  uint32_t t_ms  = 0;

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
};

struct SensorSlot {
  const char* name;
  uint8_t addr;

  bool present = false;

  Bsec2 bsec;
  uint8_t mem[BSEC_INSTANCE_SIZE];

  bool beginOk = false;
  bool subOk   = false;
  bool cfgOk   = false;
  int  lastStatus = 0;

  float sampleRate = BSEC_SAMPLE_RATE_LP;

  AmbReading r;
};

// Two fixed “slots” by address
static SensorSlot S_AMB = { "AMB", 0x77 };
static SensorSlot S_ENV = { "ENV", 0x76 };

// -------------------------
// Blocky POST screen (ASCII-only)
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

// -------------------------
// Indicator lights
// -------------------------
enum LedMode : uint8_t { LEDMODE_OFF = 0, LEDMODE_STATE = 1, LEDMODE_MANUAL = 2 };
static LedMode gLedMode = LEDMODE_STATE;

static uint8_t gLedR = 0, gLedG = 0, gLedB = 0; // manual values

#line 179 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void ledWriteRGB(uint8_t r, uint8_t g, uint8_t b);
#line 185 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void ledAllOff();
#line 189 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void ledBootBlink();
#line 196 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void ledStateUpdate();
#line 255 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printPoster();
#line 264 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static int noteHz(const char* n);
#line 316 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void sfxCoin();
#line 317 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void sfxBump();
#line 319 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void sfxPowerUp();
#line 324 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void sfx1Upish();
#line 329 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void sfxSuperMorgIOBoot();
#line 349 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printCoreInfo();
#line 368 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* out, size_t n);
#line 378 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t &val);
#line 382 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printI2cScan();
#line 396 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static float pressureToHpa(float p);
#line 420 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static SensorSlot* pickSlotByName(const String &s);
#line 427 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void snapStatus(SensorSlot &s, const char* tag);
#line 435 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printOneSensorLine(const SensorSlot &s);
#line 454 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printOneSensorNdjson(const SensorSlot &s);
#line 479 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void liveOutput();
#line 494 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void slotCallbackCommon(SensorSlot &s, const bme68xData data, const bsecOutputs outputs);
#line 552 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static bool slotInit(SensorSlot &s);
#line 633 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static bool initAll();
#line 674 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printHelp();
#line 695 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void printStatus();
#line 728 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void handleCommand(String line);
#line 865 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
void setup();
#line 906 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void runSlot(SensorSlot &s);
#line 911 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
void loop();
#line 179 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA.ino"
static void ledWriteRGB(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(AO_PINS[0], r);
  analogWrite(AO_PINS[1], g);
  analogWrite(AO_PINS[2], b);
}

static void ledAllOff() {
  ledWriteRGB(0, 0, 0);
}

static void ledBootBlink() {
  ledWriteRGB(0, 0, 50); delay(80);
  ledWriteRGB(0, 50, 0); delay(80);
  ledWriteRGB(50, 0, 0); delay(60);
  ledWriteRGB(0, 0, 0);  delay(60);
}

static void ledStateUpdate() {
  if (gLedMode == LEDMODE_OFF) {
    ledAllOff();
    return;
  }
  if (gLedMode == LEDMODE_MANUAL) {
    ledWriteRGB(gLedR, gLedG, gLedB);
    return;
  }

  const bool ambPresent = S_AMB.present;
  const bool envPresent = S_ENV.present;
  const bool anyPresent = ambPresent || envPresent;

  const bool ambBegin = S_AMB.beginOk;
  const bool envBegin = S_ENV.beginOk;
  const bool anyBeginFail = (ambPresent && !ambBegin) || (envPresent && !envBegin);

  const bool ambSub = S_AMB.subOk;
  const bool envSub = S_ENV.subOk;
  const bool anySubFail = (ambPresent && ambBegin && !ambSub) || (envPresent && envBegin && !envSub);

  const bool ambData = S_AMB.r.valid;
  const bool envData = S_ENV.r.valid;
  const bool initPhase = anyPresent && ((ambPresent && !ambData) || (envPresent && !envData));

  if (!anyPresent) {
    uint8_t v = (uint8_t)(20 + (millis() / 6) % 80);
    ledWriteRGB(v, 0, 0);
    return;
  }

  if (anyBeginFail) {
    bool on = ((millis() / 250) % 2) == 0;
    ledWriteRGB(on ? 120 : 0, 0, 0);
    return;
  }

  if (initPhase) {
    uint8_t v = (uint8_t)(30 + (millis() / 8) % 90);
    ledWriteRGB(0, 0, v);
    return;
  }

  if (anySubFail) {
    ledWriteRGB(80, 60, 0);
    return;
  }

  ledWriteRGB(0, 90, 0);
}

// -------------------------
// Helpers
// -------------------------
static void beepOnce(int freqHz = 2000, int ms = 60) {
  tone(BUZZER_PIN, freqHz, ms);
}

static void printPoster() {
  Serial.println();
  Serial.print(kPoster);
  Serial.println();
}

// -------------------------
// SuperMorgIO: retro buzzer music kit (original “platformer vibe”)
// -------------------------
static int noteHz(const char* n) {
  if (!n) return 0;
  if (!strcmp(n, "REST")) return 0;

  if (!strcmp(n, "C4")) return 262;
  if (!strcmp(n, "CS4")) return 277;
  if (!strcmp(n, "D4")) return 294;
  if (!strcmp(n, "DS4")) return 311;
  if (!strcmp(n, "E4")) return 330;
  if (!strcmp(n, "F4")) return 349;
  if (!strcmp(n, "FS4")) return 370;
  if (!strcmp(n, "G4")) return 392;
  if (!strcmp(n, "GS4")) return 415;
  if (!strcmp(n, "A4")) return 440;
  if (!strcmp(n, "AS4")) return 466;
  if (!strcmp(n, "B4")) return 494;

  if (!strcmp(n, "C5")) return 523;
  if (!strcmp(n, "CS5")) return 554;
  if (!strcmp(n, "D5")) return 587;
  if (!strcmp(n, "DS5")) return 622;
  if (!strcmp(n, "E5")) return 659;
  if (!strcmp(n, "F5")) return 698;
  if (!strcmp(n, "FS5")) return 740;
  if (!strcmp(n, "G5")) return 784;
  if (!strcmp(n, "GS5")) return 831;
  if (!strcmp(n, "A5")) return 880;
  if (!strcmp(n, "AS5")) return 932;
  if (!strcmp(n, "B5")) return 988;

  if (!strcmp(n, "C6")) return 1047;
  if (!strcmp(n, "D6")) return 1175;
  if (!strcmp(n, "E6")) return 1319;

  return 0;
}

static void playTone(int hz, int ms, int gapMs = 8) {
  if (hz > 0) {
    tone(BUZZER_PIN, hz);
    delay(ms);
    noTone(BUZZER_PIN);
  } else {
    delay(ms);
  }
  if (gapMs > 0) delay(gapMs);
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
// Core info
// -------------------------
static void printCoreInfo() {
  Serial.println("--- CORE / SDK ---");
#if defined(ESP_ARDUINO_VERSION_MAJOR)
  Serial.printf("Arduino-ESP32 core: %d.%d.%d\n",
                ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH);
#else
  Serial.println("Arduino-ESP32 core: (ESP_ARDUINO_VERSION_* not defined)");
#endif
  Serial.printf("ESP SDK: %s\n", ESP.getSdkVersion());
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("CPU freq: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.println("---------------");
}

// -------------------------
// I2C helpers
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

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t &val) {
  return i2cReadBytes(addr, reg, &val, 1);
}

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

static float pressureToHpa(float p) {
  if (!isfinite(p) || p <= 0) return NAN;
  if (p > 20000.0f) return p / 100.0f;  // Pa -> hPa
  if (p > 2000.0f)  return p / 10.0f;   // deci-hPa -> hPa
  if (p > 200.0f)   return p;           // already hPa
  if (p > 20.0f)    return p * 10.0f;   // kPa -> hPa
  return p * 1000.0f;                   // bar-ish -> hPa
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

static SensorSlot* pickSlotByName(const String &s) {
  String t = s; t.toLowerCase();
  if (t == "amb") return &S_AMB;
  if (t == "env") return &S_ENV;
  return nullptr;
}

static void snapStatus(SensorSlot &s, const char* tag) {
  s.lastStatus = (int)s.bsec.status;
  Serial.printf("[%s %s] BSEC status=%d\n", s.name, tag, s.lastStatus);
}

// -------------------------
// Output formatting
// -------------------------
static void printOneSensorLine(const SensorSlot &s) {
  if (!s.present || !s.r.valid) return;

  uint32_t age = millis() - s.r.t_ms;

  Serial.printf("%s addr=0x%02X age=%lums T=%.2fC RH=%.2f%% P=%.2fhPa(raw=%.2f) Gas=%.0fOhm",
                s.name, s.addr, (unsigned long)age,
                s.r.tC, s.r.rh, s.r.p_hPa, s.r.p_raw, s.r.gas_ohm);

  if (!isnan(s.r.iaq)) {
    Serial.printf(" IAQ=%.2f acc=%.0f sIAQ=%.2f CO2eq=%.2f VOC=%.2f",
                  s.r.iaq, s.r.iaqAccuracy, s.r.staticIaq, s.r.co2eq, s.r.voc);
  } else {
    Serial.print(" IAQ=N/A");
  }

  Serial.println();
}

static void printOneSensorNdjson(const SensorSlot &s) {
  if (!s.present || !s.r.valid) return;

  uint32_t ts = millis();

  Serial.print("{\"ts_ms\":"); Serial.print(ts);
  Serial.print(",\"sensor\":\""); Serial.print(s.name); Serial.print("\"");
  Serial.print(",\"addr\":\"0x"); Serial.printf("%02X", s.addr); Serial.print("\"");

  Serial.print(",\"tC\":"); Serial.print(s.r.tC, 2);
  Serial.print(",\"rh\":"); Serial.print(s.r.rh, 2);
  Serial.print(",\"p_hPa\":"); Serial.print(s.r.p_hPa, 2);
  Serial.print(",\"gas\":"); Serial.print(s.r.gas_ohm, 0);

  if (!isnan(s.r.iaq)) {
    Serial.print(",\"iaq\":"); Serial.print(s.r.iaq, 2);
    Serial.print(",\"acc\":"); Serial.print(s.r.iaqAccuracy, 0);
    Serial.print(",\"siaq\":"); Serial.print(s.r.staticIaq, 2);
    Serial.print(",\"co2eq\":"); Serial.print(s.r.co2eq, 2);
    Serial.print(",\"voc\":"); Serial.print(s.r.voc, 2);
  }

  Serial.println("}");
}

static void liveOutput() {
  if (gFmt == FMT_LINES) {
    Serial.println("===== LIVE =====");
    if (S_AMB.present) printOneSensorLine(S_AMB);
    if (S_ENV.present) printOneSensorLine(S_ENV);
    Serial.println("===============");
  } else {
    if (S_AMB.present) printOneSensorNdjson(S_AMB);
    if (S_ENV.present) printOneSensorNdjson(S_ENV);
  }
}

// -------------------------
// BSEC callbacks (one per slot)
// -------------------------
static void slotCallbackCommon(SensorSlot &s, const bme68xData data, const bsecOutputs outputs) {
  s.r.valid = true;
  s.r.t_ms  = millis();

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

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData &o = outputs.output[i];
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
      default:
        break;
    }
  }

  if (gDebug && (millis() - gLastDbgPrintMs >= gDbgPeriodMs)) {
    gLastDbgPrintMs = millis();
    Serial.printf("[DBG %s] rawP=%.2f => P=%.2f hPa | T=%.2f RH=%.2f Gas=%.0f | iaq=%s acc=%.0f\n",
                  s.name, s.r.p_raw, s.r.p_hPa, s.r.tC, s.r.rh, s.r.gas_ohm,
                  isnan(s.r.iaq) ? "N/A" : String(s.r.iaq, 2).c_str(),
                  isnan(s.r.iaqAccuracy) ? -1.0f : s.r.iaqAccuracy);
  }
}

static void cbAMB(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_AMB, data, outputs);
}
static void cbENV(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_ENV, data, outputs);
}

// -------------------------
// Sensor init
// -------------------------
static bool slotInit(SensorSlot &s) {
  s.present = false;
  s.beginOk = s.subOk = s.cfgOk = false;
  s.lastStatus = 0;
  s.r = AmbReading();

  // Feed watchdog during init
  yield();
  
  Wire.beginTransmission(s.addr);
  if (Wire.endTransmission() != 0) {
    Serial.printf("[%s] not found @ 0x%02X\n", s.name, s.addr);
    return false;
  }
  s.present = true;

  yield(); // Feed watchdog
  
  printBmeIdentity(s.addr, 2);
  
  yield(); // Feed watchdog

  s.bsec.allocateMemory(s.mem);
  
  yield(); // Feed watchdog

  Serial.printf("[%s] begin(0x%02X)...\n", s.name, s.addr);
  if (!s.bsec.begin(s.addr, Wire)) {
    Serial.printf("[%s] begin FAILED\n", s.name);
    snapStatus(s, "begin");
    return false;
  }
  s.beginOk = true;
  Serial.printf("[%s] begin OK\n", s.name);
  snapStatus(s, "begin");

  s.bsec.setTemperatureOffset(0.0f);

  if (CFG_PTR && CFG_LEN) {
    Serial.printf("[%s] setConfig(blob %lu bytes)...\n", s.name, (unsigned long)CFG_LEN);
    if (!s.bsec.setConfig(CFG_PTR)) {
      Serial.printf("[%s] setConfig FAILED\n", s.name);
      snapStatus(s, "setConfig");
      s.cfgOk = false;
    } else {
      Serial.printf("[%s] setConfig OK\n", s.name);
      snapStatus(s, "setConfig");
      s.cfgOk = true;
    }
  } else {
    Serial.printf("[%s] setConfig skipped\n", s.name);
    snapStatus(s, "setConfig-skip");
    s.cfgOk = false;
  }

  if (s.addr == 0x77) s.bsec.attachCallback(cbAMB);
  if (s.addr == 0x76) s.bsec.attachCallback(cbENV);

  bsecSensor list[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
  };

  Serial.printf("[%s] updateSubscription(%s)...\n", s.name,
                (s.sampleRate == BSEC_SAMPLE_RATE_LP) ? "LP" : "ULP");

  if (s.bsec.updateSubscription(list, (uint8_t)ARRAY_LEN(list), s.sampleRate)) {
    Serial.printf("[%s] updateSubscription OK\n", s.name);
    snapStatus(s, "sub");
    s.subOk = true;
  } else {
    Serial.printf("[%s] updateSubscription FAILED\n", s.name);
    snapStatus(s, "sub-fail");
    s.subOk = false;
  }

  return true;
}

static bool initAll() {
  Wire.end();
  Wire.begin(gSda, gScl);
  Wire.setClock(gI2cHz);

  Serial.printf("I2C: SDA=%d SCL=%d @ %lu Hz\n", gSda, gScl, (unsigned long)gI2cHz);
  printI2cScan();

  // init phase indicator
  if (gLedMode == LEDMODE_STATE) ledWriteRGB(0, 0, 90);

  bool any = false;
  if (slotInit(S_AMB)) any = true;
  if (slotInit(S_ENV)) any = true;

  Serial.println("----- STARTER BUNDLE -----");
  Serial.printf("I2C: SDA=%d SCL=%d @ %lu Hz\n", gSda, gScl, (unsigned long)gI2cHz);
  Serial.printf("CFG blob len: %lu\n", (unsigned long)CFG_LEN);

  Serial.printf("AMB: present=%s addr=0x%02X begin=%s sub=%s status=%d rate=%s\n",
                S_AMB.present ? "YES":"NO", S_AMB.addr,
                S_AMB.beginOk ? "OK":"FAIL",
                S_AMB.subOk ? "OK":"FAIL",
                S_AMB.lastStatus,
                (S_AMB.sampleRate == BSEC_SAMPLE_RATE_LP) ? "LP":"ULP");

  Serial.printf("ENV: present=%s addr=0x%02X begin=%s sub=%s status=%d rate=%s\n",
                S_ENV.present ? "YES":"NO", S_ENV.addr,
                S_ENV.beginOk ? "OK":"FAIL",
                S_ENV.subOk ? "OK":"FAIL",
                S_ENV.lastStatus,
                (S_ENV.sampleRate == BSEC_SAMPLE_RATE_LP) ? "LP":"ULP");

  Serial.println("--------------------------");

  return any;
}

// -------------------------
// CLI
// -------------------------
static void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  help                               - this help");
  Serial.println("  poster                             - reprint SuperMorgIO POST screen");
  Serial.println("  morgio                             - play SuperMorgIO boot jingle");
  Serial.println("  coin | bump | power | 1up          - tiny retro SFX");
  Serial.println("  scan                               - I2C scan");
  Serial.println("  i2c <sda> <scl> [hz]               - set I2C pins + optional clock");
  Serial.println("  live on|off                        - periodic live output");
  Serial.println("  status                             - init stages + readings");
  Serial.println("  probe amb|env [n]                  - read chip_id/variant_id (default n=3)");
  Serial.println("  regs amb|env                       - read chip_id/variant_id once");
  Serial.println("  dbg on|off                         - toggle callback debug prints");
  Serial.println("  fmt lines|json                     - output format (human lines or NDJSON)");
  Serial.println("  rate amb|env lp|ulp                - set per-sensor sample rate and re-init");
  Serial.println("  led mode off|state|manual          - indicator lights mode");
  Serial.println("  led rgb <r 0-255> <g 0-255> <b 0-255> - set manual RGB (auto -> manual)");
  Serial.println();
}

static void printStatus() {
  printCoreInfo();

  Serial.printf("I2C: SDA=%d SCL=%d @ %lu Hz\n", gSda, gScl, (unsigned long)gI2cHz);
  Serial.printf("Format=%s Debug=%s Live=%s period=%lums\n",
                (gFmt == FMT_LINES) ? "lines":"json",
                gDebug ? "on":"off",
                gLive ? "on":"off",
                (unsigned long)gLivePeriodMs);

  const char* lm = (gLedMode == LEDMODE_OFF) ? "off" : (gLedMode == LEDMODE_STATE) ? "state" : "manual";
  Serial.printf("LED mode=%s  manual rgb=%u,%u,%u\n", lm, gLedR, gLedG, gLedB);

  Serial.printf("AMB: present=%s addr=0x%02X begin=%s sub=%s status=%d rate=%s\n",
                S_AMB.present ? "YES":"NO", S_AMB.addr,
                S_AMB.beginOk ? "OK":"FAIL",
                S_AMB.subOk ? "OK":"FAIL",
                S_AMB.lastStatus,
                (S_AMB.sampleRate == BSEC_SAMPLE_RATE_LP) ? "LP":"ULP");

  Serial.printf("ENV: present=%s addr=0x%02X begin=%s sub=%s status=%d rate=%s\n",
                S_ENV.present ? "YES":"NO", S_ENV.addr,
                S_ENV.beginOk ? "OK":"FAIL",
                S_ENV.subOk ? "OK":"FAIL",
                S_ENV.lastStatus,
                (S_ENV.sampleRate == BSEC_SAMPLE_RATE_LP) ? "LP":"ULP");

  if (S_AMB.present && S_AMB.r.valid) printOneSensorLine(S_AMB);
  if (S_ENV.present && S_ENV.r.valid) printOneSensorLine(S_ENV);
}

static String gLine;

static void handleCommand(String line) {
  line.trim();
  if (!line.length()) return;

  auto nextTok = [&](int &pos) -> String {
    while (pos < (int)line.length() && isspace((unsigned char)line[pos])) pos++;
    int start = pos;
    while (pos < (int)line.length() && !isspace((unsigned char)line[pos])) pos++;
    return line.substring(start, pos);
  };

  int pos = 0;
  String cmd = nextTok(pos);
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") { printHelp(); return; }
  if (cmd == "poster") { printPoster(); return; }

  if (cmd == "morgio" || cmd == "morg") { sfxSuperMorgIOBoot(); return; }
  if (cmd == "coin") { sfxCoin(); return; }
  if (cmd == "bump") { sfxBump(); return; }
  if (cmd == "power") { sfxPowerUp(); return; }
  if (cmd == "1up") { sfx1Upish(); return; }

  if (cmd == "scan") { printI2cScan(); return; }

  if (cmd == "led") {
    String sub = nextTok(pos); sub.toLowerCase();
    if (sub == "mode") {
      String m = nextTok(pos); m.toLowerCase();
      if (m == "off") gLedMode = LEDMODE_OFF;
      else if (m == "state") gLedMode = LEDMODE_STATE;
      else if (m == "manual") gLedMode = LEDMODE_MANUAL;
      Serial.printf("LED mode set to %s\n", (gLedMode==LEDMODE_OFF)?"off":(gLedMode==LEDMODE_STATE)?"state":"manual");
      return;
    }
    if (sub == "rgb") {
      String sr = nextTok(pos), sg = nextTok(pos), sb = nextTok(pos);
      if (!sr.length() || !sg.length() || !sb.length()) {
        Serial.println("Usage: led rgb <r> <g> <b>");
        return;
      }
      gLedR = (uint8_t)constrain(sr.toInt(), 0, 255);
      gLedG = (uint8_t)constrain(sg.toInt(), 0, 255);
      gLedB = (uint8_t)constrain(sb.toInt(), 0, 255);
      gLedMode = LEDMODE_MANUAL;
      Serial.printf("LED manual rgb=%u,%u,%u\n", gLedR, gLedG, gLedB);
      return;
    }
    Serial.println("Usage: led mode off|state|manual  OR  led rgb <r> <g> <b>");
    return;
  }

  if (cmd == "i2c") {
    String sSda = nextTok(pos);
    String sScl = nextTok(pos);
    String sHz  = nextTok(pos);
    if (sSda.length()) gSda = (uint8_t)sSda.toInt();
    if (sScl.length()) gScl = (uint8_t)sScl.toInt();
    if (sHz.length())  gI2cHz = (uint32_t)sHz.toInt();
    Serial.println("Re-initializing...");
    initAll();
    return;
  }

  if (cmd == "live") {
    String v = nextTok(pos);
    v.toLowerCase();
    if (v == "on") gLive = true;
    else if (v == "off") gLive = false;
    Serial.printf("LIVE=%s\n", gLive ? "on" : "off");
    return;
  }

  if (cmd == "status") { printStatus(); return; }

  if (cmd == "dbg") {
    String v = nextTok(pos);
    v.toLowerCase();
    if (v == "on") gDebug = true;
    else if (v == "off") gDebug = false;
    Serial.printf("DBG=%s\n", gDebug ? "on" : "off");
    return;
  }

  if (cmd == "fmt") {
    String v = nextTok(pos);
    v.toLowerCase();
    if (v == "lines") gFmt = FMT_LINES;
    else if (v == "json") gFmt = FMT_NDJSON;
    Serial.printf("FMT=%s\n", (gFmt == FMT_LINES) ? "lines" : "json");
    return;
  }

  if (cmd == "probe" || cmd == "regs") {
    String which = nextTok(pos);
    SensorSlot* s = pickSlotByName(which);
    if (!s) {
      Serial.println("Usage: probe amb|env [n]  OR regs amb|env");
      return;
    }
    int n = 1;
    if (cmd == "probe") {
      String sn = nextTok(pos);
      n = sn.length() ? sn.toInt() : 3;
      n = constrain(n, 1, 20);
    }
    printBmeIdentity(s->addr, n);
    return;
  }

  if (cmd == "rate") {
    String which = nextTok(pos);
    String rate  = nextTok(pos);
    SensorSlot* s = pickSlotByName(which);
    if (!s) {
      Serial.println("Usage: rate amb|env lp|ulp");
      return;
    }
    rate.toLowerCase();
    if (rate == "lp") s->sampleRate = BSEC_SAMPLE_RATE_LP;
    else if (rate == "ulp") s->sampleRate = BSEC_SAMPLE_RATE_ULP;
    else {
      Serial.println("rate must be lp or ulp");
      return;
    }
    Serial.println("Re-initializing...");
    initAll();
    return;
  }

  Serial.printf("Unknown cmd: %s (try 'help')\n", cmd.c_str());
}

// -------------------------
// Arduino setup/loop
// -------------------------
void setup() {
  // Disable brownout detector to prevent resets during sensor init
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200);

  // Give the host time to attach the monitor after reset/upload.
  // This is the big “serial doesn’t print” fix.
  delay(BOOT_SERIAL_WAIT_MS);

  for (int i = 0; i < 4; i++) pinMode(AIN_PINS[i], INPUT);

  // Setup “indicator lights” on AO pins
  for (int i = 0; i < 3; i++) {
    pinMode(AO_PINS[i], OUTPUT);
    analogWriteResolution(AO_PINS[i], 8);  // esp32 core 3.2.0 requires pin + bits
    analogWrite(AO_PINS[i], 0);
  }

  ledBootBlink();

  // Boot POST + jingle
  printPoster();
  sfxSuperMorgIOBoot();

  Serial.println("ESP32AB A-SIDE Dual BME688 (AMB/ENV) + BSEC2");
  printCoreInfo();
  printHelp();

  // TEMPORARILY DISABLE SENSOR INIT TO TEST
  // If this works, sensors are causing the crash
  Serial.println("\n[TEST MODE] Sensor initialization DISABLED");
  Serial.println("If you see this, firmware boots OK without sensors.");
  Serial.println("Uncomment initAll() below to enable sensors.");
  Serial.flush();
  
  // initAll(); // COMMENTED OUT FOR TESTING
}

static void runSlot(SensorSlot &s) {
  if (!s.present || !s.beginOk) return;
  (void)s.bsec.run();
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gLine;
      gLine = "";
      handleCommand(line);
    } else {
      if (gLine.length() < 200) gLine += c;
    }
  }

  runSlot(S_AMB);
  runSlot(S_ENV);

  // Keep indicator lights alive
  ledStateUpdate();

  if (gLive && millis() - gLastLiveMs >= gLivePeriodMs) {
    gLastLiveMs = millis();
    liveOutput();
  }
}

#line 1 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA_MINIMAL_TEST.ino"
/*
 * MINIMAL TEST - MycoBrain Side-A
 * This is the simplest possible firmware to test if hardware is OK
 * If this works, the issue is in sensor/BSEC code
 * If this doe1n't work, there's a hardware problem
 */

#include "soc/rtc_cntl_reg.h" // For brownout detector disable

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  
  // Wait for serial
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(10);
  }
  
  delay(2000); // Extra delay for Serial Monitor
  
  Serial.println("\n\n========================================");
  Serial.println("MINIMAL TEST - Hardware Check");
  Serial.println("========================================");
  Serial.println("If you see this, hardware is OK!");
  Serial.println("Device is NOT resetting.");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  
  // Print every 2 seconds
  if (now - lastPrint >= 2000) {
    Serial.print("Loop running: ");
    Serial.print(now / 1000);
    Serial.println(" seconds");
    Serial.flush();
    lastPrint = now;
  }
  
  delay(100);
  yield(); // Feed watchdog
}


#line 1 "C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MAS\\mycosoft-mas\\firmware\\MycoBrain_SideA\\MycoBrain_SideA_NO_SENSORS.ino"
/*
 * MycoBrain Side-A - WITHOUT SENSORS
 * This version skips all sensor initialization
 * Use this to test if sensors are causing the crash
 */

#include <Arduino.h>
#include "soc/rtc_cntl_reg.h" // For brownout detector disable

// Pin definitions
static const int BUZZER_PIN = 16;
static const int AO_PINS[3] = {12, 13, 14}; // R,G,B

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  pinMode(BUZZER_PIN, OUTPUT);
  
  Serial.begin(115200);
  
  // Wait for serial
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(10);
  }
  
  delay(2000); // Extra delay
  
  // Setup LED pins
  for (int i = 0; i < 3; i++) {
    pinMode(AO_PINS[i], OUTPUT);
    analogWriteResolution(AO_PINS[i], 8);
    analogWrite(AO_PINS[i], 0);
  }
  
  Serial.println("\n\n========================================");
  Serial.println("MycoBrain Side-A - NO SENSORS TEST");
  Serial.println("========================================");
  Serial.println("This firmware skips all sensor initialization.");
  Serial.println("If you see this, the firmware boots OK!");
  Serial.println("The crash is likely in sensor/BSEC code.");
  Serial.println("========================================");
  Serial.flush();
  
  // Blink LED
  analogWrite(AO_PINS[0], 0);
  analogWrite(AO_PINS[1], 255);
  analogWrite(AO_PINS[2], 0);
  delay(500);
  analogWrite(AO_PINS[0], 0);
  analogWrite(AO_PINS[1], 0);
  analogWrite(AO_PINS[2], 0);
  
  // Beep
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  
  Serial.println("\nDevice ready! No sensors initialized.");
  Serial.println("Type 'help' for commands (if implemented).");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  
  // Print status every 5 seconds
  if (now - lastPrint >= 5000) {
    Serial.print("Uptime: ");
    Serial.print(now / 1000);
    Serial.println(" seconds - Device running OK!");
    Serial.flush();
    lastPrint = now;
  }
  
  delay(100);
  yield(); // Feed watchdog
}


