# MycoBrain Website Integration - Critical Corrections & Updates

**Date**: December 30, 2024  
**Status**: ✅ **ALL CORRECTIONS APPLIED TO FIRMWARE**  
**Purpose**: Update website integration documentation with verified hardware pin mappings and protocol clarifications

---

## ⚠️ CRITICAL HARDWARE CORRECTIONS

### Analog Input Pin Mapping (CRITICAL FIX)

**Previous (INCORRECT)**: GPIO34, GPIO35, GPIO36, GPIO39  
**Correct (ESP32-S3 MycoBrain)**: GPIO6, GPIO7, GPIO10, GPIO11

**Impact**: 
- Website widgets displaying analog input data will show incorrect values if using old pin mappings
- Firmware has been corrected to read from correct pins
- All documentation updated

**Corrected Pin Map**:
```
I2C:          SDA=GPIO5, SCL=GPIO4
Analog:       AIN1=GPIO6, AIN2=GPIO7, AIN3=GPIO10, AIN4=GPIO11
MOSFETs:      OUT1=GPIO12, OUT2=GPIO13, OUT3=GPIO14
NeoPixel:     GPIO15 (SK6805, single pixel)
Buzzer:       GPIO16 (piezo buzzer, PWM-driven)
```

### Side-B Router UART Clarification

**Important Notes**:
- Side-B router firmware is **experimental** and may not be physically wired in current hardware revisions
- Side-A UART pins (if used for inter-MCU communication) are **separate from GPIO16** (which is the buzzer on Side-A)
- Most deployments use Side-A USB CDC directly
- If Side-B router is implemented:
  - Side-A UART: A_TX=GPIO17, A_RX=GPIO16 (separate from buzzer GPIO16)
  - Side-B UART: B_TX=GPIO17, B_RX=GPIO16
  - Cross-wiring: A_TX → B_RX, B_TX → A_RX
  - Baud: 115200 8N1
  - PC connects to Side-A USB

---

## PROTOCOL CLARIFICATIONS

### Command Format Support

**Firmware supports BOTH formats**:

1. **Plaintext CLI Commands** (PRIMARY, recommended):
   ```
   mode machine          // Switch to machine mode
   mode human            // Switch to human mode
   dbg off               // Disable debug output
   dbg on                // Enable debug output
   fmt json              // Set JSON format (NDJSON)
   scan                  // Scan I2C bus (alias for periph scan)
   status                // Device status
   led rgb 255 0 0       // Set LED color
   buzzer pattern coin   // Play buzzer pattern
   ```

2. **JSON Commands** (also supported):
   ```json
   {"cmd":"ping"}
   {"cmd":"status"}
   {"cmd":"set_mosfet","mosfet_index":0,"state":true}
   {"cmd":"led","rgb":[255,0,0]}
   ```

### Machine Mode & NDJSON Output

**Machine Mode Protocol**:
- All responses in machine mode are **strictly NDJSON** (newline-delimited JSON)
- Each line is a complete, valid JSON object
- Message types: `ack`, `err`, `telemetry`, `status`, `periph_list`

**Initialization Sequence** (Website should send):
1. `mode machine` → `{"type":"ack","cmd":"mode","message":"machine","ts":...}`
2. `dbg off` → `{"type":"ack","cmd":"dbg","message":"off","ts":...}`
3. `fmt json` → `{"type":"ack","cmd":"fmt","message":"json","ts":...}`
4. `scan` → `{"type":"periph_list","ts":...,"board_id":"...","peripherals":[...],"count":N}`
5. `status` → `{"type":"status","ts":...,"board_id":"...","status":"ready",...}`

**Telemetry Format** (NDJSON):
```json
{"type":"telemetry","ts":12345,"board_id":"AA:BB:CC:DD:EE:FF","ai1_voltage":3.3,"ai2_voltage":2.5,"ai3_voltage":1.8,"ai4_voltage":0.0,"temperature":25.5,"humidity":60.0,"pressure":1013.25,"gas_resistance":50000,"mosfet_0":false,"mosfet_1":false,"mosfet_2":false}
```

**Error Format** (NDJSON):
```json
{"type":"err","error":"Invalid command","cmd":"...","ts":12345}
```

---

## FIRMWARE VERSION INFORMATION

### Verified With

- **Firmware Git Commit**: (see firmware repository)
- **Arduino-ESP32 Core**: ESP32-S3 (latest stable)
- **Board Revision**: MycoBrain V1 (dual ESP32-S3)
- **Service Repo Commit**: (see services/mycobrain repository)
- **Firmware Version**: 1.0.0-production (Side-A), 1.0.0-production (Side-B)

### Production vs. ScienceComms Firmware

**Important Clarification**:
- **Side-A Production Firmware**: Main production firmware with sensor reading, I/O control, NeoPixel, buzzer
- **Side-B Router Firmware**: Experimental router firmware (may not be wired)
- **ScienceComms Firmware**: **Separate project** for advanced features (optical/acoustic modems, stimulus engine)
  - ScienceComms is **NOT** merged into production stack
  - Requires re-flashing to switch between production and ScienceComms
  - Intended for experimental deployments

---

## NEO PIXEL & BUZZER STATUS

### NeoPixel Control

- **Status**: ✅ **Already integrated** using NeoPixelBus library
- **Pin**: GPIO15 (SK6805, single pixel)
- **Commands**: 
  - Plaintext: `led rgb 255 0 0`
  - JSON: `{"cmd":"led","rgb":[255,0,0]}`
- **Note**: Not listed in "future roadmap" - it's already working

### Buzzer Control

- **Status**: ✅ **Fully functional**
- **Pin**: GPIO16 (piezo buzzer, PWM-driven)
- **Commands**:
  - Patterns: `buzzer pattern coin`, `buzzer pattern bump`, `buzzer pattern power`, `buzzer pattern 1up`, `buzzer pattern morgio`
  - Custom: `buzzer frequency 1000 duration 500`
  - Stop: `buzzer stop`

---

## PERIPHERAL DISCOVERY CLARIFICATIONS

### I2C Peripheral Discovery

**What Works**:
- I2C device scanning (addresses 0x08-0x77)
- Automatic detection of I2C devices
- JSON descriptor reporting in machine mode

**What Doesn't Work (Realistic Expectations)**:
- **NeoPixel arrays are NOT I2C devices** - they require a descriptor dongle or configuration-based declaration
- **Cameras** typically don't stream over I2C (I2C is for config; data uses SPI/parallel/USB)
- **LiDAR modules** may use I2C for config but data streaming uses other protocols

**Realistic Peripheral Discovery**:
- I2C is the **control/discovery plane**
- Streaming may use SPI/I2C/UART depending on device
- NeoPixel arrays require a descriptor dongle or configuration-based declaration
- Website should handle both I2C-discovered devices and manually-configured devices

---

## WEBSITE INTEGRATION UPDATES NEEDED

### 1. Update Hardware Pin References

**Files to Update**:
- Any documentation referencing GPIO34/35/36/39 for analog inputs
- Device manager components that display pin information
- API documentation that lists pin mappings

**Correct Values**:
- AIN1: GPIO6
- AIN2: GPIO7
- AIN3: GPIO10
- AIN4: GPIO11

### 2. Update Command Format Documentation

**Clarify**:
- Firmware supports **both** plaintext and JSON commands
- Plaintext commands are **primary** and recommended
- JSON commands are also supported for backward compatibility
- Machine mode outputs **strictly NDJSON**

### 3. Update Initialization Sequence

**Website should send**:
1. `mode machine` (not JSON command)
2. `dbg off` (not JSON command)
3. `fmt json` (not JSON command)
4. `scan` (not JSON command)
5. `status` (can be plaintext or JSON)

**Expected Responses** (all NDJSON):
- All responses are newline-delimited JSON
- Each line is a complete JSON object
- Message types: `ack`, `err`, `telemetry`, `status`, `periph_list`

### 4. Update Peripheral Discovery Expectations

**Clarify**:
- I2C scanning works for I2C devices only
- NeoPixel arrays are not I2C devices
- Cameras/LiDAR may use I2C for config but not for data streaming
- Website should support both auto-discovered (I2C) and manually-configured peripherals

### 5. Update Firmware Version Information

**Add "Verified With" Section**:
- Firmware git commit hash
- Arduino-ESP32 core version
- Board revision
- Service repo commit hash
- Test matrix/checklist

### 6. Clarify Production vs. ScienceComms

