# WiFi Sense Capability Specification

## Overview

WiFi Sense is a new MycoBrain capability that enables **radio-based presence, motion, and pose sensing** using **WiFi Channel State Information (CSI)**. This turns MycoBrain nodes into distributed RF probe mesh sensors that work without cameras.

---

## Capability Phases

### Phase 0: Occupancy + Motion (Fast, Reliable)
**Target**: 3-6 months

- **Occupancy Detection**: "Someone is in zone A/B/C"
- **Motion Intensity**: Coarse motion detection
- **Dwell Time**: Time spent in zones
- **Activity Classes**: Still/walking classification

**Requirements:**
- Multiple MycoBrain nodes (3-6 minimum)
- RSSI + basic CSI data
- Simple ML model (occupancy + motion)
- NatureOS dashboard integration

### Phase 1: Body Silhouette / Coarse Keypoints (Moderate)
**Target**: 6-12 months

- **Body Silhouette**: Coarse body shape detection
- **Coarse Keypoints**: Basic body part localization
- **Multi-link CSI**: Multiple CSI links required
- **Calibration**: Device placement and calibration

**Requirements:**
- Multiple CSI-capable radios
- Multi-antenna support
- Calibration tools
- Enhanced ML models

### Phase 2: Dense Pose (Hard, Research-Grade)
**Target**: 12-24 months

- **Dense Pose**: Full-body tracking with UV coordinates
- **Multi-view Fusion**: Multiple viewpoints required
- **Advanced ML**: Deep learning models
- **Research Integration**: Academic collaboration

**Requirements:**
- Multi-antenna, stable CSI extraction
- Multiple viewpoints
- Training/evaluation loop
- Research-grade hardware

---

## Architecture Integration

### 1. Sensing Layer (MycoBrain Nodes)

**Option A (Recommended): MycoBrain as Control + Gateway; External CSI Sensor**

- MycoBrain provisions CSI sensor
- Timestamps and signs data
- Forwards to edge compute (WiFi/Ethernet) and/or upstream (Azure)

**Option B (R&D): ESP32-S3 Native CSI Capture**

- ESP32-S3 can expose CSI-like data in ESP-IDF
- Limited bandwidth/subcarriers compared to research-grade setups
- Useful for Phase 0/1 features
- Build MycoBrain-native dataset

### 2. Data Packet Format (MDP Extension)

**New MDP Message Type**: `MDP_WIFISENSE = 0x07`

**WiFi Sense Telemetry Structure:**
```c
#pragma pack(push,1)
struct WiFiSenseTelemetryV1 {
  mdp_hdr_v1_t hdr;          // Standard MDP header
  
  uint32_t timestamp_ns;      // Nanosecond timestamp
  uint8_t  link_id;            // TX/RX pair identifier
  uint8_t  channel;            // WiFi channel (1-14 for 2.4GHz)
  uint8_t  bandwidth;          // 20/40/80 MHz
  int8_t   rssi;               // Received signal strength
  
  // CSI Data
  uint16_t csi_length;         // Number of CSI samples
  uint8_t  csi_format;         // 0=I/Q, 1=amplitude+phase
  uint16_t num_subcarriers;    // Number of subcarriers
  uint8_t  num_antennas;       // MIMO antenna count
  
  // CSI Raw Data (variable length)
  // Format: I/Q pairs or amplitude+phase pairs
  int16_t  csi_data[];         // I/Q: [I0, Q0, I1, Q1, ...]
                               // A/P: [A0, P0, A1, P1, ...]
  
  // Optional: IMU/Compass for node orientation
  float    imu_accel[3];       // Accelerometer (m/s²)
  float    imu_gyro[3];         // Gyroscope (rad/s)
  float    compass[3];         // Magnetometer (µT)
  
  // Optional: Environmental context
  float    temp_c;             // Temperature (°C)
  float    humidity_rh;        // Relative humidity (%)
} WiFiSenseTelemetryV1;
#pragma pack(pop)
```

**New MDP Commands:**
```c
#define CMD_WIFISENSE_START    0x0010  // Start CSI capture
#define CMD_WIFISENSE_STOP     0x0011  // Stop CSI capture
#define CMD_WIFISENSE_CONFIG   0x0012  // Configure CSI params
#define CMD_WIFISENSE_CALIBRATE 0x0013 // Calibration routine
```

### 3. Edge Compute Layer (Local Server / Proxmox)

**WiFiSense Service** (Docker container):

**Components:**
- **CSI Processor**: Phase sanitization, calibration
- **Feature Extractor**: Filtering, feature extraction
- **Inference Engine**: Occupancy/activity/pose models
- **Tracker**: Multi-target tracking
- **API/Streaming**: WebSocket + REST API

**Data Flow:**
```
MycoBrain Nodes → WiFiSense Service (UDP/gRPC/WebSocket)
  → Phase Sanitization
  → Feature Extraction
  → Inference (occupancy/activity/pose)
  → Output: presence_events, tracks, pose
```

**API Endpoints:**
- `POST /wifisense/ingest` - Ingest CSI stream
- `GET /wifisense/status` - Service status
- `GET /wifisense/zones` - Zone definitions
- `GET /wifisense/events` - Presence events
- `GET /wifisense/tracks` - Active tracks
- `WS /wifisense/stream` - WebSocket stream

### 4. MINDEX Integration

**New Database Tables:**

```sql
-- WiFi Sense device configuration
CREATE TABLE telemetry.wifisense_device (
  device_id UUID PRIMARY KEY REFERENCES telemetry.device(id),
  link_id VARCHAR(32) NOT NULL,
  channel SMALLINT,
  bandwidth SMALLINT,
  csi_format SMALLINT,
  num_antennas SMALLINT,
  num_subcarriers SMALLINT,
  calibration_data JSONB,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- CSI raw data (time-series)
CREATE TABLE telemetry.wifisense_csi (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES telemetry.device(id),
  link_id VARCHAR(32),
  timestamp_ns BIGINT NOT NULL,
  channel SMALLINT,
  rssi SMALLINT,
  csi_data BYTEA,  -- Compressed CSI samples
  csi_length INTEGER,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Presence events (derived)
CREATE TABLE telemetry.wifisense_presence (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  zone_id VARCHAR(64),
  timestamp TIMESTAMPTZ NOT NULL,
  presence_type VARCHAR(32),  -- 'occupancy', 'motion', 'activity'
  confidence FLOAT,
  metadata JSONB,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tracks (multi-target)
CREATE TABLE telemetry.wifisense_track (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  track_id VARCHAR(64) NOT NULL,
  zone_id VARCHAR(64),
  position POINT,  -- PostGIS
  velocity FLOAT,
  activity_class VARCHAR(32),
  confidence FLOAT,
  first_seen TIMESTAMPTZ,
  last_seen TIMESTAMPTZ,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Pose data (Phase 2)
CREATE TABLE telemetry.wifisense_pose (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  track_id VARCHAR(64),
  timestamp TIMESTAMPTZ NOT NULL,
  keypoints JSONB,  -- {body_part: {x, y, z, confidence}}
  dense_uv JSONB,    -- DensePose UV coordinates
  confidence FLOAT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes
CREATE INDEX idx_wifisense_csi_device_time ON telemetry.wifisense_csi(device_id, timestamp_ns DESC);
CREATE INDEX idx_wifisense_presence_zone_time ON telemetry.wifisense_presence(zone_id, timestamp DESC);
CREATE INDEX idx_wifisense_track_zone ON telemetry.wifisense_track(zone_id, last_seen DESC);
```

**MINDEX API Extensions:**

```python
# mindex_api/routers/wifisense.py
@router.post("/wifisense/ingest")
async def ingest_csi(packet: WiFiSenseTelemetryV1):
    # Store raw CSI data
    # Trigger feature extraction
    # Return ingestion confirmation

@router.get("/wifisense/zones")
async def get_zones():
    # Return zone definitions

@router.get("/wifisense/events")
async def get_events(zone_id: str = None, since: datetime = None):
    # Return presence events

@router.get("/wifisense/tracks")
async def get_tracks(zone_id: str = None, active: bool = True):
    # Return active tracks
```

### 5. NatureOS Integration

**New Dashboard Widget**: `WiFiSenseWidget`

**Features:**
- Floorplan/zone view
- Occupancy heat map
- Active tracks visualization
- Timeline + alerts
- (Phase 2) Stick figure / dense overlay

