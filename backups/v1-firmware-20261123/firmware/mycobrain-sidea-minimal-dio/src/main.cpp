/*
 * MycoBrain Recovery Operator Firmware v0.9-j18-bmm150
 * BSEC2 dual BME688 + HLK-LD2450 mmWave (UART2 / J18 GPIO9 RX / GPIO8 TX)
 * + BMM150 magnetometer on I2C (scan 0x10-0x13; shared bus with BME688 @ 0x77)
 *
 * LD2450 wiring (Side A, J18 UART2 Connect — schematic-correct):
 *   Manual power: 5V (>200 mA). IO level 3.3V (UART safe).
 *   pin3 RXD2 <- LD2450 TX -> GPIO9 (ESP RX), pin4 TXD2 -> LD2450 RX -> GPIO8 (ESP TX)
 *   Do NOT use GPIO 17/18 — Side A<->Side B crossover (must stay free before Side B revive).
 *   Never use GPIO 19/20 (USB D+/D-). I2C stays explicit Wire.begin(5,4).
 * BMM150: never hot-plug on powered board (VDDIO damage). Publish raw uT only;
 *   tiltCompensated=false; NEVER emit headingDeg without tilt compensation.
 * Protocol LD2450: 256000 8N1, header AA FF 03 00, footer 55 CC,
 *   x/y/speed are SIGN-MAGNITUDE (MSB = sign), not two's complement.
 */
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "bsec2.h"
#include "bsec_selectivity.h"

static const uint8_t* CFG_PTR = bsec_selectivity_config;
static const uint32_t CFG_LEN = (uint32_t)bsec_selectivity_config_len;

#ifndef ARRAY_LEN
  #define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

static const int LED_PIN = 15;
static const int PIXEL_COUNT = 1;
Adafruit_NeoPixel pixels(PIXEL_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static uint8_t led_r = 0, led_g = 0, led_b = 180;  // default boot blue
static const int BUZZER_PIN = 16;
static const int I2C_SDA = 5;
static const int I2C_SCL = 4;

// HLK-LD2450 on UART2 / J18 — sensor TX→ESP RX (GPIO9), sensor RX→ESP TX (GPIO8)
static const int LD2450_RX_PIN = 9;
static const int LD2450_TX_PIN = 8;
static const uint32_t LD2450_BAUD = 256000;
static const float LD2450_MOUNT_BEARING_DEG = 0.0f;
static const float LD2450_MAX_RANGE_M = 6.0f;
static const float LD2450_FOV_DEG = 120.0f;
static const uint8_t LD2450_MAX_TARGETS = 3;
static const uint8_t LD2450_FRAME_LEN = 30;  // 4 hdr + 24 tgt + 2 ftr

static const char* FW_VERSION = "recovery-operator-bsec2-v0.9-j18-bmm150";
static const char* DEVICE_ID = "mycobrain-sidea-10b41d";
static const char* DEVICE_ROLE = "side_a";
static const char* CONFIG_VER = "bsec2-default-lp";
static const float TEMP_OFFSET_C = 0.0f;
static const char* SAMPLE_RATE_MODE = "lp";
static const uint8_t CLASS_COUNT = 4;

static unsigned long lastBeat = 0;
static unsigned long lastSensorPrint = 0;
static unsigned long lastMmwavePrint = 0;
static unsigned long lastMagPrint = 0;
static String buf;
static bool buzzerReady = false;
static uint32_t bootCount = 0;
static bool ld2450Ready = false;
static uint32_t ld2450FramesOk = 0;
static uint32_t ld2450LastFrameMs = 0;

// BMM150 (Bosch) — address strapped 0x10..0x13
static const uint8_t BMM150_CHIP_ID_REG = 0x40;
static const uint8_t BMM150_CHIP_ID_VAL = 0x32;
static const uint8_t BMM150_DATA_X_LSB = 0x42;
static const uint8_t BMM150_POWER_CTRL = 0x4B;
static const uint8_t BMM150_OP_MODE = 0x4C;
static bool bmm150Ready = false;
static uint8_t bmm150Addr = 0;
static float bmm150_uT_x = NAN;
static float bmm150_uT_y = NAN;
static float bmm150_uT_z = NAN;
static uint32_t bmm150LastMs = 0;
static uint32_t bmm150SamplesOk = 0;

// ---- BSEC2 reading struct: all outputs the CTO requested ----

struct SensorReading {
  bool valid = false;
  uint32_t t_ms = 0;

  // Raw sensor (ambient, pre-compensation)
  float ambient_temperature_c = NAN;
  float ambient_humidity_pct = NAN;
  float pressure_hpa = NAN;
  float gas_resistance_ohm = NAN;

  // BSEC heat-compensated
  float temperature_c_comp = NAN;
  float humidity_pct_comp = NAN;
  float gas_resistance_ohm_comp = NAN;

  // IAQ / gas analysis
  float iaq = NAN;
  float iaq_static = NAN;
  float iaq_accuracy = NAN;
  float eco2_ppm = NAN;
  float bvoc_ppm = NAN;
  float gas_percentage = NAN;

  // Sensor status
  float stabilization_status = NAN;
  float run_in_status = NAN;
  float output_accuracy = NAN;
  int8_t bsec_status_code = 0;
  int8_t bme68x_status_code = 0;

  // Gas estimates (selectivity config)
  float gas_estimate_1 = NAN;
  float gas_estimate_2 = NAN;
  float gas_estimate_3 = NAN;
  float gas_estimate_4 = NAN;
  float gas_estimate_accuracy = NAN;

  // Raw BME68x flags
  bool new_data = false;
  bool measuring = false;
  bool gas_measuring = false;
  uint8_t gas_meas_index = 0;
  uint8_t sub_meas_index = 0;
  bool gas_valid = false;
  bool heat_stable = false;
};

struct SensorSlot {
  const char* name = nullptr;
  uint8_t addr = 0;
  bool present = false;
  Bsec2 bsec;
  uint8_t mem[BSEC_INSTANCE_SIZE];
  bool beginOk = false;
  bool subOk = false;
  SensorReading r;
  uint8_t chip_id = 0;
  uint8_t variant_id = 0;

  void init(const char* n, uint8_t a) {
    name = n;
    addr = a;
  }
};

static SensorSlot S_AMB;
static SensorSlot S_ENV;
static SensorSlot* sensorPtrs[2] = { &S_AMB, &S_ENV };
static const int SENSOR_COUNT = 2;

static bool slotInit(SensorSlot& s);

// ---- HLK-LD2450 mmWave (sign-magnitude) ----

struct Ld2450Target {
  bool valid = false;
  int16_t xMm = 0;
  int16_t yMm = 0;
  int16_t speedCmS = 0;
  uint16_t distanceResMm = 0;
  float bearingDeg = NAN;
  float rangeM = NAN;
};

static Ld2450Target ld2450Targets[LD2450_MAX_TARGETS];
static uint8_t ld2450RxBuf[LD2450_FRAME_LEN];
static uint8_t ld2450RxLen = 0;

/** LD2450 encodes x/y/speed as sign-magnitude: bit15 = sign, bits14..0 = magnitude. */
static int16_t ld2450SignMagnitude(uint16_t raw) {
  const uint16_t mag = (uint16_t)(raw & 0x7FFFu);
  return (raw & 0x8000u) ? (int16_t)(-(int32_t)mag) : (int16_t)mag;
}

static uint16_t ld2450U16le(const uint8_t* p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void ld2450ClearTargets() {
  for (uint8_t i = 0; i < LD2450_MAX_TARGETS; i++) {
    ld2450Targets[i] = Ld2450Target();
  }
}

static void ld2450DecodeFrame(const uint8_t* frame) {
  // header AA FF 03 00 already verified; footer 55 CC at [28..29]
  for (uint8_t i = 0; i < LD2450_MAX_TARGETS; i++) {
    const uint8_t* t = frame + 4 + (i * 8);
    const uint16_t xRaw = ld2450U16le(t + 0);
    const uint16_t yRaw = ld2450U16le(t + 2);
    const uint16_t sRaw = ld2450U16le(t + 4);
    const uint16_t dRes = ld2450U16le(t + 6);

    Ld2450Target& out = ld2450Targets[i];
    // All-zero slot = empty (NOT contact at origin)
    if (xRaw == 0 && yRaw == 0 && sRaw == 0 && dRes == 0) {
      out = Ld2450Target();
      continue;
    }

    const int16_t xMm = ld2450SignMagnitude(xRaw);
    const int16_t yMm = ld2450SignMagnitude(yRaw);
    const int16_t speedCmS = ld2450SignMagnitude(sRaw);
    const float rangeMm = sqrtf((float)xMm * (float)xMm + (float)yMm * (float)yMm);
    // Body frame: +Y forward from sensor face, +X starboard; bearing clockwise from mount.
    float bearing = atan2f((float)xMm, (float)yMm) * 180.0f / (float)M_PI;
    bearing += LD2450_MOUNT_BEARING_DEG;
    while (bearing > 180.0f) bearing -= 360.0f;
    while (bearing < -180.0f) bearing += 360.0f;

    out.valid = true;
    out.xMm = xMm;
    out.yMm = yMm;
    out.speedCmS = speedCmS;
    out.distanceResMm = dRes;
    out.bearingDeg = bearing;
    out.rangeM = rangeMm / 1000.0f;
  }
  ld2450FramesOk++;
  ld2450LastFrameMs = millis();
}

static void initLd2450() {
  ld2450ClearTargets();
  // Serial1: RX=9 (J18 ← LD2450 TX), TX=8 (J18 → LD2450 RX)
  Serial1.begin(LD2450_BAUD, SERIAL_8N1, LD2450_RX_PIN, LD2450_TX_PIN);
  ld2450Ready = true;
  Serial.printf("OK: ld2450 uart2/j18 rx=%d tx=%d baud=%lu\n",
                LD2450_RX_PIN, LD2450_TX_PIN, (unsigned long)LD2450_BAUD);
  Serial.flush();
}

static bool bmm150WriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool bmm150ReadRegs(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int)addr, (int)len);
  if (n != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static void initBmm150() {
  bmm150Ready = false;
  bmm150Addr = 0;
  bmm150_uT_x = bmm150_uT_y = bmm150_uT_z = NAN;
  // Scan strapped addresses 0x10-0x13 (do not assume 0x13).
  // Power-control bit MUST be set before most registers are usable; chip ID
  // is readable in suspend after POR, but some modules NACK until powered.
  for (uint8_t addr = 0x10; addr <= 0x13; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;  // no ACK

    if (!bmm150WriteReg(addr, BMM150_POWER_CTRL, 0x01)) continue;  // power on
    delay(5);
    uint8_t chip = 0;
    if (!bmm150ReadRegs(addr, BMM150_CHIP_ID_REG, &chip, 1)) continue;
    if (chip != BMM150_CHIP_ID_VAL) {
      Serial.printf("OK: bmm150 skip addr=0x%02X chip=0x%02X\n", addr, chip);
      continue;
    }
    // Soft reset then back to power-on / normal mode
    (void)bmm150WriteReg(addr, BMM150_POWER_CTRL, 0x83);
    delay(3);
    if (!bmm150WriteReg(addr, BMM150_POWER_CTRL, 0x01)) continue;
    delay(5);
    // Regular preset repetitions (Bosch): REPXY=(9-1)/2=0x04, REPZ=15-1=0x0F
    (void)bmm150WriteReg(addr, 0x51, 0x04);
    (void)bmm150WriteReg(addr, 0x52, 0x0F);
    // Normal mode, ODR 10 Hz
    if (!bmm150WriteReg(addr, BMM150_OP_MODE, 0x00)) continue;
    delay(10);
    uint8_t chip2 = 0;
    if (!bmm150ReadRegs(addr, BMM150_CHIP_ID_REG, &chip2, 1) || chip2 != BMM150_CHIP_ID_VAL) {
      continue;
    }
    bmm150Addr = addr;
    bmm150Ready = true;
    Serial.printf("OK: bmm150 present addr=0x%02X chip=0x%02X\n", addr, chip2);
    Serial.flush();
    return;
  }
  Serial.println("OK: bmm150 absent (scan 0x10-0x13)");
  Serial.flush();
}

/** Convert BMM150 raw registers to approx uT (uncalibrated; trim not applied). */
static void serviceBmm150() {
  if (!bmm150Ready || bmm150Addr == 0) return;
  uint8_t raw[8];
  if (!bmm150ReadRegs(bmm150Addr, BMM150_DATA_X_LSB, raw, 8)) return;
  // X/Y: 13-bit; Z: 15-bit — see BST-BMM150-DS001
  int16_t x = (int16_t)(((uint16_t)raw[1] << 8) | raw[0]);
  x >>= 3;
  int16_t y = (int16_t)(((uint16_t)raw[3] << 8) | raw[2]);
  y >>= 3;
  int16_t z = (int16_t)(((uint16_t)raw[5] << 8) | raw[4]);
  z >>= 1;
  // Nominal sensitivity ~0.3 uT/LSB in regular preset (uncalibrated publish)
  bmm150_uT_x = (float)x * 0.3f;
  bmm150_uT_y = (float)y * 0.3f;
  bmm150_uT_z = (float)z * 0.3f;
  bmm150LastMs = millis();
  bmm150SamplesOk++;
}

static void serviceLd2450() {
  if (!ld2450Ready) return;
  while (Serial1.available() > 0) {
    const uint8_t b = (uint8_t)Serial1.read();
    if (ld2450RxLen == 0) {
      if (b == 0xAA) {
        ld2450RxBuf[0] = b;
        ld2450RxLen = 1;
      }
      continue;
    }
    ld2450RxBuf[ld2450RxLen++] = b;

    // Validate header as it arrives
    if (ld2450RxLen == 2 && ld2450RxBuf[1] != 0xFF) { ld2450RxLen = 0; continue; }
    if (ld2450RxLen == 3 && ld2450RxBuf[2] != 0x03) { ld2450RxLen = 0; continue; }
    if (ld2450RxLen == 4 && ld2450RxBuf[3] != 0x00) { ld2450RxLen = 0; continue; }

    if (ld2450RxLen >= LD2450_FRAME_LEN) {
      if (ld2450RxBuf[28] == 0x55 && ld2450RxBuf[29] == 0xCC) {
        ld2450DecodeFrame(ld2450RxBuf);
      }
      ld2450RxLen = 0;
    }
  }
}

static void printMmwaveJson();  // defined after JSON helpers

// ---- LED ----

enum LedMode { LED_OFF = 0, LED_ON, LED_BLINK, LED_HEARTBEAT };
static LedMode ledMode = LED_HEARTBEAT;
static bool ledState = true;
static unsigned long lastLedUpdate = 0;
static unsigned long heartbeatPhaseStart = 0;

static void reply(const char* s) {
  Serial.println(s);
  Serial.flush();
}

static void setLed(bool on) {
  ledState = on;
  if (on) {
    pixels.setPixelColor(0, pixels.Color(led_r, led_g, led_b));
  } else {
    pixels.clear();
  }
  pixels.show();
}

static void applyLedModeImmediate() {
  if (ledMode == LED_OFF) setLed(false);
  else if (ledMode == LED_ON) setLed(true);
}

// ---- Buzzer ----

static void initBuzzer() {
  if (!buzzerReady) {
    ledcSetup(0, 2000, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWriteTone(0, 0);
    buzzerReady = true;
  }
}

static void playToneHz(int hz, int durationMs, int gapMs = 20) {
  initBuzzer();
  if (hz > 0) {
    ledcWriteTone(0, hz);
    delay(durationMs);
    ledcWriteTone(0, 0);
  } else {
    delay(durationMs);
  }
  if (gapMs > 0) delay(gapMs);
}

static void sfxCoin() { playToneHz(988, 60, 10); playToneHz(1319, 90, 0); }
static void sfxBump() { playToneHz(220, 70, 0); }
static void sfxPower() { playToneHz(523, 70); playToneHz(659, 70); playToneHz(784, 120, 0); }
static void sfx1Up() { playToneHz(659, 70); playToneHz(784, 70); playToneHz(988, 70); playToneHz(1319, 140, 0); }

// ---- I2C helpers ----

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t& val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 1) != 1) return false;
  val = Wire.read();
  return true;
}

static void readChipIds(SensorSlot& s) {
  i2cRead8(s.addr, 0xD0, s.chip_id);
  i2cRead8(s.addr, 0xF0, s.variant_id);
}

static void scanI2C() {
  Serial.println("OK: scan begin");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("I2C: found 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("OK: scan count=%d\n", found);
  Serial.flush();
}

// ---- Pressure conversion (from MDP firmware) ----

static float pressureToHpa(float p) {
  if (!isfinite(p) || p <= 0) return NAN;
  if (p > 20000.0f) return p / 100.0f;
  if (p > 2000.0f) return p / 10.0f;
  if (p > 200.0f) return p;
  if (p > 20.0f) return p * 10.0f;
  return p * 1000.0f;
}

// ---- BSEC2 callbacks ----

static void slotCallbackCommon(SensorSlot& s, const bme68xData data, const bsecOutputs outputs) {
  s.r.valid = true;
  s.r.t_ms = millis();

  // Raw sensor data from BME68x
  s.r.ambient_temperature_c = data.temperature;
  s.r.ambient_humidity_pct = data.humidity;
  s.r.pressure_hpa = pressureToHpa((float)data.pressure);
  s.r.gas_resistance_ohm = (float)data.gas_resistance;

  // Raw status flags
  s.r.new_data = (data.status & BME68X_NEW_DATA_MSK) != 0;
  s.r.measuring = false;
  s.r.gas_measuring = (data.status & BME68X_GASM_VALID_MSK) != 0;
  s.r.gas_meas_index = data.gas_index;
  s.r.sub_meas_index = data.meas_index;
  s.r.gas_valid = (data.status & BME68X_GASM_VALID_MSK) != 0;
  s.r.heat_stable = (data.status & BME68X_HEAT_STAB_MSK) != 0;

  // Reset BSEC-derived outputs (populated from bsecOutputs below)
  s.r.iaq = NAN;
  s.r.iaq_static = NAN;
  s.r.iaq_accuracy = NAN;
  s.r.eco2_ppm = NAN;
  s.r.bvoc_ppm = NAN;
  s.r.gas_percentage = NAN;
  s.r.stabilization_status = NAN;
  s.r.run_in_status = NAN;
  s.r.output_accuracy = NAN;
  s.r.gas_estimate_1 = NAN;
  s.r.gas_estimate_2 = NAN;
  s.r.gas_estimate_3 = NAN;
  s.r.gas_estimate_4 = NAN;
  s.r.gas_estimate_accuracy = NAN;

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData& o = outputs.output[i];
    switch (o.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        s.r.iaq = o.signal;
        s.r.iaq_accuracy = (float)o.accuracy;
        s.r.output_accuracy = (float)o.accuracy;
        break;
      case BSEC_OUTPUT_STATIC_IAQ:
        s.r.iaq_static = o.signal;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        s.r.eco2_ppm = o.signal;
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        s.r.bvoc_ppm = o.signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        s.r.temperature_c_comp = o.signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        s.r.humidity_pct_comp = o.signal;
        break;
      case BSEC_OUTPUT_GAS_PERCENTAGE:
        s.r.gas_percentage = o.signal;
        break;
      case BSEC_OUTPUT_STABILIZATION_STATUS:
        s.r.stabilization_status = o.signal;
        break;
      case BSEC_OUTPUT_RUN_IN_STATUS:
        s.r.run_in_status = o.signal;
        break;
      case BSEC_OUTPUT_GAS_ESTIMATE_1:
        s.r.gas_estimate_1 = o.signal;
        s.r.gas_estimate_accuracy = (float)o.accuracy;
        break;
      case BSEC_OUTPUT_GAS_ESTIMATE_2:
        s.r.gas_estimate_2 = o.signal;
        break;
      case BSEC_OUTPUT_GAS_ESTIMATE_3:
        s.r.gas_estimate_3 = o.signal;
        break;
      case BSEC_OUTPUT_GAS_ESTIMATE_4:
        s.r.gas_estimate_4 = o.signal;
        break;
      default:
        break;
    }
  }

  s.r.bsec_status_code = s.bsec.status;
  s.r.bme68x_status_code = s.bsec.sensor.status;
}

