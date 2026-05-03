#ifndef MDP_V2_FUSARIUM_H
#define MDP_V2_FUSARIUM_H

// Unified MDP v2 message type definitions for FUSARIUM defense telemetry
// This file is the canonical enum mapping for MycoBrain firmware.

typedef enum {
    MDP_V2_HEARTBEAT = 0x01,
    MDP_V2_SENSOR_TELEMETRY = 0x02,
    MDP_V2_COMMAND = 0x03,
    MDP_V2_ACK = 0x04,

    MDP_V2_ACOUSTIC_RAW = 0x20,
    MDP_V2_ACOUSTIC_FINGERPRINT = 0x21,
    MDP_V2_MAGNETIC_ANOMALY = 0x22,
    MDP_V2_OCEAN_ENVIRONMENT = 0x23,
    MDP_V2_TACTICAL_ASSESSMENT = 0x24,
    MDP_V2_ZEETA_BRIDGE = 0x25
} mdp_v2_message_type_t;

#endif // MDP_V2_FUSARIUM_H
