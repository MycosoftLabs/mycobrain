#include <Arduino.h>
#include <SPI.h>

// Communications module build flags (set in platformio.ini)
#ifndef ENABLE_LORA
#define ENABLE_LORA 1
#endif
#ifndef ENABLE_WIFI
#define ENABLE_WIFI 0
#endif
#ifndef ENABLE_BLE
#define ENABLE_BLE 0
#endif

#if ENABLE_LORA
#include <RadioLib.h>
#endif

#if ENABLE_WIFI
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#endif

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#endif

#include <mdp_types.h>
#include <mdp_utils.h>

namespace cfg {
constexpr uint32_t USB_BAUD = 115200;

// UART to Side-A
constexpr uint32_t UART_BAUD = 115200;
constexpr int PIN_B_RX2 = 9;
constexpr int PIN_B_TX2 = 8;

// buffers
constexpr size_t MAX_FRAME   = 1200;
constexpr size_t MAX_PAYLOAD = 900;

// reliability
constexpr uint32_t UART_RTO_MS = 120;
constexpr uint32_t LORA_RTO_MS = 1800;
constexpr uint32_t WIFI_RTO_MS = 500;
constexpr uint8_t  MAX_RETRIES = 5;

// ===== SX1262 pin map (authoritative) =====
// SX_Reset  -> GPIO7
// SX_Busy   -> GPIO12
// SX_CLK    -> GPIO18
// SX_CS     -> GPIO17
// SX_DI01   -> GPIO21
// SX_DI02   -> GPIO22 (not used by default)
// SX_MISO   -> GPIO19
// SX_MOSI   -> GPIO20
constexpr int LORA_RST  = 7;
constexpr int LORA_BUSY = 12;
constexpr int LORA_SCK  = 18;
constexpr int LORA_NSS  = 17;
constexpr int LORA_MISO = 19;
constexpr int LORA_MOSI = 20;
constexpr int LORA_DIO1 = 21;

constexpr float LORA_FREQ_MHZ = 915.0;

// ===== WiFi configuration =====
#if ENABLE_WIFI
// WiFi credentials are stored in NVS, these are compile-time defaults
#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT ""
#endif
#ifndef WIFI_PASS_DEFAULT
#define WIFI_PASS_DEFAULT ""
#endif
// Gateway server for WiFi telemetry forwarding
#ifndef GATEWAY_HOST_DEFAULT
#define GATEWAY_HOST_DEFAULT "192.168.0.188"
#endif
#ifndef GATEWAY_PORT_DEFAULT
#define GATEWAY_PORT_DEFAULT 8001
#endif
constexpr uint16_t WIFI_UDP_PORT = 5555;  // UDP port for telemetry
constexpr uint32_t WIFI_RECONNECT_MS = 30000;  // Reconnect interval
#endif

// ===== BLE configuration =====
#if ENABLE_BLE
#define BLE_DEVICE_NAME "MycoBrain"
// MycoBrain BLE service UUID
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHAR_TX_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_CHAR_RX_UUID        "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#endif
}

// ========== LoRa Module ==========
#if ENABLE_LORA
SX1262 radio = new Module(cfg::LORA_NSS, cfg::LORA_DIO1, cfg::LORA_RST, cfg::LORA_BUSY);
static bool loraReady = false;

static bool loraInit() {
  SPI.begin(cfg::LORA_SCK, cfg::LORA_MISO, cfg::LORA_MOSI, cfg::LORA_NSS);
  int st = radio.begin(cfg::LORA_FREQ_MHZ);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("{\"lora_init\":\"fail\",\"err\":");
    Serial.print(st);
    Serial.println("}");
    return false;
  }
  Serial.println("{\"lora_init\":\"ok\"}");
  radio.startReceive();
  loraReady = true;
  return true;
}

static bool loraSendMdp(const uint8_t* payload, uint16_t len) {
  if (!loraReady) return false;
  static uint8_t frame[cfg::MAX_FRAME];
  size_t n = mdp_build_frame(payload, len, frame, sizeof(frame));
  if (!n) return false;
  int st = radio.transmit(frame, n);
  radio.startReceive();
  return (st == RADIOLIB_ERR_NONE);
}
#else
// Stub functions when LoRa disabled
static bool loraInit() { return false; }
static bool loraSendMdp(const uint8_t*, uint16_t) { return false; }
#endif

// ========== WiFi Module ==========
#if ENABLE_WIFI
static bool wifiReady = false;
static uint32_t wifiLastReconnect = 0;
static WiFiUDP wifiUdp;
static char wifiSsid[64] = WIFI_SSID_DEFAULT;
static char wifiPass[64] = WIFI_PASS_DEFAULT;
static char gatewayHost[64] = GATEWAY_HOST_DEFAULT;
static uint16_t gatewayPort = GATEWAY_PORT_DEFAULT;

static bool wifiInit() {
  if (strlen(wifiSsid) == 0) {
    Serial.println("{\"wifi_init\":\"no_ssid\"}");
    return false;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);
  
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
    delay(100);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("{\"wifi_init\":\"ok\",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
    wifiUdp.begin(cfg::WIFI_UDP_PORT);
    wifiReady = true;
    return true;
  }
  
  Serial.println("{\"wifi_init\":\"fail\"}");
  return false;
}

static void wifiReconnectIfNeeded(uint32_t now) {
  if (wifiReady && WiFi.status() == WL_CONNECTED) return;
  if ((now - wifiLastReconnect) < cfg::WIFI_RECONNECT_MS) return;
  
  wifiLastReconnect = now;
  wifiReady = false;
  
  if (strlen(wifiSsid) > 0) {
    WiFi.disconnect();
    WiFi.begin(wifiSsid, wifiPass);
    
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 5000) {
      delay(50);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiReady = true;
      Serial.print("{\"wifi_reconnect\":\"ok\",\"ip\":\"");
      Serial.print(WiFi.localIP());
      Serial.println("\"}");
    }
  }
}

static bool wifiSendMdp(const uint8_t* payload, uint16_t len) {
  if (!wifiReady || WiFi.status() != WL_CONNECTED) return false;
  
  // Send via UDP to gateway
  static uint8_t frame[cfg::MAX_FRAME];
  size_t n = mdp_build_frame(payload, len, frame, sizeof(frame));
  if (!n) return false;
  
  IPAddress gatewayIP;
  if (!gatewayIP.fromString(gatewayHost)) return false;
  
  wifiUdp.beginPacket(gatewayIP, gatewayPort);
  wifiUdp.write(frame, n);
  return wifiUdp.endPacket() == 1;
}

static void wifiPollUdp() {
  if (!wifiReady) return;
  
  int packetSize = wifiUdp.parsePacket();
  if (packetSize > 0) {
    static uint8_t rxBuf[cfg::MAX_FRAME];
    int len = wifiUdp.read(rxBuf, sizeof(rxBuf));
    if (len > 0) {
      // Decode and handle incoming MDP packet
      static uint8_t payload[cfg::MAX_PAYLOAD];
      size_t plen = mdp_decode_frame(rxBuf, (size_t)len, payload, sizeof(payload));
      if (plen > 0) {
        // Handle as if from gateway (same as LoRa)
        // handleFromGW(payload, (uint16_t)plen);  // Called later
      }
    }
  }
}
#else
// Stub functions when WiFi disabled
static bool wifiInit() { return false; }
static void wifiReconnectIfNeeded(uint32_t) {}
static bool wifiSendMdp(const uint8_t*, uint16_t) { return false; }
static void wifiPollUdp() {}
#endif

// ========== BLE Module ==========
#if ENABLE_BLE
static bool bleReady = false;
static BLEServer* bleServer = nullptr;
static BLECharacteristic* bleTxChar = nullptr;
static BLECharacteristic* bleRxChar = nullptr;
static bool bleDeviceConnected = false;
static bool bleOldDeviceConnected = false;

// Buffer for incoming BLE data
static uint8_t bleRxBuffer[cfg::MAX_PAYLOAD];
static volatile size_t bleRxLen = 0;

class BLEServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleDeviceConnected = true;
    Serial.println("{\"ble\":\"connected\"}");
  }
  
  void onDisconnect(BLEServer* pServer) override {
    bleDeviceConnected = false;
    Serial.println("{\"ble\":\"disconnected\"}");
  }
};

class BLERxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string rxValue = pChar->getValue();
    if (rxValue.length() > 0 && rxValue.length() <= sizeof(bleRxBuffer)) {
      memcpy(bleRxBuffer, rxValue.data(), rxValue.length());
      bleRxLen = rxValue.length();
    }
  }
};

static bool bleInit() {
  BLEDevice::init(BLE_DEVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BLEServerCallbacks());
  
  BLEService* pService = bleServer->createService(BLE_SERVICE_UUID);
  
  // TX characteristic (notify)
  bleTxChar = pService->createCharacteristic(
    BLE_CHAR_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  bleTxChar->addDescriptor(new BLE2902());
  
  // RX characteristic (write)
  bleRxChar = pService->createCharacteristic(
    BLE_CHAR_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  bleRxChar->setCallbacks(new BLERxCallbacks());
  
  pService->start();
  
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleReady = true;
  Serial.println("{\"ble_init\":\"ok\"}");
  return true;
}

static bool bleSendMdp(const uint8_t* payload, uint16_t len) {
  if (!bleReady || !bleDeviceConnected || !bleTxChar) return false;
  if (len > 512) return false;  // BLE MTU limit
  
  bleTxChar->setValue((uint8_t*)payload, len);
  bleTxChar->notify();
  return true;
}

static void blePoll() {
  if (!bleReady) return;
  
  // Handle connection state changes
  if (!bleDeviceConnected && bleOldDeviceConnected) {
    delay(500);
    bleServer->startAdvertising();
  }
  bleOldDeviceConnected = bleDeviceConnected;
  
  // Process received BLE data
  if (bleRxLen > 0) {
    // Decode MDP frame from BLE
    static uint8_t payload[cfg::MAX_PAYLOAD];
    size_t plen = mdp_decode_frame(bleRxBuffer, bleRxLen, payload, sizeof(payload));
    if (plen > 0) {
      // Handle as command from BLE peer
      // Similar to handleFromGW
    }
    bleRxLen = 0;
  }
}
#else
// Stub functions when BLE disabled
static bool bleInit() { return false; }
static bool bleSendMdp(const uint8_t*, uint16_t) { return false; }
static void blePoll() {}
#endif

static void uartSendMdp(const uint8_t* payload, uint16_t len) {
  static uint8_t frame[cfg::MAX_FRAME];
  size_t n = mdp_build_frame(payload, len, frame, sizeof(frame));
  if (n) Serial2.write(frame, n);
}

// ---------- Reliability queues ----------
struct TxItem {
  bool used=false;
  bool viaLoRa=false;
  uint32_t seq=0;
  uint16_t len=0;
  uint8_t  payload[cfg::MAX_PAYLOAD];
  uint32_t lastSend=0;
  uint8_t retries=0;
  uint32_t rto=0;
};

static TxItem txq[8];
static uint32_t b_tx_seq = 1;
static uint32_t ack_from_a = 0;
static uint32_t ack_from_gw = 0;
static uint32_t last_inorder_a = 0;
static uint32_t last_inorder_gw = 0;

static TxItem* txAlloc() {
  for (auto &it: txq) {
    if (!it.used) {
      it.used = true;
      it.retries = 0;
      it.lastSend = 0;
      return &it;
    }
  }
  return nullptr;
}

static void txFreeAcked(bool viaLoRa, uint32_t ackVal) {
  for (auto &it: txq) {
    if (it.used && it.viaLoRa == viaLoRa && it.seq != 0 && it.seq <= ackVal) {
      it.used = false;
    }
  }
}

static void txEnqueue(bool viaLoRa, const uint8_t* payload, uint16_t len, uint32_t seq, uint32_t rto) {
  TxItem* it = txAlloc();
  if (!it) return;
  it->viaLoRa = viaLoRa;
  it->seq = seq;
  it->len = len;
  it->rto = rto;
  memcpy(it->payload, payload, len);
}

static void txSendNow(TxItem& it, uint32_t now) {
  if (!it.used) return;
  if (it.retries > cfg::MAX_RETRIES) { it.used = false; return; }
  if (it.viaLoRa) (void)loraSendMdp(it.payload, it.len);
  else uartSendMdp(it.payload, it.len);
  it.lastSend = now;
  it.retries++;
}

static void txPump(uint32_t now) {
  for (auto &it: txq) {
    if (!it.used) continue;
    uint32_t acked = it.viaLoRa ? ack_from_gw : ack_from_a;
    if (acked >= it.seq) { it.used = false; continue; }
    if (it.lastSend == 0 || (now - it.lastSend) >= it.rto) {
      txSendNow(it, now);
    }
  }
}

// ---------- ACK builders ----------
static void sendAckToA(bool requestAckBack=false) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = MDP_MAGIC;
  h->version = MDP_VER;
  h->msg_type = MDP_ACK;
  h->seq = b_tx_seq++;
  h->ack = last_inorder_a;
  h->flags = IS_ACK | (requestAckBack ? ACK_REQUESTED : 0);
  h->src = EP_SIDE_B;
  h->dst = EP_SIDE_A;
  h->rsv = 0;

  txEnqueue(false, out, sizeof(out), h->seq, cfg::UART_RTO_MS);
  uartSendMdp(out, sizeof(out));
}

static void sendAckToGW(bool requestAckBack=false) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = MDP_MAGIC;
  h->version = MDP_VER;
  h->msg_type = MDP_ACK;
  h->seq = b_tx_seq++;
  h->ack = last_inorder_gw;
  h->flags = IS_ACK | (requestAckBack ? ACK_REQUESTED : 0);
  h->src = EP_SIDE_B;
  h->dst = EP_GATEWAY;
  h->rsv = 0;

  txEnqueue(true, out, sizeof(out), h->seq, cfg::LORA_RTO_MS);
  (void)loraSendMdp(out, sizeof(out));
}

// ---------- UART RX (COBS framed) ----------
static uint8_t uart_rx[cfg::MAX_FRAME];
static size_t uart_rx_len = 0;
static uint8_t uart_payload[cfg::MAX_PAYLOAD];

static void handleFromA(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* h = (const mdp_hdr_v1_t*)p;
  if (h->magic != MDP_MAGIC || h->version != MDP_VER) return;

  ack_from_a = max(ack_from_a, h->ack);
  txFreeAcked(false, ack_from_a);

  if (h->seq == last_inorder_a + 1) last_inorder_a = h->seq;
  if (h->flags & ACK_REQUESTED) sendAckToA(false);

  // Forward telemetry reliably (LoRa can be lossy; this enables replay/ack).
  if (h->msg_type == MDP_TELEMETRY) {
    uint8_t out[cfg::MAX_PAYLOAD];
    if (len > sizeof(out)) return;
    memcpy(out, p, len);
    auto* oh = (mdp_hdr_v1_t*)out;
    oh->src = EP_SIDE_B;
    oh->dst = EP_GATEWAY;
    oh->seq = b_tx_seq++;
    oh->ack = last_inorder_gw;
    oh->flags |= ACK_REQUESTED;
    txEnqueue(true, out, len, oh->seq, cfg::LORA_RTO_MS);
    (void)loraSendMdp(out, len);
    return;
  }

  // Forward events reliably
  if (h->msg_type == MDP_EVENT) {
    uint8_t out[cfg::MAX_PAYLOAD];
    if (len > sizeof(out)) return;
    memcpy(out, p, len);

    auto* oh = (mdp_hdr_v1_t*)out;
    oh->src = EP_SIDE_B;
    oh->dst = EP_GATEWAY;
    oh->seq = b_tx_seq++;
    oh->ack = last_inorder_gw;
    oh->flags |= ACK_REQUESTED;

    txEnqueue(true, out, len, oh->seq, cfg::LORA_RTO_MS);
    (void)loraSendMdp(out, len);
  }
}

