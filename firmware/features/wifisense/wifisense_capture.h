#ifndef WIFISENSE_CAPTURE_H
#define WIFISENSE_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/mdp_wifisense_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize WiFi Sense capture
bool wifisense_init(void);

// Start CSI capture
bool wifisense_start(const wifisense_config_t* config);

// Stop CSI capture
bool wifisense_stop(void);

// Check if capture is active
bool wifisense_is_active(void);

// Get latest CSI data
bool wifisense_get_telemetry(wifisense_telemetry_v1_t* telemetry);

// Calibration routine
bool wifisense_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif

