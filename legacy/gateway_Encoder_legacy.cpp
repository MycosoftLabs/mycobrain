/***************************************************
  Mycosoft Gateway — LoRa <-> USB Bridge (MDP v1)
  - RX LoRa: decode COBS frame, CRC check, print NDJSON to USB
  - TX LoRa: accept simple CLI on USB, send MDP COMMAND with ACK_REQUESTED

  Uses RadioLib SX1262
***************************************************/
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ---- COBS + CRC ----
static size_t cobsEncode(const uint8_t* in, size_t n, uint8_t* out){ size_t r=0,w=1,c=0; uint8_t code=1; while(r<n){ if(in[r]==0){ out[c]=code; code=1; c=w++; r++; } else { out[w++]=in[r++]; if(++code==0xFF){ out[c]=code; code=1; c=w++; }}} out[c]=code; return w; }
static bool cobsDecode(const uint8_t* in, size_t n, uint8_t* out, size_t* outN){ size_t r=0,w=0; while(r<n){ uint8_t code=in[r]; if(code==0||r+code>n+1) return false; r++; for(uint8_t i=1;i<code;i++) out[w++]=in[r++]; if(code!=0xFF && r<n) out[w++]=0; } *outN=w; return true; }
static uint16_t crc16_ccitt_false(const uint8_t* data, size_t len){ uint16_t crc=0xFFFF; for(size_t i=0;i<len;i++){ crc^=(uint16_t)data[i]<<8; for(int b=0;b<8;b++) crc=(crc&0x8000)?(crc<<1)^0x1021:(crc<<1);} return crc; }

// ---- MDP ----
namespace cfg {
constexpr uint32_t USB_BAUD=115200;
constexpr uint16_t MDP_MAGIC=0xA15A;
constexpr uint8_t  MDP_VER=1;
constexpr uint8_t  EP_GATEWAY=0xG0;
constexpr uint8_t  EP_SIDE_B=0xB1;

constexpr size_t MAX_FRAME=1200;
constexpr size_t MAX_PAYLOAD=900;

// LoRa pins: set for your gateway board (example uses MycoBrain map)
constexpr int LORA_SCK=9, LORA_MISO=12, LORA_MOSI=8, LORA_NSS=13, LORA_DIO1=14, LORA_BUSY=10;
constexpr int LORA_RST=RADIOLIB_NC;

constexpr float LORA_FREQ_MHZ=915.0;
constexpr int   LORA_SF=9;
constexpr float LORA_BW_KHZ=125.0;
constexpr int   LORA_CR=7;
constexpr int   LORA_PREAMBLE=12;
constexpr int   LORA_TX_DBM=14;
}

enum : uint8_t { MDP_TELEMETRY=0x01, MDP_COMMAND=0x02, MDP_ACK=0x03, MDP_EVENT=0x05 };
enum : uint8_t { ACK_REQUESTED=0x01, IS_ACK=0x02 };

#pragma pack(push,1)
struct mdp_hdr_v1_t{
  uint16_t magic; uint8_t version; uint8_t msg_type;
  uint32_t seq; uint32_t ack;
  uint8_t flags; uint8_t src; uint8_t dst; uint8_t rsv;
};
#pragma pack(pop)

SPIClass spiLora(FSPI);
SX1262 radio = new Module(cfg::LORA_NSS, cfg::LORA_DIO1, cfg::LORA_RST, cfg::LORA_BUSY);

static int crToRadioLib(int cr){ if(cr>=5&&cr<=8) return cr; return 7; }

static uint32_t gw_seq=1;
static uint32_t last_inorder=0;

static bool loraSendPayload(const uint8_t* payload, uint16_t len){
  static uint8_t raw[cfg::MAX_FRAME];
  static uint8_t enc[cfg::MAX_FRAME];
  if(len+2>sizeof(raw)) return false;
  memcpy(raw,payload,len);
  uint16_t crc=crc16_ccitt_false(payload,len);
  raw[len]=crc&0xFF; raw[len+1]=(crc>>8)&0xFF;
  size_t encLen=cobsEncode(raw,len+2,enc);
  enc[encLen]=0x00;
  int st=radio.transmit(enc,encLen+1);
  radio.startReceive();
  return st==RADIOLIB_ERR_NONE;
}

static void sendAck(uint32_t ackVal){
  mdp_hdr_v1_t h{};
  h.magic=cfg::MDP_MAGIC; h.version=cfg::MDP_VER; h.msg_type=MDP_ACK;
  h.seq=gw_seq++; h.ack=ackVal;
  h.flags=IS_ACK;
  h.src=cfg::EP_GATEWAY; h.dst=cfg::EP_SIDE_B;
  loraSendPayload((uint8_t*)&h,sizeof(h));
}