static void cbAMB(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_AMB, data, outputs);
}

static void cbENV(const bme68xData data, const bsecOutputs outputs, const Bsec2 /*bsec*/) {
  slotCallbackCommon(S_ENV, data, outputs);
}

// ---- Sensor init (ported from MDP firmware) ----

static bool slotInit(SensorSlot& s) {
  s.present = false;
  s.beginOk = s.subOk = false;
  s.r = SensorReading();

  Wire.beginTransmission(s.addr);
  if (Wire.endTransmission() != 0) return false;
  s.present = true;

  readChipIds(s);

  s.bsec.allocateMemory(s.mem);
  if (!s.bsec.begin(s.addr, Wire)) {
    Serial.printf("BSEC: %s begin FAIL at 0x%02X\n", s.name, s.addr);
    return false;
  }
  s.beginOk = true;

  s.bsec.setTemperatureOffset(TEMP_OFFSET_C);
  // Selectivity config skipped — blob is incompatible with BSEC 2.6.1.0.
  // Gas estimates (1-4) require a matching config from BME AI-Studio.

  if (s.addr == 0x77) s.bsec.attachCallback(cbAMB);
  if (s.addr == 0x76) s.bsec.attachCallback(cbENV);

  bsecSensor list[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY
  };

  if (!s.bsec.updateSubscription(list, (uint8_t)ARRAY_LEN(list), BSEC_SAMPLE_RATE_LP)) {
    if (s.bsec.status < BSEC_OK) {
      Serial.printf("BSEC: %s subscription ERROR (status=%d)\n", s.name, s.bsec.status);
      return false;
    }
    Serial.printf("BSEC: %s subscription WARNING (status=%d) — proceeding\n", s.name, s.bsec.status);
  }
  s.subOk = true;
  Serial.printf("BSEC: %s OK at 0x%02X chip=0x%02X var=0x%02X\n",
                s.name, s.addr, s.chip_id, s.variant_id);
  return true;
}

