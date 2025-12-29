# MycoDRONE Capability Specification

## Overview

MycoDRONE is a rugged, in-house quadcopter purpose-built to:
1. **Deploy** Mushroom 1 Nodes and SporeBase units from a powered dock into the field
2. **Recover** devices when they're low-power, damaged, or flagged by NatureOS/MINDEX
3. Act as an **airborne comms + data-mule node** (LoRa/Wi‑Fi/BT) for the ENVINT network
4. Provide **AI-assisted aerial sensing** (camera + mic) and **airborne environmental sampling** (BME688)

---

## System Architecture

### Separation of Concerns

**A) Flight Controller (Autopilot)**
- Handles: stabilization, motor control, waypoint navigation, RTL/land, safety failsafes
- **Recommended**: Pixhawk 6C, Cube Orange+, or similar
- **Protocol**: MAVLink 2.0

**B) MycoBrain = Mission Computer + Comms Hub**
- Handles: LoRa/Wi‑Fi/Bluetooth networking, device discovery, data-muling, payload actuation logic, environmental sensors, bridge to NatureOS/MINDEX
- **Hardware**: MycoBrain V1 (ESP32-S3 + SX1262)
- **Interface**: UART to flight controller (MAVLink bridge)

### Hardware Integration

**MycoBrain V1 Capabilities:**
- ESP32-S3-WROOM-1U-N16R8
- CORE1262-868M LoRa module (SX1262-class)
- Multiple I2C headers (for BME688)
- UART pins for flight controller communication

**Flight Controller Interface:**
- UART connection: MycoBrain ↔ Flight Controller
- MAVLink protocol for telemetry + commands
- MycoBrain acts as MAVLink-to-MDP bridge

---

## Key Missions

### Mission A: Deploy Mushroom 1 Node

**Steps:**
1. Drone undocks, verifies payload latched
2. Flies to GPS waypoint (or area)
3. Runs precision landing routine (v1: GPS; v2: fiducial/RTK/UWB assist)
4. Lands, releases payload, confirms release
5. Takes off, returns to dock

**MDP Commands:**
```c
#define CMD_DRONE_DEPLOY_PAYLOAD  0x0020  // Deploy payload at current location
#define CMD_DRONE_VERIFY_PAYLOAD  0x0021  // Verify payload status
#define CMD_DRONE_RELEASE_PAYLOAD 0x0022  // Release payload mechanism
```

### Mission B: Retrieve Mushroom 1 Node / SporeBase

**Trigger Conditions:**
- Device reports low battery / health fault / damage
- Heartbeat stops
- NatureOS flags a retrieval job

**Steps:**
1. Fly to last known GPS
2. Acquire target via:
   - LoRa beacon + RSSI gradient
   - Optional UWB close-in ranging (v2)
   - Vision marker for final alignment
3. Land near device, latch/hoist, confirm secure, return

**MDP Commands:**
```c
#define CMD_DRONE_RETRIEVE_PAYLOAD 0x0023  // Retrieve payload at location
#define CMD_DRONE_LATCH_PAYLOAD    0x0024  // Latch onto payload
#define CMD_DRONE_HOIST_PAYLOAD    0x0025  // Hoist payload
```

### Mission C: Data Mule / Airborne Relay

**Capabilities:**
- Fly within LoRa range of nodes
- Perform store-and-forward
- Loiter to extend comms (airborne "tower")
- Use Wi‑Fi when close for high-rate transfer (logs, media, firmware)

**Protocol:**
1. **LoRa HELLO**: drone → device: `drone_id + timestamp`
2. **LoRa STATUS**: device → drone: `battery, storage, health flags`
3. **LoRa SYNC command**: drone requests high-rate mode
4. **Wi‑Fi/BLE session**: transfer logs/media/config; checksum + signing
5. **LoRa DONE**: device returns to low power; drone reports to NatureOS/MINDEX

---

## Mechanical Design

### Airframe Specifications

- **Wheelbase**: 550-700 mm (carbon frame, foldable arms recommended)
- **Landing Gear**: High-clearance (payload can sit below frame)
- **Electronics Bay**: Sealed + vibration isolation for FC + cameras
- **Antenna Mounts**: LoRa + GNSS kept away from power wiring

### Propulsion Sizing

**Rule**: At max takeoff weight (AUW), design for **T/W ≥ 2.0** (total max thrust at least 2× weight)

**Payload Capacity**: **2.0 kg** (Mushroom 1 Node = 1.2 kg baseline)

### Payload Interface: "MicoLatch" Standard

