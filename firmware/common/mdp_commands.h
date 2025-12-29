#ifndef MDP_COMMANDS_H
#define MDP_COMMANDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_SET_I2C        0x0001
#define CMD_SCAN_I2C       0x0002
#define CMD_SET_TELEM_MS   0x0003
#define CMD_SET_MOS        0x0004
#define CMD_SAVE_NVS       0x0007
#define CMD_LOAD_NVS       0x0008
#define CMD_REBOOT         0x0009

// WiFi Sense commands
#define CMD_WIFISENSE_START     0x0010
#define CMD_WIFISENSE_STOP      0x0011
#define CMD_WIFISENSE_CONFIG    0x0012
#define CMD_WIFISENSE_CALIBRATE 0x0013

// MycoDRONE commands
#define CMD_DRONE_START_MISSION    0x0020
#define CMD_DRONE_STOP_MISSION     0x0021
#define CMD_DRONE_RTL              0x0022
#define CMD_DRONE_LAND             0x0023
#define CMD_DRONE_DEPLOY_PAYLOAD   0x0024
#define CMD_DRONE_RETRIEVE_PAYLOAD 0x0025
#define CMD_DRONE_LATCH_PAYLOAD    0x0026
#define CMD_DRONE_RELEASE_PAYLOAD  0x0027
#define CMD_DRONE_GOTO_WAYPOINT    0x0028
#define CMD_DRONE_SET_HOME         0x0029
#define CMD_DRONE_DATA_MULE_START  0x002A
#define CMD_DRONE_DATA_MULE_SYNC   0x002B

#ifdef __cplusplus
}
#endif

#endif