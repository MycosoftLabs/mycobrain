# MycoBrain Firmware - Critical Fixes Summary

## ⚠️ CRITICAL ISSUES FIXED

This document summarizes all critical fixes applied to MycoBrain firmware and documentation.

---

## 1. Hardware Pin Mapping Errors (CRITICAL)

### Problem
Previous documentation incorrectly listed analog input pins as GPIO34/35/36/39 (classic ESP32 pins). These pins:
- Do not exist or are not ADC-capable on ESP32-S3
- Would cause firmware to read wrong pins or fail
- Are incompatible with MycoBrain V1 hardware

### Fix
✅ **Corrected Analog Pin Mapping**:
- AIN1: GPIO6 (was GPIO34)
- AIN2: GPIO7 (was GPIO35)
- AIN3: GPIO10 (was GPIO36)
- AIN4: GPIO11 (was GPIO39)

### Files Fixed
- `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`
- `docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE.md`
- `firmware/README.md`
- All documentation references

### Verified Configuration
```
I2C:          SDA=GPIO5, SCL=GPIO4
Analog:       AIN1=GPIO6, AIN2=GPIO7, AIN3=GPIO10, AIN4=GPIO11
MOSFETs:      OUT1=GPIO12, OUT2=GPIO13, OUT3=GPIO14
NeoPixel:     GPIO15 (SK6805, 1 pixel)
Buzzer:       GPIO16 (PWM-driven)
```

---

## 2. Side-B Router UART Pin Conflicts

### Problem
- Side-B firmware documented UART pins as GPIO16/17
- Side-A uses GPIO16 for buzzer
- No clear documentation of inter-MCU wiring
- Unclear which USB port PC connects to

### Fix
✅ **Clarified Side-B Status**:
- Side-B router firmware is **EXPERIMENTAL**
- May not be physically wired in current hardware
- Side-B uses **Side-B's GPIO16/17** (not Side-A's)
- Side-A would need **separate UART pins** (not GPIO16) for inter-MCU communication
- Most deployments use Side-A USB CDC directly

### Documentation Added
- Experimental status clearly marked
- Physical wiring requirements documented
- Current deployment reality clarified

---

## 3. Protocol/Command Format Mismatch

### Problem
- Documentation showed JSON commands: `{"cmd":"ping"}`
- Working firmware uses plaintext: `mode machine`, `scan`, etc.
- Unclear which format is primary
- Confusion about command vs response format

### Fix
✅ **Clarified Protocol**:
- **Commands**: Plaintext CLI (primary) OR JSON (optional)
- **Responses**: NDJSON in machine mode (newline-delimited JSON)
- **Message Types**: `ack`, `err`, `telemetry`, `periph_list`, `status`

### Examples
**Plaintext Commands** (primary):
```
mode machine
dbg off
fmt json
scan
led rgb 255 0 0
buzzer pattern coin
```

**JSON Commands** (also supported):
```json
{"cmd":"ping"}
{"cmd":"set_mosfet","mosfet_index":0,"state":true}
```

**NDJSON Responses** (machine mode):
```json
{"type":"ack","cmd":"mode","message":"machine","ts":12345}
{"type":"telemetry","ts":12346,"board_id":"AA:BB:CC:DD:EE:FF",...}
```

---

## 4. Missing Version/Test Matrix

### Problem
- Claims of "production-ready" without version pinning
- No test matrix or verification checklist
- No Arduino-ESP32 core version specified
- No board revision information

### Fix
✅ **Added Verification Section**:
- Firmware versions documented
- Arduino-ESP32 core version specified (3.0.x)
- Board revision noted (MycoBrain V1)
- Test matrix with verified/unverified features
- Git commit references added

---

## 5. ScienceComms vs Production Firmware Confusion

### Problem
- Three separate firmware projects listed
- Unclear if ScienceComms is merged or separate
- Deployment confusion about which to use

### Fix
✅ **Clarified Firmware Projects**:
1. **Side-A Production**: Primary sensor/control firmware
2. **Side-B Router**: Experimental router firmware (optional)
3. **ScienceComms**: Separate firmware for science experiments (replaces Side-A)

**Deployment Guide**:
- Production use: Flash Side-A Production
- Science experiments: Flash ScienceComms (replaces Side-A)
- Dual-MCU setup: Flash both Side-A Production + Side-B Router (experimental)

---

## 6. NeoPixelBus Contradiction

### Problem
- Documentation claimed NeoPixelBus was present
- Roadmap also listed NeoPixelBus as future enhancement
- Contradictory statements

### Fix
✅ **Resolved Contradiction**:
- **Current**: Basic GPIO control only (digital on/off, basic patterns)
- **Future**: NeoPixelBus library integration (roadmap item)
- Status clearly marked in all documentation

---

## 7. Unrealistic Peripheral Discovery Claims

### Problem
- Documentation implied all peripherals auto-discoverable via I2C
- NeoPixel arrays, cameras, microphones listed as I2C devices
- Unrealistic expectations

### Fix
✅ **Added Reality Check**:
- I2C is **control/discovery plane** only
- Streaming data uses SPI/I2S/UART
- NeoPixel arrays are **NOT I2C** (one-wire timing)
- Cameras use I2C for config, SPI/parallel/USB for data
- Microphones use I2S, not I2C
- NeoPixel arrays require descriptor dongle or configuration-based declaration

---

## 8. File Path References

### Problem
- Documentation contains specific file paths that may change
- Internal references that may not match repository structure
- Aging documentation risk

### Fix
✅ **Improved References**:
- Module names instead of exact paths where possible
- Version-controlled file references
- Clear indication of experimental vs production code
- Links to integration API docs

---

## Summary of All Fixes

| Issue | Severity | Status | Files Updated |
|-------|----------|--------|---------------|
| Analog pin mapping | CRITICAL | ✅ Fixed | Firmware + All docs |
| Side-B UART conflicts | CRITICAL | ✅ Clarified | All docs |
| Protocol mismatch | CRITICAL | ✅ Clarified | All docs |
| Missing version info | Medium | ✅ Added | Production docs |
| Firmware confusion | Medium | ✅ Clarified | All docs |
| NeoPixelBus contradiction | Medium | ✅ Resolved | All docs |
| Peripheral discovery | Medium | ✅ Clarified | All docs |
| File path references | Minor | ✅ Improved | All docs |

---

## Testing Status

✅ **Verified**:
- [x] Analog pin definitions corrected in firmware
- [x] I2C scanning works
- [x] Machine mode initialization
- [x] NDJSON output format
- [x] Plaintext command parsing
- [x] JSON command parsing (optional)
- [x] Peripheral discovery format

⚠️ **Not Yet Verified**:
- [ ] BME688 full sensor reading (library integration pending)
- [ ] NeoPixelBus library (future enhancement)
- [ ] Side-B UART physical wiring
- [ ] MDP v1 protocol (future)

---

## Documentation Updated

1. ✅ `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino` - Pin definitions fixed
2. ✅ `docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE.md` - All corrections applied
3. ✅ `docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE_CORRECTED.md` - New comprehensive reference
4. ✅ `docs/firmware/WEBSITE_INTEGRATION_UPDATES.md` - Corrections added
5. ✅ `docs/firmware/CRITICAL_FIXES_SUMMARY.md` - This document
6. ✅ `firmware/README.md` - Pin configuration corrected

---

## Next Steps for Website Agent

1. **Use Correct Pin Definitions**: GPIO6/7/10/11 for analog inputs
2. **Understand Protocol**: Plaintext commands (primary), JSON optional, NDJSON responses
3. **Clarify Side-B**: Experimental, may not be wired
4. **Check NeoPixel Status**: Basic control only, NeoPixelBus is future
5. **Realistic Peripherals**: I2C for discovery, other protocols for data

---

**Document Version**: 1.0.0  
**Last Updated**: 2024-12  
**Status**: All Critical Issues Resolved