static void uartPoll() {
  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();
    if (b == 0x00) {
      if (uart_rx_len == 0) continue;
      size_t plen = mdp_decode_frame(uart_rx, uart_rx_len, uart_payload, sizeof(uart_payload));
      if (plen) handleFromA(uart_payload, (uint16_t)plen);
      uart_rx_len = 0;
      continue;
    }

    if (uart_rx_len < sizeof(uart_rx)) uart_rx[uart_rx_len++] = b;
    else uart_rx_len = 0;
  }
}

// ---------- LoRa RX (COBS framed) ----------
#if ENABLE_LORA
static uint8_t lora_rx[cfg::MAX_FRAME];
#endif

static void handleFromGW(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* h = (const mdp_hdr_v1_t*)p;
  if (h->magic != MDP_MAGIC || h->version != MDP_VER) return;

  ack_from_gw = max(ack_from_gw, h->ack);
  txFreeAcked(true, ack_from_gw);

  if (h->seq == last_inorder_gw + 1) last_inorder_gw = h->seq;
  if (h->flags & ACK_REQUESTED) sendAckToGW(false);

  // Commands from gateway -> forward reliably to Side-A
  if (h->msg_type == MDP_COMMAND) {
    uint8_t out[cfg::MAX_PAYLOAD];
    if (len > sizeof(out)) return;
    memcpy(out, p, len);

    auto* oh = (mdp_hdr_v1_t*)out;
    oh->src = EP_SIDE_B;
    oh->dst = EP_SIDE_A;
    oh->seq = b_tx_seq++;
    oh->ack = last_inorder_a;
    oh->flags |= ACK_REQUESTED;

    txEnqueue(false, out, len, oh->seq, cfg::UART_RTO_MS);
    uartSendMdp(out, len);
  }
}

#if ENABLE_LORA
static void loraPoll() {
  if (!loraReady) return;
  int16_t st = radio.receive(lora_rx, sizeof(lora_rx));
  if (st == RADIOLIB_ERR_NONE) {
    int pktLen = radio.getPacketLength();
    if (pktLen > 0) {
      // mdp_decode_frame can accept (encoded + 0x00)
      size_t plen = mdp_decode_frame(lora_rx, (size_t)pktLen, lora_rx, sizeof(lora_rx));
      if (plen) handleFromGW(lora_rx, (uint16_t)plen);
    }
    radio.startReceive();
  } else if (st == RADIOLIB_ERR_RX_TIMEOUT || st == RADIOLIB_ERR_CRC_MISMATCH) {
    radio.startReceive();
  }
}
#else
static void loraPoll() {}
#endif

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  // UART to Side-A (always enabled)
  Serial2.begin(cfg::UART_BAUD, SERIAL_8N1, cfg::PIN_B_RX2, cfg::PIN_B_TX2);
  
  // Initialize enabled communication modules
#if ENABLE_LORA
  (void)loraInit();
#endif

#if ENABLE_WIFI
  (void)wifiInit();
#endif

#if ENABLE_BLE
  (void)bleInit();
#endif

  // Status message with enabled modules
  Serial.print("{\"side\":\"B\",\"mdp\":1");
#if ENABLE_LORA
  Serial.print(",\"lora\":");
  Serial.print(loraReady ? "true" : "false");
#endif
#if ENABLE_WIFI
  Serial.print(",\"wifi\":");
  Serial.print(wifiReady ? "true" : "false");
#endif
#if ENABLE_BLE
  Serial.print(",\"ble\":");
  Serial.print(bleReady ? "true" : "false");
#endif
  Serial.println(",\"status\":\"ready\"}");
}

void loop() {
  uint32_t now = millis();
  
  // UART to Side-A (always polled)
  uartPoll();
  
  // Poll enabled communication modules
#if ENABLE_LORA
  loraPoll();
#endif

#if ENABLE_WIFI
  wifiReconnectIfNeeded(now);
  wifiPollUdp();
#endif

#if ENABLE_BLE
  blePoll();
#endif

  // Reliability queue pump
  txPump(now);
}