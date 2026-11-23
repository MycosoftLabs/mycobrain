/**

 * Side B USB-JSON transport role: BLE + LoRa (CORE1262-868M).

 * Aug 03 2026 — honest lora_ready/ble_ready from real init (no fake true).

 * 2.2.1: USB-safe boot - Serial first; LoRa busy timeout; BLE deferred.
 * 2.2.2: BLE RX GATT callbacks - restart/reboot/reset -> ESP.restart() for soft recovery.
 * Jetson hub reads USB CDC JSON lines (FB:14).

 */

#include <Arduino.h>

#include <ArduinoJson.h>

#include <Adafruit_NeoPixel.h>

#include <SPI.h>

#include <RadioLib.h>

#include <BLEDevice.h>

#include <BLEUtils.h>

#include <BLEServer.h>

#include <BLE2902.h>



static const char* FW_VERSION = "side-b-usb-json-2.2.2-ble-lora-usbsafe";



#ifndef NEOPIXEL_PIN

#define NEOPIXEL_PIN 15

#endif



// CORE1262 SPI (USB-JSON path does not use HW UART on these pins)

static const int LORA_RST = 7;

static const int LORA_BUSY = 12;

static const int LORA_SCK = 18;

static const int LORA_NSS = 17;

static const int LORA_MISO = 19;

static const int LORA_MOSI = 20;

static const int LORA_DIO1 = 21;

static const float LORA_FREQ_MHZ = 868.0f;

// Defer BLE so TinyUSB CDC can re-enumerate before radio stack starts.

static const uint32_t BLE_DEFER_MS = 8000;

static const uint32_t LORA_BUSY_TIMEOUT_MS = 2000;



#define BLE_DEVICE_NAME "MycoBrain-SideB"

#define BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

#define BLE_CHAR_TX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define BLE_CHAR_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"



Adafruit_NeoPixel pixels(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);



bool lora_ready = false;

bool ble_ready = false;

bool wifi_ready = false;

bool sim_ready = false;

bool ble_advertising = false;

bool ble_init_attempted = false;

int lora_init_err = 0;

String usbLineBuffer;

uint32_t lastEmitMs = 0;

uint32_t lastLedMs = 0;

uint32_t bootMs = 0;

bool ledOn = false;



static BLEServer* bleServer = nullptr;

static BLECharacteristic* bleTxChar = nullptr;

static BLECharacteristic* bleRxChar = nullptr;

static BLEAdvertising* bleAdvertising = nullptr;

// Set by BLE RX callback; consumed in loop() so restart is not mid-GATT.
static volatile bool bleRestartRequested = false;



void set_led(uint8_t r, uint8_t g, uint8_t b) {

  pixels.setPixelColor(0, pixels.Color(r, g, b));

  pixels.show();

}



bool loraInit() {

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Prevent infinite hang if CORE1262 absent / BUSY stuck.

#if defined(RADIOLIB_VERSION_MAJOR) && (RADIOLIB_VERSION_MAJOR >= 7)

  radio.setBusyTimeout(LORA_BUSY_TIMEOUT_MS);

#else

  // RadioLib 6.x has no setBusyTimeout; SX126x::reset() already bounds the BUSY
  // wait (~1s) and returns RADIOLIB_ERR_SPI_CMD_TIMEOUT instead of blocking.

  (void)LORA_BUSY_TIMEOUT_MS;

#endif

  int st = radio.begin(LORA_FREQ_MHZ);

  lora_init_err = st;

  if (st != RADIOLIB_ERR_NONE) {

    lora_ready = false;

    return false;

  }

  radio.startReceive();

  lora_ready = true;

  return true;

}



void handle_line(const String& line);

class BleRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    if (characteristic == nullptr) {
      return;
    }
    std::string value = characteristic->getValue();
    if (value.empty()) {
      return;
    }
    String line;
    line.reserve(static_cast<unsigned int>(value.size()) + 1);
    for (size_t i = 0; i < value.size(); i++) {
      char c = value[i];
      if (c == '\n' || c == '\r') {
        continue;
      }
      line += c;
    }
    if (line.length() == 0) {
      return;
    }
    // Fast-path restart for recovery even if JSON parse fails later.
    String lower = line;
    lower.toLowerCase();
    if (lower.indexOf("\"cmd\":\"restart\"") >= 0 || lower.indexOf("\"cmd\":\"reboot\"") >= 0 ||
        lower.indexOf("\"cmd\":\"reset\"") >= 0 || lower == "restart" || lower == "reboot" ||
        lower == "reset") {
      bleRestartRequested = true;
      return;
    }
    handle_line(line);
  }
};