**Mechanical Standard:**
- **One top-centered lifting point** (ring/handle) rated for 3× payload mass
- **Self-centering geometry** (tapered guide or rails)
- **Positive lock** (mechanical latch) + sensor confirmation (reed/hall switch)

**Grab Methods:**

**V1 (Fastest, Most Reliable)**: Land-to-grab + land-to-place
- Servo gripper or latch engages the ring/handle
- Drone lands next to/over device, secures, then takes off

**V2 (Forest/Brush/Trees)**: Winch-enabled
- Small winch lowers hook/latch without fully landing
- Enables SporeBase placement "hung" from branch or structure

### Device Modifications

**Add to every field device:**
- **Fiducial marker** (AprilTag/ArUco) on top face
- **Optional IR/LED beacon**
- **"Recovery mode"**: When pinged, beacons more frequently and flashes

---

## Sensing Payload

### Cameras

**Minimum Viable:**
- **Down-facing camera**: Precision landing, tag detection, pickup alignment
- **Forward camera**: Situational awareness (optional gimbal later)

### Microphone

**Purpose:**
- Detect nearby machinery/vehicles/voices at low altitude hover
- Capture "presence audio" for ops logs

**Mounting**: Away from prop wash, digital mic + noise-robust processing

### BME688 Airborne Sampling

**Integration:**
- Mount BME688 in **shielded, aspirated pocket** (small duct) to reduce propwash bias
- Connect via MycoBrain I2C (MycoBrain exposes SDA/SCL on multiple headers)

---

## Communication Stack

### LoRa + Wi‑Fi + Bluetooth

**Data Mule Protocol (Two-Stage):**

1. **LoRa Discovery + Control**
   - HELLO: drone → device
   - STATUS: device → drone
   - SYNC command: drone requests high-rate mode

2. **Wi‑Fi/BLE Bulk Transfer**
   - Transfer logs/media/config
   - Checksum + signing
   - Integrity verification

3. **LoRa Completion**
   - DONE: device returns to low power
   - Drone reports to NatureOS/MINDEX

### Swarm / Hive Networking

**Near-range drone-to-drone**: Wi‑Fi mesh (higher bandwidth)
**Long-range coordination / beacons**: LoRa
**Close-in relative positioning (optional)**: UWB upgrade path

**Swarm Behaviors:**
- Task allocation (which drone retrieves which device)
- Deconfliction (altitude separation + time slots)
- Relay formation (one loiters as comms tower, others work)

---

## Autonomy Roadmap

### Phase 1: Open-Field Autonomous Deploy (Fastest to First Test)

- GPS waypoint missions
- Conservative landing + release
- Manual override always available

### Phase 2: Precision Landing + Recovery Under Canopy Edges

- Fiducial landing to improve placement/retrieval
- Optional RTK on the dock and/or drone
- Optional UWB tag on devices for last-meter approach

### Phase 3: Forest-Capable Navigation

- Obstacle avoidance: depth camera or lidar
- Visual-inertial odometry (VIO) or optical flow for GPS-denied segments

---

## Docking Station (MycoDock)

**Requirements:**
- Landing pad with **fiducial** for auto-dock
- **Mechanical retention** (lock down for wind + charging)
- Charging bays for Mushroom 1 Node + SporeBase (powered staging)
- Data backhaul (Ethernet/Wi‑Fi) to upload logs + node telemetry

---

## MDP Protocol Extensions

### New Message Types

```c
// Drone-specific telemetry
#define MDP_DRONE_TELEMETRY 0x08

// Drone mission status
#define MDP_DRONE_MISSION_STATUS 0x09
```

### New Telemetry Structure

```c
#pragma pack(push,1)
struct DroneTelemetryV1 {
  mdp_hdr_v1_t hdr;          // Standard MDP header
  
  // Flight Controller Telemetry (MAVLink bridge)
  float    latitude;          // GPS latitude (degrees)
  float    longitude;         // GPS longitude (degrees)
  float    altitude_msl;      // Altitude MSL (meters)
  float    altitude_rel;     // Altitude relative to home (meters)
  float    heading;          // Heading (degrees)
  float    ground_speed;     // Ground speed (m/s)
  float    air_speed;        // Air speed (m/s)
  float    climb_rate;       // Climb rate (m/s)
  
  // Flight Status
  uint8_t  flight_mode;      // MAVLink flight mode
  uint8_t  arm_status;        // Armed/disarmed
  uint8_t  battery_percent;   // Battery percentage
  float    battery_voltage;   // Battery voltage (V)
  float    battery_current;  // Battery current (A)
  
  // Payload Status
  uint8_t  payload_latched;  // Payload latch status
  uint8_t  payload_type;     // 0=none, 1=Mushroom1, 2=SporeBase
  float    payload_mass;      // Payload mass (kg)
  
  // Environmental (BME688)
  float    temp_c;           // Temperature (°C)
  float    humidity_rh;      // Relative humidity (%)
  float    pressure_hpa;     // Pressure (hPa)
  float    gas_resistance;   // Gas resistance (Ohm)
  
  // Mission Status
  uint8_t  mission_state;    // 0=idle, 1=deploy, 2=retrieve, 3=data_mule
  uint32_t mission_progress; // Mission progress (%)
  char     mission_target[32]; // Target device ID or waypoint
} DroneTelemetryV1;
#pragma pack(pop)
```

