#ifndef MDP_WIFISENSE_TYPES_H
#define MDP_WIFISENSE_TYPES_H

#include <stdint.h>
#include "mdp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi Sense CSI Data Format
#define WIFISENSE_FORMAT_IQ        0  // I/Q pairs
#define WIFISENSE_FORMAT_AMPLITUDE_PHASE 1  // Amplitude + Phase

#pragma pack(push,1)
typedef struct wifisense_telemetry_v1_t {
  mdp_hdr_v1_t hdr;          // Standard MDP header (msg_type = MDP_WIFISENSE)
  
  uint32_t timestamp_ns;      // Nanosecond timestamp
  uint8_t  link_id;          // TX/RX pair identifier
  uint8_t  channel;          // WiFi channel (1-14 for 2.4GHz)
  uint8_t  bandwidth;        // 20/40/80 MHz
  int8_t   rssi;             // Received signal strength (dBm)
  
  // CSI Data
  uint16_t csi_length;       // Number of CSI samples
  uint8_t  csi_format;       // 0=I/Q, 1=amplitude+phase
  uint16_t num_subcarriers;  // Number of subcarriers
  uint8_t  num_antennas;     // MIMO antenna count
  
  // CSI Raw Data (variable length, max 512 bytes)
  // Format: I/Q: [I0, Q0, I1, Q1, ...] (int16_t pairs)
  //         A/P: [A0, P0, A1, P1, ...] (uint16_t amplitude, int16_t phase)
  uint8_t  csi_data[512];    // Variable length CSI data
  
  // Optional: IMU/Compass for node orientation
  float    imu_accel[3];     // Accelerometer (m/s²)
  float    imu_gyro[3];      // Gyroscope (rad/s)
  float    compass[3];       // Magnetometer (µT)
  
  // Optional: Environmental context
  float    temp_c;           // Temperature (°C)
  float    humidity_rh;      // Relative humidity (%)
} wifisense_telemetry_v1_t;

// WiFi Sense Configuration
typedef struct wifisense_config_t {
  uint8_t  channel;          // WiFi channel
  uint8_t  bandwidth;       // 20/40/80 MHz
  uint8_t  csi_format;      // 0=I/Q, 1=amplitude+phase
  uint16_t sample_rate_hz;  // Sampling rate (Hz)
  uint8_t  enabled;         // 0=disabled, 1=enabled
} wifisense_config_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif

