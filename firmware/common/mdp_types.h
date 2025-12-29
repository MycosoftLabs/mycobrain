#ifndef MDP_TYPES_H
#define MDP_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDP_MAGIC 0xA15A
#define MDP_VER   1

// Endpoints
#define EP_SIDE_A   0xA1
#define EP_SIDE_B   0xB1
#define EP_GATEWAY  0xC0
#define EP_BCAST    0xFF

// Message Types
enum MdpMsgType : uint8_t {
  MDP_TELEMETRY = 0x01,
  MDP_COMMAND   = 0x02,
  MDP_ACK       = 0x03,
  MDP_EVENT     = 0x05,
  MDP_HELLO     = 0x06,
  MDP_WIFISENSE = 0x07,  // WiFi Sense telemetry
  MDP_DRONE_TELEMETRY = 0x08,  // Drone telemetry
  MDP_DRONE_MISSION_STATUS = 0x09  // Drone mission status
};

// Flags
enum MdpFlags : uint8_t {
  ACK_REQUESTED = 0x01,
  IS_ACK        = 0x02,
  IS_NACK       = 0x04
};

#pragma pack(push,1)
typedef struct mdp_hdr_v1_t {
  uint16_t magic;
  uint8_t  version;
  uint8_t  msg_type;
  uint32_t seq;
  uint32_t ack;
  uint8_t  flags;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  rsv;
} mdp_hdr_v1_t;

typedef struct mdp_cmd_v1_t {
  mdp_hdr_v1_t hdr;
  uint16_t cmd_id;
  uint16_t cmd_len;
  uint8_t  cmd_data[];
} mdp_cmd_v1_t;

typedef struct mdp_evt_cmd_result_v1_t {
  mdp_hdr_v1_t hdr;
  uint16_t evt_type;
  uint16_t evt_len;
  uint16_t cmd_id;
  int16_t  status;
  uint8_t  data[];
} mdp_evt_cmd_result_v1_t;
#pragma pack(pop)

#define EVT_CMD_RESULT 0x0001

#ifdef __cplusplus
}
#endif

#endif