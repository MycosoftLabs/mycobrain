#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

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
}

// RadioLib
SX1262 radio = new Module(cfg::LORA_NSS, cfg::LORA_DIO1, cfg::LORA_RST, cfg::LORA_BUSY);

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
  return true;
}

static bool loraSendMdp(const uint8_t* payload, uint16_t len) {
  static uint8_t frame[cfg::MAX_FRAME];
  size_t n = mdp_build_frame(payload, len, frame, sizeof(frame));
  if (!n) return false;
  int st = radio.transmit(frame, n);
  radio.startReceive();
  return (st == RADIOLIB_ERR_NONE);
}

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

  // Forward telemetry best-effort
  if (h->msg_type == MDP_TELEMETRY) {
    (void)loraSendMdp(p, len);
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
static uint8_t lora_rx[cfg::MAX_FRAME];

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

static void loraPoll() {
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

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  Serial2.begin(cfg::UART_BAUD, SERIAL_8N1, cfg::PIN_B_RX2, cfg::PIN_B_TX2);
  (void)loraInit();

  Serial.println("{\"side\":\"B\",\"mdp\":1,\"status\":\"ready\"}");
}

void loop() {
  uint32_t now = millis();
  uartPoll();
  loraPoll();
  txPump(now);
}