**API Routes:**
```typescript
// app/api/wifisense/status/route.ts
export async function GET() {
  const response = await fetch(
    `${process.env.WIFISENSE_SERVICE_URL}/wifisense/status`
  );
  return Response.json(await response.json());
}

// app/api/wifisense/events/route.ts
export async function GET(request: Request) {
  const { searchParams } = new URL(request.url);
  const zoneId = searchParams.get('zone_id');
  const response = await fetch(
    `${process.env.WIFISENSE_SERVICE_URL}/wifisense/events?zone_id=${zoneId}`
  );
  return Response.json(await response.json());
}
```

**UI Components:**
- `components/wifisense/wifisense-zone-map.tsx` - Floorplan visualization
- `components/wifisense/wifisense-heatmap.tsx` - Occupancy heat map
- `components/wifisense/wifisense-tracks.tsx` - Track visualization
- `components/wifisense/wifisense-timeline.tsx` - Event timeline

### 6. MAS Integration

**New Agent**: `WiFiSenseAnalysisAgent`

**Capabilities:**
- Pattern recognition in presence data
- Anomaly detection
- Activity classification
- Predictive analytics

**Integration:**
```python
# mycosoft_mas/agents/wifisense_agent.py
class WiFiSenseAnalysisAgent:
    async def analyze_presence_patterns(self, zone_id: str, time_range: tuple):
        # Query MINDEX for presence events
        # Analyze patterns
        # Return insights
        
    async def detect_anomalies(self, zone_id: str):
        # Detect unusual patterns
        # Generate alerts
```

---

## Implementation Roadmap

### Phase 0 Implementation (3-6 months)

**Month 1-2: Hardware & Firmware**
- [ ] ESP32-S3 CSI capture implementation
- [ ] MDP WiFi Sense telemetry extension
- [ ] Basic CSI data collection
- [ ] Gateway forwarding to edge compute

**Month 3-4: Edge Compute & Processing**
- [ ] WiFiSense service Docker container
- [ ] CSI data ingestion
- [ ] Basic feature extraction
- [ ] Simple occupancy/motion model

**Month 5-6: Integration & Testing**
- [ ] MINDEX schema + API
- [ ] NatureOS dashboard widget
- [ ] End-to-end testing
- [ ] Documentation

### Phase 1 Implementation (6-12 months)

- [ ] Multi-link CSI fusion
- [ ] Calibration tools
- [ ] Enhanced ML models
- [ ] Body silhouette detection
- [ ] Coarse keypoint estimation

### Phase 2 Implementation (12-24 months)

- [ ] Dense pose models
- [ ] Multi-view fusion
- [ ] Research-grade hardware integration
- [ ] Advanced visualization

---

## Security & Privacy

### Security Requirements

- **Per-node Keys**: Device authentication
- **Signed Packets**: Cryptographic signing
- **Opt-in**: Explicit user consent
- **Audit Logging**: All access logged
- **Data Encryption**: TLS for transport, encryption at rest

### Privacy Considerations

- **No Visual Data**: Privacy-preserving (no cameras)
- **Anonymization**: Track IDs, not identities
- **Data Retention**: Configurable retention policies
- **User Control**: Opt-out capabilities

---

## Testing Strategy

### Unit Tests
- CSI data parsing
- Feature extraction
- Model inference

### Integration Tests
- MycoBrain → WiFiSense Service
- WiFiSense Service → MINDEX
- MINDEX → NatureOS

### Field Tests
- Multi-node deployment
- Real-world scenarios
- Performance benchmarking

---

## Documentation Requirements

- [ ] Hardware setup guide
- [ ] Firmware development guide
- [ ] API documentation
- [ ] Dashboard user guide
- [ ] Privacy policy
- [ ] Security best practices

---

## Success Metrics

### Phase 0
- Occupancy detection accuracy > 90%
- Motion detection latency < 1 second
- Support for 3-6 nodes per zone

### Phase 1
- Body silhouette detection accuracy > 80%
- Coarse keypoint accuracy > 70%

### Phase 2
- Dense pose accuracy comparable to camera-based systems
- Real-time processing (< 100ms latency)

---

## References

- [wifi-densepose GitHub](https://github.com/ruvnet/wifi-densepose)
- [DensePose From WiFi (arXiv)](https://arxiv.org/pdf/2301.00250)
- ESP-IDF WiFi CSI documentation
- ESP32-S3 datasheet

---

**Document Version**: 1.0  
**Status**: Draft  
**Last Updated**: 2025-01-XX

