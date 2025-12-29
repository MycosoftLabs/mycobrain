#ifndef MAVLINK_BRIDGE_H
#define MAVLINK_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/mdp_drone_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize MAVLink bridge
bool mavlink_bridge_init(uint8_t uart_num, uint32_t baud_rate);

// Process incoming MAVLink messages
void mavlink_bridge_process(void);

// Send drone telemetry (converted from MAVLink to MDP)
bool mavlink_bridge_send_telemetry(const drone_telemetry_v1_t* telemetry);

// Send command to flight controller (converted from MDP to MAVLink)
bool mavlink_bridge_send_command(uint16_t cmd_id, const uint8_t* data, uint16_t len);

// Get latest telemetry
bool mavlink_bridge_get_telemetry(drone_telemetry_v1_t* telemetry);

#ifdef __cplusplus
}
#endif

#endif