static BleRxCallbacks bleRxCallbacks;



bool bleInit() {

  BLEDevice::init(BLE_DEVICE_NAME);

  bleServer = BLEDevice::createServer();

  BLEService* svc = bleServer->createService(BLE_SERVICE_UUID);

  bleTxChar = svc->createCharacteristic(

      BLE_CHAR_TX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  bleTxChar->addDescriptor(new BLE2902());

  bleRxChar = svc->createCharacteristic(

      BLE_CHAR_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);

  bleRxChar->setCallbacks(&bleRxCallbacks);

  svc->start();

  bleAdvertising = BLEDevice::getAdvertising();

  bleAdvertising->addServiceUUID(BLE_SERVICE_UUID);

  bleAdvertising->setScanResponse(true);

  bleAdvertising->start();

  ble_advertising = true;

  ble_ready = true;

  return true;

}



void setBleAdvertise(bool en) {

  if (!ble_ready || bleAdvertising == nullptr) {

    ble_advertising = false;

    return;

  }

  if (en) {

    bleAdvertising->start();

    ble_advertising = true;

  } else {

    bleAdvertising->stop();

    ble_advertising = false;

  }

}



void emit_transport_status() {

  StaticJsonDocument<512> doc;

  doc["event"] = "transport_status";

  doc["role"] = "side_b";

  doc["device_id"] = "mycobrain-sideb";

  doc["firmware_version"] = FW_VERSION;

  doc["lora_ready"] = lora_ready;

  doc["ble_ready"] = ble_ready;

  doc["ble_advertising"] = ble_advertising;

  doc["wifi_ready"] = wifi_ready;

  doc["sim_ready"] = sim_ready;

  doc["lora_init_err"] = lora_init_err;

  doc["lora_freq_mhz"] = LORA_FREQ_MHZ;

  doc["queue_depth"] = 0;

  doc["backpressure"] = false;

  doc["transports"]["lora"] = lora_ready;

  doc["transports"]["ble"] = ble_ready;

  doc["transports"]["wifi"] = wifi_ready;

  doc["transports"]["sim"] = sim_ready;

  serializeJson(doc, Serial);

  Serial.println();

  Serial.flush();

}



void handle_line(const String& line) {

  if (line.length() == 0) return;

  DynamicJsonDocument payload(768);

  if (deserializeJson(payload, line)) {

    Serial.println("{\"ok\":false,\"error\":\"invalid_json\"}");

    return;

  }

  const char* cmd = payload["cmd"] | payload["command"] | "";

  JsonVariant params = payload["params"];



  if (strcmp(cmd, "transport_status") == 0 || strcmp(cmd, "hello") == 0 ||

      strcmp(cmd, "status") == 0 || strcmp(cmd, "ping") == 0) {

    emit_transport_status();

    return;

  }

  if (strcmp(cmd, "restart") == 0 || strcmp(cmd, "reboot") == 0 ||

      strcmp(cmd, "reset") == 0) {

    Serial.println("{\"ok\":true,\"event\":\"restarting\",\"role\":\"side_b\"}");

    Serial.flush();

    set_led(120, 0, 0);

    delay(200);

    ESP.restart();

    return;

  }

  if (strcmp(cmd, "led") == 0) {

    uint8_t r = payload["r"] | params["r"] | 0;

    uint8_t g = payload["g"] | params["g"] | 90;

    uint8_t b = payload["b"] | params["b"] | 0;

    set_led(r, g, b);

    Serial.println("{\"ok\":true,\"event\":\"led\",\"role\":\"side_b\"}");

    return;

  }

  if (strcmp(cmd, "ble_advertise") == 0) {

    if (!ble_ready && !ble_init_attempted) {

      ble_init_attempted = true;

      bleInit();

    }

    bool en = params["en"] | payload["en"] | true;

    setBleAdvertise(en);

    Serial.print("{\"ok\":true,\"event\":\"ble_advertise\",\"ble_ready\":");

    Serial.print(ble_ready ? "true" : "false");

    Serial.print(",\"ble_advertising\":");

    Serial.print(ble_advertising ? "true" : "false");

    Serial.println("}");

    emit_transport_status();

    return;

  }

  if (strcmp(cmd, "lora_send") == 0) {

    const char* data = params["payload"] | payload["payload"] | "";

    if (!lora_ready) {

      Serial.println("{\"ok\":false,\"error\":\"lora_not_ready\"}");

      return;

    }

    if (strlen(data) == 0) {

      Serial.println("{\"ok\":false,\"error\":\"missing_payload\"}");

      return;

    }

    size_t n = strlen(data);

    uint8_t buf[256];

    if (n > sizeof(buf)) n = sizeof(buf);

    memcpy(buf, data, n);

    int st = radio.transmit(buf, n);

    radio.startReceive();

    Serial.print("{\"ok\":");

    Serial.print(st == RADIOLIB_ERR_NONE ? "true" : "false");

    Serial.print(",\"event\":\"lora_send\",\"err\":");

    Serial.print(st);

    Serial.println("}");

    return;

  }

  Serial.print("{\"ok\":false,\"error\":\"unknown_cmd\",\"cmd\":\"");

  Serial.print(cmd);

  Serial.println("\"}");

}



void setup() {

  pixels.begin();

  pixels.setBrightness(64);

  set_led(0, 0, 80);

  // USB-safe: bring Serial up and announce before any radio/SPI/BLE work.

  Serial.begin(115200);

  delay(2000);

  bootMs = millis();



  StaticJsonDocument<384> boot;

  boot["side"] = "B";

  boot["role"] = "side_b";

  boot["mdp"] = 0;

  boot["usb_json"] = 1;

  boot["lora"] = false;

  boot["ble"] = false;

  boot["status"] = "booting";

  boot["firmware_version"] = FW_VERSION;

  boot["lora_freq_mhz"] = LORA_FREQ_MHZ;

  boot["note"] = "usb_safe_deferred_radio";

  serializeJson(boot, Serial);

  Serial.println();

  Serial.flush();



  bool loraOk = loraInit();



  StaticJsonDocument<384> afterLora;

  afterLora["side"] = "B";

  afterLora["role"] = "side_b";

  afterLora["status"] = "lora_init_done";

  afterLora["lora"] = loraOk;

  afterLora["ble"] = false;

  afterLora["firmware_version"] = FW_VERSION;

  afterLora["lora_init_err"] = lora_init_err;

  serializeJson(afterLora, Serial);

  Serial.println();

  Serial.flush();



  emit_transport_status();

  lastEmitMs = millis();

  lastLedMs = millis();

  if (loraOk) {

    set_led(90, 60, 0);  // amber until BLE deferred init

  } else {

    set_led(90, 0, 0);

  }

  ledOn = true;

}



void loop() {

  if (bleRestartRequested) {
    bleRestartRequested = false;
    Serial.println("{\"ok\":true,\"event\":\"restarting\",\"role\":\"side_b\",\"via\":\"ble\"}");
    Serial.flush();
    set_led(120, 0, 0);
    delay(200);
    ESP.restart();
  }

  // Deferred BLE — avoids TinyUSB CDC drop seen when BLE starts in setup().

  if (!ble_init_attempted && (millis() - bootMs) >= BLE_DEFER_MS) {

    ble_init_attempted = true;

    bleInit();

    emit_transport_status();

  }



  while (Serial.available() > 0) {

    char c = static_cast<char>(Serial.read());

    if (c == '\n' || c == '\r') {

      if (usbLineBuffer.length() > 0) {

        handle_line(usbLineBuffer);

        usbLineBuffer = "";

      }

    } else if (usbLineBuffer.length() < 512) {

      usbLineBuffer += c;

    } else {

      usbLineBuffer = "";

    }

  }



  if (millis() - lastLedMs >= 500) {

    lastLedMs = millis();

    ledOn = !ledOn;

    if (lora_ready && ble_ready) {

      set_led(0, ledOn ? 90 : 12, 0);

    } else if (lora_ready || ble_ready) {

      set_led(ledOn ? 90 : 20, ledOn ? 60 : 10, 0);

    } else {

      set_led(ledOn ? 90 : 20, 0, 0);

    }

  }



  if (millis() - lastEmitMs >= 3000) {

    emit_transport_status();

    lastEmitMs = millis();

  }

  delay(5);

}