### New Commands

```c
// Mission Control
#define CMD_DRONE_START_MISSION    0x0020  // Start mission (deploy/retrieve/data_mule)
#define CMD_DRONE_STOP_MISSION     0x0021  // Stop current mission
#define CMD_DRONE_RTL              0x0022  // Return to launch
#define CMD_DRONE_LAND             0x0023  // Land at current location

// Payload Control
#define CMD_DRONE_DEPLOY_PAYLOAD   0x0024  // Deploy payload
#define CMD_DRONE_RETRIEVE_PAYLOAD 0x0025  // Retrieve payload
#define CMD_DRONE_LATCH_PAYLOAD    0x0026  // Latch onto payload
#define CMD_DRONE_RELEASE_PAYLOAD  0x0027  // Release payload

// Waypoint Navigation
#define CMD_DRONE_GOTO_WAYPOINT    0x0028  // Fly to waypoint (lat, lon, alt)
#define CMD_DRONE_SET_HOME         0x0029  // Set home position

// Data Mule
#define CMD_DRONE_DATA_MULE_START  0x002A  // Start data mule mission
#define CMD_DRONE_DATA_MULE_SYNC   0x002B  // Sync with device
```

---

## MINDEX Integration

### New Database Tables

```sql
-- Drone registry
CREATE TABLE telemetry.drone (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES telemetry.device(id),
  drone_type VARCHAR(32),  -- 'mycodrone_v1'
  max_payload_kg FLOAT,
  max_range_km FLOAT,
  home_latitude FLOAT,
  home_longitude FLOAT,
  dock_id UUID,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Drone missions
CREATE TABLE telemetry.drone_mission (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  drone_id UUID REFERENCES telemetry.drone(id),
  mission_type VARCHAR(32),  -- 'deploy', 'retrieve', 'data_mule'
  target_device_id UUID REFERENCES telemetry.device(id),
  waypoint_lat FLOAT,
  waypoint_lon FLOAT,
  waypoint_alt FLOAT,
  status VARCHAR(32),  -- 'pending', 'in_progress', 'completed', 'failed'
  progress INTEGER,  -- 0-100
  started_at TIMESTAMPTZ,
  completed_at TIMESTAMPTZ,
  error_message TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Drone telemetry log
CREATE TABLE telemetry.drone_telemetry_log (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  drone_id UUID REFERENCES telemetry.drone(id),
  timestamp TIMESTAMPTZ NOT NULL,
  latitude FLOAT,
  longitude FLOAT,
  altitude_msl FLOAT,
  altitude_rel FLOAT,
  heading FLOAT,
  ground_speed FLOAT,
  battery_percent SMALLINT,
  battery_voltage FLOAT,
  flight_mode VARCHAR(32),
  mission_state VARCHAR(32),
  payload_latched BOOLEAN,
  payload_type VARCHAR(32),
  temp_c FLOAT,
  humidity_rh FLOAT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Docking stations
CREATE TABLE telemetry.dock (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  name VARCHAR(64),
  latitude FLOAT,
  longitude FLOAT,
  altitude FLOAT,
  fiducial_id VARCHAR(32),
  charging_bays INTEGER,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes
CREATE INDEX idx_drone_mission_status ON telemetry.drone_mission(status, created_at DESC);
CREATE INDEX idx_drone_telemetry_time ON telemetry.drone_telemetry_log(drone_id, timestamp DESC);
```

### MINDEX API Extensions