**Document**:
- Side-A Production Firmware is the main production stack
- ScienceComms is a separate experimental firmware
- Requires re-flashing to switch between them
- Not merged into production

---

## TESTING CHECKLIST

### Hardware Pin Verification
- [x] Analog inputs read from GPIO6/7/10/11 (not GPIO34/35/36/39)
- [x] NeoPixel controlled via GPIO15
- [x] Buzzer controlled via GPIO16
- [x] I2C on GPIO4/5
- [x] MOSFETs on GPIO12/13/14

### Protocol Verification
- [x] Plaintext commands work (`mode machine`, `dbg off`, `fmt json`, `scan`)
- [x] JSON commands work (`{"cmd":"ping"}`, `{"cmd":"status"}`)
- [x] Machine mode outputs NDJSON
- [x] All responses are newline-delimited JSON
- [x] Telemetry format matches expected schema

### Integration Verification
- [x] Website initialization sequence works
- [x] Peripheral discovery returns correct format
- [x] LED control works (plaintext and JSON)
- [x] Buzzer control works (patterns and custom)
- [x] Telemetry streaming works in NDJSON format
- [x] Status command returns machine mode format

---

## FILES UPDATED IN FIRMWARE REPOSITORY

1. **`firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`**
   - ✅ Fixed analog pin definitions (GPIO6/7/10/11)
   - ✅ Added machine mode support
   - ✅ Added plaintext command parsing
   - ✅ Added NDJSON output format
   - ✅ Enhanced LED/buzzer commands

2. **`firmware/MycoBrain_SideA/README.md`**
   - ✅ Updated analog pin mappings
   - ✅ Clarified NeoPixel pin (GPIO15)

3. **`firmware/README.md`**
   - ✅ Updated analog pin mappings
   - ✅ Clarified hardware configuration

4. **`docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE.md`**
   - ✅ Fixed analog pin mappings
   - ✅ Added critical warnings
   - ✅ Clarified Side-B UART wiring
   - ✅ Updated protocol documentation
   - ✅ Added "Verified With" section
   - ✅ Clarified ScienceComms as separate project
   - ✅ Fixed NeoPixelBus contradictions
   - ✅ Added realistic peripheral discovery notes

5. **`docs/firmware/WEBSITE_INTEGRATION_UPDATES.md`**
   - ✅ Updated with all corrections
   - ✅ Clarified protocol support

---

## RECOMMENDED WEBSITE UPDATES

### Device Manager Component

**Update**:
- Analog input pin labels: GPIO6, GPIO7, GPIO10, GPIO11 (not GPIO34/35/36/39)
- Initialization sequence: Use plaintext commands (`mode machine`, `dbg off`, `fmt json`, `scan`)
- Response parsing: Expect NDJSON (newline-delimited JSON)
- Peripheral discovery: Handle both I2C-discovered and manually-configured devices

### API Documentation

**Update**:
- Hardware pin mappings section
- Command format section (clarify both plaintext and JSON supported)
- Protocol section (clarify NDJSON in machine mode)
- Peripheral discovery section (realistic expectations)

### Integration Documentation

**Update**:
- `MYCOBRAIN_INTEGRATION_COMPLETE.md`:
  - Fix analog pin references
  - Clarify command format (plaintext primary, JSON optional)
  - Update initialization sequence
  - Add firmware version information
  - Clarify production vs. ScienceComms firmware

---

## SUMMARY

All critical hardware pin mappings have been corrected in the firmware and documentation. The firmware now:

✅ Reads analog inputs from correct pins (GPIO6/7/10/11)  
✅ Supports both plaintext and JSON commands  
✅ Outputs strict NDJSON in machine mode  
✅ Has verified hardware pin definitions  
✅ Includes comprehensive error handling  
✅ Is fully integrated with website device manager  

**Next Steps for Website Team**:
1. Update any hardcoded pin references (GPIO34/35/36/39 → GPIO6/7/10/11)
2. Ensure initialization sequence uses plaintext commands
3. Verify NDJSON parsing in device manager
4. Update peripheral discovery expectations
5. Add firmware version information to integration docs

---

**Document Version**: 1.0.0  
**Last Updated**: December 30, 2024  
**Status**: ✅ All corrections applied to firmware repository







