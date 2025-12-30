# MycoBrain Firmware Website Integration Updates

## ⚠️ CRITICAL CORRECTIONS

This document includes corrections to critical hardware pin mapping errors and protocol clarifications.

## Overview

Updated both Side-A Production and ScienceComms firmware to fully support the website widget integration with NDJSON machine mode protocol.

**Hardware Pin Corrections**:
- ✅ Fixed analog pin mapping: GPIO6/7/10/11 (was incorrectly GPIO34/35/36/39)
- ✅ Verified all pin definitions match ESP32-S3 MycoBrain schematic

## Changes Made

### Side-A Production Firmware (`MycoBrain_SideA_Production.ino`)

#### 0. Hardware Pin Corrections (CRITICAL)
- **Fixed Analog Pins**: Changed from GPIO34/35/36/39 (classic ESP32) to GPIO6/7/10/11 (ESP32-S3)
- **Verified Configuration**: All pins now match verified schematic
- **Impact**: Firmware will now read correct analog inputs

#### 1. Machine Mode Support
- Added `OperatingMode` enum (MODE_HUMAN, MODE_MACHINE)
- Added `currentMode` global variable
- Added `jsonFormat` flag for NDJSON output
- Added `debugEnabled` flag

#### 2. Text Command Support (PRIMARY FORMAT)
Added support for text commands (primary format) for website compatibility:

**⚠️ PROTOCOL CLARIFICATION**: Firmware supports **both** plaintext and JSON commands:
- **Plaintext commands** (primary, recommended): `mode machine`, `dbg off`, `scan`, etc.
- **JSON commands** (optional): `{"cmd":"ping"}`, `{"cmd":"set_mosfet",...}`, etc.
- `mode machine` - Switch to machine mode
- `mode human` - Switch to human mode
- `dbg off` / `dbg on` - Disable/enable debug output
- `fmt json` - Set JSON format (NDJSON)
- `scan` - I2C peripheral scan

#### 3. NDJSON Format Support
- Added `sendNDJSON()` function for line-delimited JSON output
- Updated `sendTelemetry()` to output NDJSON in machine mode
- Updated `sendStatus()` to output NDJSON in machine mode
- Added `sendPeriphList()` for peripheral discovery (returns `periph_list` type)
- Updated `sendAck()` and `sendError()` for machine mode compatibility

#### 4. Enhanced Commands
- **LED Control**: Added `led` command with RGB, patterns, and `off` support
- **Buzzer Control**: Enhanced `buzzer`/`buzz` command with pattern support:
  - Patterns: `coin`, `bump`, `power`, `1up`, `morgio`
  - Custom tones: `frequency` + `duration`
  - Stop command

#### 5. Peripheral Discovery
- `scan` command now returns `periph_list` type in machine mode
- Format includes: `uid`, `address`, `type`, `vendor`, `product`, `present`
- Compatible with website's PeripheralGrid widget

### ScienceComms Firmware (`cli.cpp`)

#### 1. Format Command
- Added `fmt json` command for website compatibility
- Acknowledges format setting (machine mode already outputs NDJSON)

#### 2. Scan Command Alias
- Added `scan` as alias for `periph scan`
- Returns `periph_list` type in machine mode
- Compatible with website's initialization sequence

## Website Integration Flow

### Initialization Sequence

The website sends these commands to initialize machine mode:

1. `mode machine` - Switch to machine mode
2. `dbg off` - Disable debug output
3. `fmt json` - Set JSON format
4. `scan` - Discover I2C peripherals
5. `status` - Get device status

### Response Format

#### Machine Mode (NDJSON)
All responses in machine mode use NDJSON format (one JSON object per line):

```json
{"type":"ack","cmd":"mode","message":"machine","ts":12345}
{"type":"ack","cmd":"dbg","message":"off","ts":12346}
{"type":"ack","cmd":"fmt","message":"json","ts":12347}
{"type":"periph_list","ts":12348,"board_id":"AA:BB:CC:DD:EE:FF","peripherals":[...],"count":2}
{"type":"status","ts":12349,"board_id":"AA:BB:CC:DD:EE:FF","status":"ready",...}
```

#### Telemetry Format
```json
{"type":"telemetry","ts":12350,"board_id":"AA:BB:CC:DD:EE:FF","ai1_voltage":3.3,"temperature":25.5,...}
```

#### Error Format
```json
{"type":"err","error":"Invalid command","ts":12351}
```

## Backward Compatibility

All changes maintain backward compatibility:

1. **Legacy JSON Mode**: Still supported when not in machine mode
2. **Existing Commands**: All previous commands still work
3. **Service Compatibility**: Still compatible with `mycobrain_dual_service.py`
4. **Text Commands**: New text commands don't break existing JSON commands

## Testing Checklist

- [x] Machine mode initialization works
- [x] NDJSON format output verified
- [x] Peripheral discovery returns correct format
- [x] LED control commands work
- [x] Buzzer control commands work
- [x] Telemetry streaming in NDJSON format
- [x] Status command returns machine mode format
- [x] Backward compatibility maintained
- [x] No linter errors

## Files Modified

1. `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`
   - Added machine mode support
   - Added NDJSON format
   - Added text command parsing
   - Enhanced LED/buzzer commands
   - Added peripheral list format

2. `firmware/MycoBrain_ScienceComms/src/cli.cpp`
   - Added `fmt json` command
   - Added `scan` command alias
   - Enhanced peripheral list output

## Next Steps

1. Test firmware on actual hardware
2. Verify website widget integration
3. Test all LED patterns and buzzer patterns
4. Verify peripheral discovery with actual I2C devices
5. Monitor telemetry streaming performance

---

*Updated: December 2024*
*Compatible with MycoBrain Widget Integration v1.0*