```python
# mindex_api/routers/drone.py
@router.post("/drone/missions")
async def create_mission(mission: DroneMissionCreate):
    # Create mission record
    # Queue command to drone
    # Return mission ID

@router.get("/drone/missions/{mission_id}")
async def get_mission(mission_id: UUID):
    # Return mission status

@router.get("/drone/{drone_id}/telemetry/latest")
async def get_latest_telemetry(drone_id: UUID):
    # Return latest drone telemetry

@router.post("/drone/{drone_id}/commands")
async def send_command(drone_id: UUID, command: DroneCommand):
    # Queue command to drone
```

---

## NatureOS Integration

### New Dashboard Widget: `DroneControlWidget`

**Features:**
- Mission planning interface
- Real-time drone status
- Map view with drone position
- Mission history
- Payload management

### API Routes

```typescript
// app/api/drone/missions/route.ts
export async function POST(request: Request) {
  const mission = await request.json();
  const response = await fetch(
    `${process.env.MINDEX_API_BASE_URL}/drone/missions`,
    {
      method: 'POST',
      headers: {
        'X-API-Key': process.env.MINDEX_API_KEY!,
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(mission)
    }
  );
  return Response.json(await response.json());
}

// app/api/drone/{drone_id}/status/route.ts
export async function GET(
  request: Request,
  { params }: { params: { drone_id: string } }
) {
  const response = await fetch(
    `${process.env.MINDEX_API_BASE_URL}/drone/${params.drone_id}/telemetry/latest`,
    {
      headers: {
        'X-API-Key': process.env.MINDEX_API_KEY!
      }
    }
  );
  return Response.json(await response.json());
}
```

### UI Components

- `components/drone/drone-control-panel.tsx` - Mission control interface
- `components/drone/drone-map-view.tsx` - Map with drone position
- `components/drone/drone-mission-list.tsx` - Mission history
- `components/drone/drone-payload-manager.tsx` - Payload management

---

## MAS Integration

### New Agent: `DroneMissionPlannerAgent`

**Capabilities:**
- Autonomous mission planning
- Task allocation (multi-drone)
- Route optimization
- Risk assessment

**Integration:**
```python
# mycosoft_mas/agents/drone_agent.py
class DroneMissionPlannerAgent:
    async def plan_deployment(self, target_location: tuple, payload_type: str):
        # Plan deployment mission
        # Optimize route
        # Return mission plan
        
    async def plan_retrieval(self, device_id: str):
        # Plan retrieval mission
        # Account for device location
        # Return mission plan
        
    async def allocate_tasks(self, missions: list):
        # Allocate missions to available drones
        # Optimize for efficiency
        # Return allocation
```

---

## Build + Test Plan

### Bench Testing

1. Power system validation (propulsion vs avionics rails)
2. Flight controller bring-up + calibration
3. MycoBrain bring-up: LoRa link + Wi‑Fi + BLE
4. MAVLink bridge: MycoBrain ↔ flight controller over UART

### Flight Testing (No Payload)

5. Manual stabilize flight
6. RTL + geofence + failsafe tests
7. Waypoint mission test in open field

### Payload Testing (Dummy Mass)

8. Lift 1.2 kg dummy payload; validate endurance + control margins
9. Validate latch sensor + emergency release

### Deploy / Retrieve Testing

10. Autonomous land + release on marked pad
11. Retrieve from pad using fiducial alignment
12. Repeat at varied terrain angles / grass height

### Data Mule Testing

13. LoRa handshake + Wi‑Fi/BLE bulk transfer
14. Integrity check + upload at dock

### Swarm Testing (Optional)

15. Two-drone relay + worker test (simple task allocation)

---

## Critical Constraints

1. **Payload Capacity**: Must support 2.0 kg (Mushroom 1 Node = 1.2 kg)
2. **T/W Ratio**: Minimum 2.0 for safe control authority
3. **LoRa Range**: Must reach devices within deployment area
4. **Battery Life**: Sufficient for mission + safety margin
5. **Weather Limits**: Wind, rain, temperature constraints

---

## Success Metrics

### Phase 1
- Successful autonomous deployment in open field
- Successful autonomous retrieval in open field
- Data mule functionality working

### Phase 2
- Precision landing with fiducial markers
- Retrieval under canopy edges
- Multi-drone coordination

### Phase 3
- Forest-capable navigation
- Obstacle avoidance
- GPS-denied operation

---

## Documentation Requirements

- [ ] Hardware assembly guide
- [ ] Flight controller setup
- [ ] MycoBrain integration guide
- [ ] Mission planning guide
- [ ] Maintenance procedures
- [ ] Safety protocols

---

**Document Version**: 1.0  
**Status**: Draft  
**Last Updated**: 2025-01-XX