static void initLoRa(){
  spiLora.begin(cfg::LORA_SCK,cfg::LORA_MISO,cfg::LORA_MOSI,cfg::LORA_NSS);
  radio.setSPI(spiLora);
  int st=radio.begin(cfg::LORA_FREQ_MHZ,cfg::LORA_BW_KHZ,cfg::LORA_SF,crToRadioLib(cfg::LORA_CR),cfg::LORA_PREAMBLE,cfg::LORA_TX_DBM);
  Serial.print("{\"lora_init\":"); Serial.print(st==RADIOLIB_ERR_NONE?"\"ok\"":"\"fail\""); Serial.println("}");
  radio.startReceive();
}

static void handlePayload(const uint8_t* p, uint16_t len){
  if(len<sizeof(mdp_hdr_v1_t)) return;
  auto* h=(const mdp_hdr_v1_t*)p;
  if(h->magic!=cfg::MDP_MAGIC || h->version!=cfg::MDP_VER) return;

  if(h->seq==last_inorder+1) last_inorder=h->seq;

  // auto-ACK if requested
  if(h->flags & ACK_REQUESTED) sendAck(last_inorder);

  // print minimal NDJSON
  Serial.print("{\"rx\":1,\"type\":"); Serial.print((int)h->msg_type);
  Serial.print(",\"seq\":"); Serial.print(h->seq);
  Serial.print(",\"src\":"); Serial.print((int)h->src);
  Serial.print(",\"dst\":"); Serial.print((int)h->dst);
  Serial.println("}");
}

static uint8_t rxbuf[cfg::MAX_FRAME];
static void pollLoRa(){
  int st=radio.receive(rxbuf,sizeof(rxbuf));
  if(st==RADIOLIB_ERR_NONE){
    int n=radio.getPacketLength();
    if(n>1 && rxbuf[n-1]==0x00){
      uint8_t dec[cfg::MAX_FRAME]; size_t decN=0;
      if(cobsDecode(rxbuf,n-1,dec,&decN) && decN>=2){
        uint16_t recv=dec[decN-2] | ((uint16_t)dec[decN-1]<<8);
        uint16_t calc=crc16_ccitt_false(dec,decN-2);
        if(recv==calc) handlePayload(dec,(uint16_t)(decN-2));
      }
    }
    radio.startReceive();
  } else if(st==RADIOLIB_ERR_RX_TIMEOUT || st==RADIOLIB_ERR_CRC_MISMATCH){
    radio.startReceive();
  }
}

// USB command format:
//   scan
//   mos <idx 1..3> <0|1>
// These are forwarded as MDP_COMMAND to Side-B.
static void sendCommand(uint16_t cmd_id, const uint8_t* data, uint16_t dataLen){
  uint8_t buf[128];
  if(sizeof(mdp_hdr_v1_t)+4+dataLen>sizeof(buf)) return;
  auto* h=(mdp_hdr_v1_t*)buf;
  h->magic=cfg::MDP_MAGIC; h->version=cfg::MDP_VER; h->msg_type=MDP_COMMAND;
  h->seq=gw_seq++; h->ack=last_inorder;
  h->flags=ACK_REQUESTED;
  h->src=cfg::EP_GATEWAY; h->dst=cfg::EP_SIDE_B; h->rsv=0;
  // cmd header
  uint16_t* p16=(uint16_t*)(buf+sizeof(mdp_hdr_v1_t));
  p16[0]=cmd_id;
  p16[1]=dataLen;
  if(dataLen) memcpy(buf+sizeof(mdp_hdr_v1_t)+4,data,dataLen);
  loraSendPayload(buf,(uint16_t)(sizeof(mdp_hdr_v1_t)+4+dataLen));
}

static void pollUsbCli(){
  static String line;
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='\n'){
      line.trim();
      if(line=="scan"){
        sendCommand(0x0002,nullptr,0);
      } else if(line.startsWith("mos ")){
        int i=-1,v=-1;
        if(sscanf(line.c_str(),"mos %d %d",&i,&v)==2){
          uint8_t d[2]={(uint8_t)i,(uint8_t)v};
          sendCommand(0x0004,d,2);
        }
      }
      line="";
    } else line += c;
  }
}

void setup(){
  Serial.begin(cfg::USB_BAUD);
  delay(50);
  initLoRa();
  Serial.println("{\"gateway\":\"mdp\",\"status\":\"ready\"}");
}
void loop(){
  pollLoRa();
  pollUsbCli();
}