static void initAllSensors() {
  Wire.end();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  S_AMB.init("amb", 0x77);
  S_ENV.init("env", 0x76);
  slotInit(S_AMB);
  slotInit(S_ENV);
  initBmm150();
}

// ---- JSON output helpers ----

static void jf(const char* key, float val, int dec = 2) {
  Serial.print(",\""); Serial.print(key); Serial.print("\":");
  if (isnan(val)) Serial.print("null");
  else Serial.print(val, dec);
}

static void ji(const char* key, int val) {
  Serial.print(",\""); Serial.print(key); Serial.print("\":");
  Serial.print(val);
}

static void jb(const char* key, bool val) {
  Serial.print(",\""); Serial.print(key); Serial.print("\":");
  Serial.print(val ? "true" : "false");
}

static void js(const char* key, const char* val) {
  Serial.print(",\""); Serial.print(key); Serial.print("\":\"");
  Serial.print(val); Serial.print("\"");
}

static void jshex(const char* key, uint8_t val) {
  char h[5];
  snprintf(h, sizeof(h), "0x%02X", val);
  js(key, h);
}

static void printMmwaveJson() {
  const bool hardwarePresent = ld2450Ready && (ld2450FramesOk > 0) &&
                               (millis() - ld2450LastFrameMs < 2000);
  Serial.print("{\"type\":\"mmwave\"");
  js("device_id", DEVICE_ID);
  js("node_id", DEVICE_ID);
  js("role", DEVICE_ROLE);
  js("fw_version", FW_VERSION);
  js("sensorClass", "mmwave");
  js("sensorModel", "HLK-LD2450");
  js("host", "esp32-side-a");
  js("hardware", hardwarePresent ? "present" : (ld2450Ready ? "waiting" : "absent"));
  jf("maxRangeM", LD2450_MAX_RANGE_M, 1);
  jf("fovDeg", LD2450_FOV_DEG, 0);
  jf("mountBearingDeg", LD2450_MOUNT_BEARING_DEG, 1);
  ji("ts_ms", (int)millis());
  ji("frames_ok", (int)ld2450FramesOk);
  Serial.print(",\"targets\":[");
  for (uint8_t i = 0; i < LD2450_MAX_TARGETS; i++) {
    if (i) Serial.print(",");
    const Ld2450Target& t = ld2450Targets[i];
    Serial.print("{\"id\":");
    Serial.print(i);
    jb("valid", t.valid);
    if (t.valid) {
      ji("xMm", t.xMm);
      ji("yMm", t.yMm);
      ji("speedCmS", t.speedCmS);
      ji("distanceResMm", (int)t.distanceResMm);
      jf("bearingDeg", t.bearingDeg, 1);
      jf("rangeM", t.rangeM, 3);
    } else {
      // Empty slot: valid:false — NEVER emit rangeM:0 (that would plot a false origin contact)
      Serial.print(",\"xMm\":null,\"yMm\":null,\"speedCmS\":null,\"bearingDeg\":null,\"rangeM\":null");
    }
    Serial.print("}");
  }
  Serial.println("]}");
  Serial.flush();
}

