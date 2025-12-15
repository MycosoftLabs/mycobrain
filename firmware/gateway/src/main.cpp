#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <ArduinoJson.h>

#include <mdp_types.h>
#include <mdp_utils.h>

namespace cfg {
constexpr uint32_t USB_BAUD = 115200;

constexpr size_t MAX_FRAME   = 1200;
constexpr size_t MAX_PAYLOAD = 900;

// LoRa reliability
constexpr uint32_t LORA_RTO_MS = 1800;
constexpr uint8_t  MAX_RETRIES = 5;

// ===== SX1262 pin map (authoritative) =====
constexpr int LORA_RST  = 7;
constexpr int LORA_BUSY = 12;
constexpr int LORA_SCK  = 18;
constexpr int LORA_NSS  = 17;
constexpr int LORA_MISO = 19;
constexpr int LORA_MOSI = 20;
constexpr int LORA_DIO1 = 21;

constexpr float LORA_FREQ_MHZ = 915.0;
}

SX1262 radio = new Module(cfg::LORA_NSS, cfg::LORA_DIO1, cfg::LORA_RST, cfg::LORA_BUSY);

static uint32_t gw_tx_seq = 1;
static uint32_t ack_from_b = 0;
static uint32_t last_inorder_b = 0;

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

static void sendAckToB(bool requestAckBack=false) {
  uint8_t out[sizeof(mdp_hdr_v1_t)];
  auto* h = (mdp_hdr_v1_t*)out;
  h->magic = MDP_MAGIC;
  h->version = MDP_VER;
  h->msg_type = MDP_ACK;
  h->seq = gw_tx_seq++;
  h->ack = last_inorder_b;
  h->flags = IS_ACK | (requestAckBack ? ACK_REQUESTED : 0);
  h->src = EP_GATEWAY;
  h->dst = EP_SIDE_B;
  h->rsv = 0;
  (void)loraSendMdp(out, sizeof(out));
}

// Simple command retransmit queue
struct TxItem {
  bool used=false;
  uint32_t seq=0;
  uint16_t len=0;
  uint8_t payload[cfg::MAX_PAYLOAD];
  uint32_t lastSend=0;
  uint8_t retries=0;
};
static TxItem txq[4];

static TxItem* txAlloc(){ for(auto &it:txq){ if(!it.used){ it.used=true; it.retries=0; it.lastSend=0; return &it; } } return nullptr; }
static void txFreeAcked(uint32_t ackVal){ for(auto &it:txq){ if(it.used && it.seq && it.seq<=ackVal) it.used=false; } }
static void txEnqueue(const uint8_t* payload, uint16_t len, uint32_t seq){ auto* it=txAlloc(); if(!it) return; it->seq=seq; it->len=len; memcpy(it->payload,payload,len); }
static void txPump(uint32_t now){
  for(auto &it:txq){
    if(!it.used) continue;
    if(ack_from_b >= it.seq){ it.used=false; continue; }
    if(it.lastSend==0 || now-it.lastSend>=cfg::LORA_RTO_MS){
      if(it.retries > cfg::MAX_RETRIES){ it.used=false; continue; }
      (void)loraSendMdp(it.payload, it.len);
      it.lastSend = now;
      it.retries++;
    }
  }
}

static uint8_t lora_rx[cfg::MAX_FRAME];

static void handleFromB(const uint8_t* p, uint16_t len) {
  if (len < sizeof(mdp_hdr_v1_t)) return;
  auto* h = (const mdp_hdr_v1_t*)p;
  if (h->magic != MDP_MAGIC || h->version != MDP_VER) return;

  ack_from_b = max(ack_from_b, h->ack);
  txFreeAcked(ack_from_b);

  if (h->seq == last_inorder_b + 1) last_inorder_b = h->seq;
  if (h->flags & ACK_REQUESTED) sendAckToB(false);

  StaticJsonDocument<512> doc;
  doc["t_ms"] = (uint32_t)millis();
  doc["src"] = h->src;
  doc["dst"] = h->dst;
  doc["seq"] = h->seq;
  doc["ack"] = h->ack;
  doc["type"] = h->msg_type;
  doc["flags"] = h->flags;

  serializeJson(doc, Serial);
  Serial.println();
}

static void loraPoll() {
  int16_t st = radio.receive(lora_rx, sizeof(lora_rx));
  if (st == RADIOLIB_ERR_NONE) {
    int pktLen = radio.getPacketLength();
    if (pktLen > 0) {
      size_t plen = mdp_decode_frame(lora_rx, (size_t)pktLen, lora_rx, sizeof(lora_rx));
      if (plen) handleFromB(lora_rx, (uint16_t)plen);
    }
    radio.startReceive();
  } else if (st == RADIOLIB_ERR_RX_TIMEOUT || st == RADIOLIB_ERR_CRC_MISMATCH) {
    radio.startReceive();
  }
}

// USB command injector: expects one-line JSON like:
// {"cmd":2,"dst":161,"data":[1,2,3]}
static void usbPoll() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line.trim();
      if (line.length() == 0) { line = ""; continue; }

      StaticJsonDocument<384> doc;
      auto err = deserializeJson(doc, line);
      line = "";
      if (err) {
        Serial.println("{\"error\":\"json_parse\"}");
        return;
      }

      uint16_t cmd_id = (uint16_t)(doc["cmd"] | 0);
      uint8_t dst = (uint8_t)(doc["dst"] | (int)EP_SIDE_A);

      uint8_t out[cfg::MAX_PAYLOAD];
      auto* cmd = (mdp_cmd_v1_t*)out;
      cmd->hdr.magic = MDP_MAGIC;
      cmd->hdr.version = MDP_VER;
      cmd->hdr.msg_type = MDP_COMMAND;
      cmd->hdr.seq = gw_tx_seq++;
      cmd->hdr.ack = last_inorder_b;
      cmd->hdr.flags = ACK_REQUESTED;
      cmd->hdr.src = EP_GATEWAY;
      cmd->hdr.dst = dst;
      cmd->hdr.rsv = 0;

      cmd->cmd_id = cmd_id;
      uint16_t cmd_len = 0;
      if (doc.containsKey("data")) {
        JsonArray arr = doc["data"].as<JsonArray>();
        for (JsonVariant v : arr) {
          if (sizeof(mdp_cmd_v1_t) + cmd_len >= sizeof(out)) break;
          out[sizeof(mdp_cmd_v1_t) + cmd_len++] = (uint8_t)(v.as<int>() & 0xFF);
        }
      }
      cmd->cmd_len = cmd_len;

      uint16_t total = (uint16_t)(sizeof(mdp_cmd_v1_t) + cmd_len);
      txEnqueue(out, total, cmd->hdr.seq);
      (void)loraSendMdp(out, total);

      Serial.print("{\"sent\":true,\"seq\":");
      Serial.print(cmd->hdr.seq);
      Serial.println("}");

      return;
    } else {
      line += c;
    }
  }
}

void setup() {
  Serial.begin(cfg::USB_BAUD);
  delay(50);

  (void)loraInit();
  Serial.println("{\"side\":\"gateway\",\"mdp\":1,\"status\":\"ready\"}");
}

void loop() {
  uint32_t now = millis();
  loraPoll();
  usbPoll();
  txPump(now);
}