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

#ifdef __cplusplus
}
#endif

#endif