static void printMagnetometerJson() {
  Serial.print("{\"type\":\"magnetometer\"");
  js("device_id", DEVICE_ID);
  js("node_id", DEVICE_ID);
  js("role", DEVICE_ROLE);
  js("fw_version", FW_VERSION);
  js("sensorModel", "BMM150");
  js("host", "esp32-side-a");
  js("bus", "i2c");
  js("hardware", bmm150Ready ? "present" : "absent");
  if (bmm150Ready) jshex("address", bmm150Addr);
  else Serial.print(",\"address\":null");
  jb("calibrated", false);
  jb("tiltCompensated", false);
  // NEVER emit headingDeg without tilt compensation — GCS must refuse bearing
  Serial.print(",\"headingDeg\":null");
  jf("magX_uT", bmm150_uT_x, 2);
  jf("magY_uT", bmm150_uT_y, 2);
  jf("magZ_uT", bmm150_uT_z, 2);
  ji("samples_ok", (int)bmm150SamplesOk);
  ji("lastMs", (int)bmm150LastMs);
  ji("ts_ms", (int)millis());
  Serial.println("}");
  Serial.flush();
}

// ---- Full telemetry JSON per slot ----

static void printSlotJson(const SensorSlot& s) {
  const SensorReading& r = s.r;
  uint32_t uptime_s = millis() / 1000;

  Serial.print("{\"type\":\"telemetry\"");
  js("device_id", DEVICE_ID);
  js("node_id", DEVICE_ID);
  js("role", DEVICE_ROLE);
  js("sensor_slot", s.name);
  jshex("address", s.addr);
  jb("present", s.present);
  jb("valid", r.valid);
  js("fw_version", FW_VERSION);
  js("config_version", CONFIG_VER);

  // BSEC version
  char bver[24];
  snprintf(bver, sizeof(bver), "%d.%d.%d.%d",
           s.bsec.version.major, s.bsec.version.minor,
           s.bsec.version.major_bugfix, s.bsec.version.minor_bugfix);
  js("bsec_version", bver);

  Serial.print(",\"location\":{\"lat\":null,\"lon\":null,\"source\":\"na\"}");

  ji("ts_ms", (int)millis());

  if (r.valid) {
    // Compensated environmental
    jf("temperature_c_comp", r.temperature_c_comp);
    if (!isnan(r.temperature_c_comp))
      jf("temperature_f_comp", r.temperature_c_comp * 9.0f / 5.0f + 32.0f);
    else
      jf("temperature_f_comp", NAN);
    jf("humidity_pct_comp", r.humidity_pct_comp);
    jf("gas_resistance_ohm_comp", r.gas_resistance_ohm_comp);

    // Ambient (raw, pre-compensation)
    jf("ambient_temperature_c", r.ambient_temperature_c);
    if (!isnan(r.ambient_temperature_c))
      jf("ambient_temperature_f", r.ambient_temperature_c * 9.0f / 5.0f + 32.0f);
    else
      jf("ambient_temperature_f", NAN);
    jf("ambient_humidity_pct", r.ambient_humidity_pct);

    // IAQ / gas analysis
    jf("iaq", r.iaq);
    jf("iaq_static", r.iaq_static);
    jf("iaq_accuracy", r.iaq_accuracy, 0);
    jf("eco2_ppm", r.eco2_ppm);
    jf("bvoc_ppm", r.bvoc_ppm);
    jf("gas_percentage", r.gas_percentage);

    // Pressure + raw gas
    jf("pressure_hpa", r.pressure_hpa);
    jf("gas_resistance_ohm", r.gas_resistance_ohm, 0);

    // Sensor status
    jf("stabilization_status", r.stabilization_status, 0);
    jf("run_in_status", r.run_in_status, 0);
    jf("output_accuracy", r.output_accuracy, 0);
    ji("bsec_status_code", r.bsec_status_code);
    ji("bme68x_status_code", r.bme68x_status_code);

    // Gas estimates
    jf("gas_estimate_1", r.gas_estimate_1);
    jf("gas_estimate_2", r.gas_estimate_2);
    jf("gas_estimate_3", r.gas_estimate_3);
    jf("gas_estimate_4", r.gas_estimate_4);
    jf("gas_estimate_accuracy", r.gas_estimate_accuracy, 0);

    // Raw sensor flags
    jb("new_data", r.new_data);
    jb("measuring", r.measuring);
    jb("gas_measuring", r.gas_measuring);
    ji("gas_meas_index", r.gas_meas_index);
    ji("sub_meas_index", r.sub_meas_index);
    jb("gas_valid", r.gas_valid);
    jb("heat_stable", r.heat_stable);
    jshex("chip_id", s.chip_id);
    jshex("variant_id", s.variant_id);
  }

  // Metadata (always emitted)
  jf("temp_offset_c", TEMP_OFFSET_C, 1);
  js("sample_rate_mode", SAMPLE_RATE_MODE);
  ji("heater_profile_id", 0);
  js("model_type", "selectivity");
  ji("class_count", CLASS_COUNT);
  ji("uptime_s", (int)uptime_s);
  ji("boot_count", (int)bootCount);

  Serial.println("}");
  Serial.flush();
}

static void printAllSensorJson() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    SensorSlot* s = sensorPtrs[i];
    if (s->present) printSlotJson(*s);
  }
}

// ---- CLI ----

static void printHelp() {
  Serial.println("OK: commands");
  Serial.println("  ping");
  Serial.println("  help");
  Serial.println("  status");
  Serial.println("  scan");
  Serial.println("  sensor");
  Serial.println("  sensor init");
  Serial.println("  sensor json");
  Serial.println("  mmwave");
  Serial.println("  mag");
  Serial.println("  led on|off|blink|heartbeat|probe");
  Serial.println("  coin|bump|power|1up");
  Serial.println("  test outputs");
  Serial.println("  reboot");
  Serial.flush();
}

static void printStatus() {
  Serial.printf("OK: status version=%s uptime_ms=%lu\n", FW_VERSION, millis());
  for (int i = 0; i < SENSOR_COUNT; i++) {
    SensorSlot* s = sensorPtrs[i];
    Serial.printf("  %s: present=%s addr=0x%02X begin=%s sub=%s chip=0x%02X var=0x%02X\n",
                  s->name,
                  s->present ? "YES" : "NO",
                  s->addr,
                  s->beginOk ? "OK" : "FAIL",
                  s->subOk ? "OK" : "FAIL",
                  s->chip_id, s->variant_id);
  }
  Serial.printf("  ld2450: ready=%s frames=%lu last_ms=%lu rx=%d tx=%d\n",
                ld2450Ready ? "YES" : "NO",
                (unsigned long)ld2450FramesOk,
                (unsigned long)ld2450LastFrameMs,
                LD2450_RX_PIN, LD2450_TX_PIN);
  Serial.printf("  bmm150: ready=%s addr=0x%02X samples=%lu\n",
                bmm150Ready ? "YES" : "NO",
                bmm150Addr,
                (unsigned long)bmm150SamplesOk);
  Serial.printf("  led_mode=%d buzzer=%s reboot_count=%u\n",
                (int)ledMode, buzzerReady ? "ready" : "not_init", bootCount);
  Serial.flush();
}

static void ledProbe() {
  Serial.println("OK: led probe begin");
  ledMode = LED_OFF;
  setLed(false);
  delay(300);
  for (int i = 0; i < 5; i++) {
    setLed(true);
    delay(300);
    setLed(false);
    delay(300);
  }
  ledMode = LED_HEARTBEAT;
  heartbeatPhaseStart = millis();
  Serial.println("OK: led probe done; mode=heartbeat");
  Serial.flush();
}

static void testOutputs() {
  Serial.println("OK: test outputs begin");
  ledProbe();
  sfxCoin(); delay(100);
  sfxBump(); delay(100);
  sfxPower(); delay(100);
  sfx1Up();
  Serial.println("OK: test outputs done");
  Serial.flush();
}

