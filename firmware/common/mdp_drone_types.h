#ifndef MDP_DRONE_TYPES_H
#define MDP_DRONE_TYPES_H

#include <stdint.h>
#include "mdp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Flight Modes (MAVLink compatible)
#define DRONE_MODE_MANUAL       0
#define DRONE_MODE_STABILIZE    1
#define DRONE_MODE_LOITER       2
#define DRONE_MODE_AUTO         3
#define DRONE_MODE_RTL          4
#define DRONE_MODE_LAND         5

// Mission States
#define DRONE_MISSION_IDLE      0
#define DRONE_MISSION_DEPLOY    1
#define DRONE_MISSION_RETRIEVE  2
#define DRONE_MISSION_DATA_MULE 3

// Payload Types
#define DRONE_PAYLOAD_NONE      0
#define DRONE_PAYLOAD_MUSHROOM1 1
#define DRONE_PAYLOAD_SPOREBASE 2

#pragma pack(push,1)
typedef struct drone_telemetry_v1_t {
  mdp_hdr_v1_t hdr;          // Standard MDP header (msg_type = MDP_DRONE_TELEMETRY)
  
  // Flight Controller Telemetry (MAVLink bridge)
  float    latitude;         // GPS latitude (degrees)
  float    longitude;        // GPS longitude (degrees)
  float    altitude_msl;     // Altitude MSL (meters)
  float    altitude_rel;     // Altitude relative to home (meters)
  float    heading;          // Heading (degrees)
  float    ground_speed;     // Ground speed (m/s)
  float    air_speed;        // Air speed (m/s)
  float    climb_rate;       // Climb rate (m/s)
  
  // Flight Status
  uint8_t  flight_mode;      // MAVLink flight mode
  uint8_t  arm_status;        // 0=disarmed, 1=armed
  uint8_t  battery_percent;  // Battery percentage (0-100)
  float    battery_voltage;   // Battery voltage (V)
  float    battery_current;   // Battery current (A)
  
  // Payload Status
  uint8_t  payload_latched;  // 0=not latched, 1=latched
  uint8_t  payload_type;     // 0=none, 1=Mushroom1, 2=SporeBase
  float    payload_mass;      // Payload mass (kg)
  
  // Environmental (BME688)
  float    temp_c;           // Temperature (°C)
  float    humidity_rh;      // Relative humidity (%)
  float    pressure_hpa;     // Pressure (hPa)
  float    gas_resistance;   // Gas resistance (Ohm)
  
  // Mission Status
  uint8_t  mission_state;    // 0=idle, 1=deploy, 2=retrieve, 3=data_mule
  uint32_t mission_progress; // Mission progress (0-100)
  char     mission_target[32]; // Target device ID or waypoint
} drone_telemetry_v1_t;

// Drone Mission Status
typedef struct drone_mission_status_v1_t {
  mdp_hdr_v1_t hdr;          // Standard MDP header (msg_type = MDP_DRONE_MISSION_STATUS)
  
  uint8_t  mission_state;    // Current mission state
  uint32_t mission_id;       // Mission ID
  uint32_t progress;         // Progress (0-100)
  uint8_t  status;           // 0=pending, 1=in_progress, 2=completed, 3=failed
  char     error_message[64]; // Error message if failed
} drone_mission_status_v1_t;

// Waypoint Command Data
typedef struct drone_waypoint_t {
  float latitude;
  float longitude;
  float altitude;
} drone_waypoint_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif

