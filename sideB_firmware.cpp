/***************************************************
  MycoBrain V1 — Side-B Firmware
  UART <-> LoRa router with MDP v1 + COBS + CRC16
  ACK/Retry for commands/events, telemetry forwarded best-effort.

  LoRa pin map derived from MycoBrain SCH.PDF NLSX0* nets. :contentReference[oaicite:10]{index=10}
***************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ---------- COBS + CRC ----------
static size_t cobsEncode(const uint8_t* input, size_t length, uint8_t* output) {
  size_t read_index=0, write_index=1, code_index=0; uint8_t code=1;
  while (read_index < length) {
    if (input[read_index] == 0) { output[code_index]=code; code=1; code_index=write_index++; read_index++; }
    else {
      output[write_index++] = input[read_index++];
      if (++code == 0xFF) { output[code_index]=code; code=1; code_index=write_index++; }
    }
  }
  output[code_index]=code;
  return write_index;
}
static bool cobsDecode(const uint8_t* input, size_t length, uint8_t* output, size_t* outLen) {
  size_t read_index=0, write_index=0;
  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code==0 || read_index+code > length+1) return false;
    read_index++;
    for (uint8_t i=1;i<code;i++) output[write_index++] = input[read_index++];
    if (code != 0xFF && read_index < length) output[write_index++] = 0;
  }
  *outLen = write_index; return true;
}
static uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc=0xFFFF;
  for (size_t i=0;i<len;i++){ crc ^= (uint16_t)data[i]<<8; for(int b=0;b<8;b++) crc = (crc&0x8000)?(crc<<1)^0x1021:(crc<<1); }
  return crc;
}

// ---------- MDP ----------
namespace cfg {
constexpr uint32_t USB_BAUD = 115200;

// UART to Side-A (edit if your board routes differently)
constexpr uint32_t UART_BAUD = 115200;
constexpr int PIN_B_RX2 = 9;
constexpr int PIN_B_TX2 = 8;

// MDP
constexpr uint16_t MDP_MAGIC = 0xA15A;
constexpr uint8_t  MDP_VER   = 1;

// endpoints
constexpr uint8_t EP_SIDE_A  = 0xA1;
constexpr uint8_t EP_SIDE_B  = 0xB1;
constexpr uint8_t EP_GATEWAY = 0xG0;

// buffers
constexpr size_t MAX_FRAME   = 1200;
constexpr size_t MAX_PAYLOAD = 900;

// reliability
constexpr uint32_t UART_RTO_MS = 120;
constexpr uint32_t LORA_RTO_MS = 1800;
constexpr uint8_t  MAX_RETRIES = 5;

// ===== SX1262 pin map (from schematic NLSX0*) =====
// NLSX0CLK  -> GPIO9   :contentReference[oaicite:11]{index=11}
// NLSX0CS   -> GPIO13  :contentReference[oaicite:12]{index=12}
// NLSX0MISO -> GPIO12  :contentReference[oaicite:13]{index=13}
// NLSX0Busy -> GPIO10  :contentReference[oaicite:14]{index=14}
// NLSX0DI01 -> GPIO14  :contentReference[oaicite:15]{index=15}
// NLSX0DI02 -> GPIO11  :contentReference[oaicite:16]{index=16}
constexpr int LORA_SCK  = 9;
constexpr int LORA_MISO = 12;
constexpr int LORA_MOSI = 8;   // practical default; confirm MOSI net if needed
constexpr int LORA_NSS  = 13;
constexpr int LORA_DIO1 = 14;
constexpr int LORA_RST  = RADIOLIB_NC; // set if you confirm reset GPIO
constexpr int LORA_BUSY = 10;

constexpr float LORA_FREQ_MHZ = 915.0;
constexpr int   LORA_SF       = 9;
constexpr float LORA_BW_KHZ   = 125.0;
constexpr int   LORA_CR       = 7;
constexpr int   LORA_PREAMBLE = 12;
constexpr int   LORA_TX_DBM   = 14;
} // namespace cfg

enum MdpMsgType : uint8_t { MDP_TELEMETRY=0x01, MDP_COMMAND=0x02, MDP_ACK=0x03, MDP_EVENT=0x05 };
enum MdpFlags : uint8_t { ACK_REQUESTED=0x01, IS_ACK=0x02 };

#pragma pack(push,1)
struct mdp_hdr_v1_t {
  uint16_t magic; uint8_t version; uint8_t msg_type;
  uint32_t seq; uint32_t ack;
  uint8_t flags; uint8_t src; uint8_t dst; uint8_t rsv;
};
#pragma pack(pop)

// ---------- LoRa ----------
SPIClass spiLora(FSPI);
SX1262 radio = new Module(cfg::LORA_NSS, cfg::LORA_DIO1, cfg::LORA_RST, cfg::LORA_BUSY);

static int crToRadioLib(int cr){ if(cr>=5 && cr<=8) return cr; return 7; }

static bool loraInit() {
  spiLora.begin(cfg::LORA_SCK, cfg::LORA_MISO, cfg::LORA_MOSI, cfg::LORA_NSS);
  radio.setSPI(spiLora);
  int st = radio.begin(cfg::LORA_FREQ_MHZ, cfg::LORA_BW_KHZ, cfg::LORA_SF, crToRadioLib(cfg::LORA_CR),
                       cfg::LORA_PREAMBLE, cfg::LORA_TX_DBM);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("{\"lora_init\":\"fail\",\"err\":"); Serial.print(st); Serial.println("}");
    return false;
  }
  Serial.println("{\"lora_init\":\"ok\"}");
  radio.startReceive();
  return true;
}

// ---------- Framing helpers ----------
static void uartSendCOBS(const uint8_t* payload, uint16_t len) {
  static uint8_t raw[cfg::MAX_FRAME];
  static uint8_t enc[cfg::MAX_FRAME];
  if (len + 2 > sizeof(raw)) return;
  memcpy(raw, payload, len);
  uint16_t crc = crc16_ccitt_false(payload, len);
  raw[len] = crc & 0xFF; raw[len+1] = (crc >> 8) & 0xFF;
  size_t encLen = cobsEncode(raw, len+2, enc);
  Serial2.write(enc, encLen);
  Serial2.write((uint8_t)0x00);
}

static bool loraSendBinary(const uint8_t* payload, uint16_t len) {
  static uint8_t raw[cfg::MAX_FRAME];
  static uint8_t enc[cfg::MAX_FRAME];
  if (len + 2 > sizeof(raw)) return false;
  memcpy(raw, payload, len);
  uint16_t crc = crc16_ccitt_false(payload, len);
  raw[len] = crc & 0xFF; raw[len+1] = (crc >> 8) & 0xFF;
  size_t encLen = cobsEncode(raw, len+2, enc);
  if (encLen + 1 > sizeof(enc)) return false;
  enc[encLen] = 0x00;
  int st = radio.transmit(enc, encLen + 1);
  radio.startReceive();
  return (st == RADIOLIB_ERR_NONE);
}

// ---------- Reliability queues ----------
struct TxItem {
  bool used=false;
  uint32_t seq=0;
  uint16_t len=0;
  uint8_t  payload[cfg::MAX_PAYLOAD];
  uint32_t lastSend=0;
  uint8_t retries=0;
  uint32_t rto=0;
  bool viaLoRa=false;
};
static TxItem txq[8];

static uint32_t b_tx_seq = 1;
static uint32_t ack_from_a = 0;
static uint32_t ack_from_gw = 0;
static uint32_t last_inorder_a = 0;
static uint32_t last_inorder_gw = 0;

static TxItem* txAlloc() { for (auto &it:txq) if(!it.used){ it.used=true; it.retries=0; it.lastSend=0; return &it; } return nullptr; }
static void txFreeAcked(bool viaLoRa, uint32_t ackVal) {
  for (auto &it:txq) if(it.used && it.viaLoRa==viaLoRa && it.seq!=0 && it.seq<=ackVal) it.used=false;
}
static void txEnqueue(bool viaLoRa, const uint8_t* payload, uint16_t len, uint32_t seq, uint32_t rto) {
  TxItem* it=txAlloc(); if(!it) return;
  it->viaLoRa=viaLoRa; it->seq=seq; it->len=len; it->rto=rto;
  memcpy(it->payload,payload,len);
}
static void txSendNow(TxItem& it, uint32_t now) {
  if (!it.used) return;
  if (it.retries > cfg::MAX_RETRIES) { it.used=false; return; }
  if (it.viaLoRa) loraSendBinary(it.payload, it.len);
  else uartSendCOBS(it.payload, it.len);
  it.lastSend = now;
  it.retries++;
}
static void txPump(uint32_t now) {
  for (auto &it:txq) {
    if (!it.used) continue;
    uint32_t acked = it.viaLoRa ? ack_from_gw : ack_from_a;
    if (acked >= it.seq) { it.used=false; continue; }
    if (it.lastSend==0 || now - it.lastSend >= it.rto) txSendNow(it, now);
  }
}

// ---------- ACK builders ----------
static void sendAckToA(bool requestAckBack=false) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h=(mdp_hdr_v1_t*)out;
  h->magic=cfg::MDP_MAGIC; h->version=cfg::MDP_VER; h->msg_type=MDP_ACK;
  h->seq=b_tx_seq++; h->ack=last_inorder_a;
  h->flags=IS_ACK | (requestAckBack?ACK_REQUESTED:0);
  h->src=cfg::EP_SIDE_B; h->dst=cfg::EP_SIDE_A; h->rsv=0;
  txEnqueue(false,out,sizeof(out),h->seq,cfg::UART_RTO_MS);
  uartSendCOBS(out,sizeof(out));
}
static void sendAckToGW(bool requestAckBack=false) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h=(mdp_hdr_v1_t*)out;
  h->magic=cfg::MDP_MAGIC; h->version=cfg::MDP_VER; h->msg_type=MDP_ACK;
  h->seq=b_tx_seq++; h->ack=last_inorder_gw;
  h->flags=IS_ACK | (requestAckBack?ACK_REQUESTED:0);
  h->src=cfg::EP_SIDE_B; h->dst=cfg::EP_GATEWAY; h->rsv=0;
  txEnqueue(true,out,sizeof(out),h->seq,cfg::LORA_RTO_MS);
  loraSendBinary(out,sizeof(out));
}

// ---------- UART RX (COBS) ----------
static uint8_t uart_rx_frame[cfg::MAX_FRAME];
static size_t uart_rx_len=0;
static uint8_t uart_dec[cfg::MAX_FRAME];
static size_t uart_dec_len=0;

static void handleFromA(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* h=(const mdp_hdr_v1_t*)p;
  if (h->magic!=cfg::MDP_MAGIC || h->version!=cfg::MDP_VER) return;

  ack_from_a = max(ack_from_a, h->ack);
  txFreeAcked(false, ack_from_a);

  if (h->seq == last_inorder_a + 1) last_inorder_a = h->seq;
  if (h->flags & ACK_REQUESTED) sendAckToA(false);

  // Telemetry best-effort uplink
  if (h->msg_type == MDP_TELEMETRY) {
    loraSendBinary(p, len);
    return;
  }

  // Events reliable uplink
  if (h->msg_type == MDP_EVENT) {
    uint8_t out[cfg::MAX_PAYLOAD];
    if (len > sizeof(out)) return;
    memcpy(out, p, len);
    auto* oh=(mdp_hdr_v1_t*)out;
    oh->dst = cfg::EP_GATEWAY;
    oh->src = cfg::EP_SIDE_B;
    oh->seq = b_tx_seq++;
    oh->ack = last_inorder_gw;
    oh->flags |= ACK_REQUESTED;

    txEnqueue(true,out,len,oh->seq,cfg::LORA_RTO_MS);
    loraSendBinary(out,len);
    return;
  }
}

static void uartPollCOBS() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (b==0x00) {
      if (uart_rx_len==0) continue;
      if (!cobsDecode(uart_rx_frame, uart_rx_len, uart_dec, &uart_dec_len)) { uart_rx_len=0; continue; }
      if (uart_dec_len < 2) { uart_rx_len=0; continue; }
      uint16_t recv = uart_dec[uart_dec_len-2] | ((uint16_t)uart_dec[uart_dec_len-1]<<8);
      uint16_t calc = crc16_ccitt_false(uart_dec, uart_dec_len-2);
      if (recv==calc) handleFromA(uart_dec,(uint16_t)(uart_dec_len-2));
      uart_rx_len=0; continue;
    }
    if (uart_rx_len < sizeof(uart_rx_frame)) uart_rx_frame[uart_rx_len++] = b;
    else uart_rx_len=0;
  }
}

// ---------- LoRa RX (COBS framed over LoRa) ----------
static uint8_t lora_rx_buf[cfg::MAX_FRAME];

static void handleFromGW(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* h=(const mdp_hdr_v1_t*)p;
  if (h->magic!=cfg::MDP_MAGIC || h->version!=cfg::MDP_VER) return;

  ack_from_gw = max(ack_from_gw, h->ack);
  txFreeAcked(true, ack_from_gw);

  if (h->seq == last_inorder_gw + 1) last_inorder_gw = h->seq;
  if (h->flags & ACK_REQUESTED) sendAckToGW(false);

  // Commands from gateway -> forward reliable to Side-A over UART
  if (h->msg_type == MDP_COMMAND) {
    uint8_t out[cfg::MAX_PAYLOAD];
    if (len > sizeof(out)) return;
    memcpy(out, p, len);
    auto* oh=(mdp_hdr_v1_t*)out;
    oh->dst = cfg::EP_SIDE_A;
    oh->src = cfg::EP_SIDE_B;
    oh->seq = b_tx_seq++;
    oh->ack = last_inorder_a;
    oh->flags |= ACK_REQUESTED;

    txEnqueue(false,out,len,oh->seq,cfg::UART_RTO_MS);
    uartSendCOBS(out,len);
  }
}

static void loraPoll() {
  int16_t st = radio.receive(lora_rx_buf, sizeof(lora_rx_buf));
  if (st == RADIOLIB_ERR_NONE) {
    int pktLen = radio.getPacketLength();
    if (pktLen <= 1) { radio.startReceive(); return; }
    if (lora_rx_buf[pktLen-1] != 0x00) { radio.startReceive(); return; }

    uint8_t dec[cfg::MAX_FRAME];
    size_t decL=0;
    if (!cobsDecode(lora_rx_buf, pktLen-1, dec, &decL)) { radio.startReceive(); return; }
    if (decL < 2) { radio.startReceive(); return; }
    uint16_t recv = dec[decL-2] | ((uint16_t)dec[decL-1]<<8);
    uint16_t calc = crc16_ccitt_false(dec, decL-2);
    if (recv==calc) handleFromGW(dec,(uint16_t)(decL-2));
    radio.startReceive();
  } else if (st == RADIOLIB_ERR_RX_TIMEOUT || st == RADIOLIB_ERR_CRC_MISMATCH) {
    radio.startReceive();
  }
}

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  Serial2.begin(cfg::UART_BAUD, SERIAL_8N1, cfg::PIN_B_RX2, cfg::PIN_B_TX2);

  loraInit();

  Serial.println("{\"side\":\"B\",\"mdp\":1,\"status\":\"ready\"}");
}

void loop() {
  uint32_t now = millis();
  uartPollCOBS();
  loraPoll();
  txPump(now);
}