static void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "ping") {
    reply("OK: ping");
  } else if (cmd == "help") {
    printHelp();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "sensor" || cmd == "sensor json") {
    printAllSensorJson();
  } else if (cmd == "mmwave") {
    printMmwaveJson();
  } else if (cmd == "mag" || cmd == "magnetometer") {
    printMagnetometerJson();
  } else if (cmd == "sensor init") {
    initAllSensors();
    int count = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) if (sensorPtrs[i]->present) count++;
    Serial.printf("OK: sensor init found=%d\n", count);
    Serial.flush();
  } else if (cmd == "led on") {
    led_r = 0; led_g = 0; led_b = 200;
    ledMode = LED_ON; applyLedModeImmediate(); reply("OK: led on");
  } else if (cmd == "led green") {
    led_r = 0; led_g = 160; led_b = 0;
    ledMode = LED_ON; applyLedModeImmediate(); reply("OK: led green");
  } else if (cmd == "led blue") {
    led_r = 0; led_g = 0; led_b = 200;
    ledMode = LED_ON; applyLedModeImmediate(); reply("OK: led blue");
  } else if (cmd == "led off") {
    ledMode = LED_OFF; applyLedModeImmediate(); reply("OK: led off");
  } else if (cmd == "led blink") {
    ledMode = LED_BLINK; lastLedUpdate = millis(); reply("OK: led blink");
  } else if (cmd == "led heartbeat") {
    ledMode = LED_HEARTBEAT; heartbeatPhaseStart = millis(); reply("OK: led heartbeat");
  } else if (cmd == "led probe") {
    ledProbe();
  } else if (cmd == "scan") {
    scanI2C();
  } else if (cmd == "coin") {
    sfxCoin(); reply("OK: coin");
  } else if (cmd == "bump") {
    sfxBump(); reply("OK: bump");
  } else if (cmd == "power") {
    sfxPower(); reply("OK: power");
  } else if (cmd == "1up") {
    sfx1Up(); reply("OK: 1up");
  } else if (cmd == "test outputs") {
    testOutputs();
  } else if (cmd == "reboot") {
    reply("OK: rebooting");
    delay(250);
    ESP.restart();
  } else if (cmd.length()) {
    Serial.print("ERR: ");
    Serial.println(cmd);
    Serial.flush();
  }
}

// ---- LED service ----

static void serviceLed() {
  unsigned long now = millis();
  if (ledMode == LED_BLINK) {
    if (now - lastLedUpdate >= 700) {
      lastLedUpdate = now;
      setLed(!ledState);
    }
  } else if (ledMode == LED_HEARTBEAT) {
    unsigned long phase = (now - heartbeatPhaseStart) % 1800;
    bool on = (phase < 120) || (phase >= 220 && phase < 340);
    if (on != ledState) setLed(on);
  }
}

// ---- Arduino entry points ----

void setup() {
  Serial.begin(115200);
  delay(1500);

  pixels.begin();
  pixels.setBrightness(80);
  pixels.clear();
  pixels.show();
  // NeoPixel on GPIO15 — do not use plain digitalWrite
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  initAllSensors();
  initLd2450();

  led_r = 0; led_g = 0; led_b = 180;  // boot blue
  ledMode = LED_HEARTBEAT;
  heartbeatPhaseStart = millis();
  applyLedModeImmediate();
  setLed(true);

  // Boot chime (NeoPixel blue already on) — init buzzer before SFX
  initBuzzer();
  sfxPower();

  reply("BOOT: recovery-operator-bsec2-ld2450 ready");
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length()) handleCommand(buf);
      buf = "";
    } else {
      buf += c;
    }
  }

  // Run BSEC2 processing for each present sensor
  if (S_AMB.present && S_AMB.beginOk) (void)S_AMB.bsec.run();
  if (S_ENV.present && S_ENV.beginOk) (void)S_ENV.bsec.run();

  serviceLd2450();
  serviceBmm150();
  serviceLed();

  if (millis() - lastSensorPrint > 3000) {
    lastSensorPrint = millis();
    printAllSensorJson();
  }

  // ~5 Hz while frames are live; slower honest waiting/absent cadence so BSEC2 USB is not starved
  {
    const bool live = ld2450FramesOk > 0 && (millis() - ld2450LastFrameMs < 2000);
    const unsigned long period = live ? 200UL : 2000UL;
    if (millis() - lastMmwavePrint > period) {
      lastMmwavePrint = millis();
      printMmwaveJson();
    }
  }

  if (millis() - lastMagPrint > 1000) {
    lastMagPrint = millis();
    printMagnetometerJson();
  }

  if (millis() - lastBeat > 5000) {
    lastBeat = millis();
    Serial.printf("OK: heartbeat ms=%lu\n", lastBeat);
    Serial.flush();
  }

  delay(5);
}
