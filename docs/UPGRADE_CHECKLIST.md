# MycoBrain Firmware Upgrade Checklist

**Date**: December 30, 2024  
**Status**: ✅ **READY FOR DEPLOYMENT**  
**Purpose**: Ensure smooth and perfect firmware upgrade

---

## ✅ Pre-Upgrade Verification

### 1. Firmware Files Status

- [x] **Side-A Production Firmware**: `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`
  - [x] Analog pins corrected (GPIO6/7/10/11)
  - [x] Machine mode support added
  - [x] NDJSON output implemented
  - [x] Plaintext commands supported
  - [x] NeoPixel control (GPIO15)
  - [x] Buzzer control (GPIO16)

- [x] **Side-B Router Firmware**: `firmware/MycoBrain_SideB/MycoBrain_SideB.ino`
  - [x] Version set to 1.0.0-production
  - [x] UART routing implemented
  - [x] Status LED control

- [x] **ScienceComms Firmware**: `firmware/MycoBrain_ScienceComms/`
  - [x] Modular structure complete
  - [x] Optical/acoustic modems implemented
  - [x] Stimulus engine ready
  - [x] Peripheral discovery working

### 2. Documentation Status

- [x] **Main README**: `firmware/README.md`
  - [x] Pin mappings corrected
  - [x] Installation instructions complete
  - [x] Command reference updated

- [x] **Side-A README**: `firmware/MycoBrain_SideA/README.md`
  - [x] Pin mappings corrected
  - [x] Hardware configuration verified
  - [x] Commands documented

- [x] **Side-B README**: `firmware/MycoBrain_SideB/README.md`
  - [x] UART configuration documented
  - [x] Status LED behavior described

- [x] **Production Firmware Doc**: `docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE.md`
  - [x] All critical fixes applied
  - [x] Protocol clarifications added
  - [x] Version information included

- [x] **Website Integration Doc**: `docs/firmware/WEBSITE_INTEGRATION_UPDATES.md`
  - [x] Corrections documented
  - [x] Protocol details clarified

- [x] **Critical Fixes Summary**: `docs/firmware/CRITICAL_FIXES_SUMMARY.md`
  - [x] All issues documented
  - [x] Fixes verified

- [x] **Website Corrections**: `docs/firmware/WEBSITE_INTEGRATION_CORRECTIONS.md`
  - [x] Complete correction guide
  - [x] Testing checklist included

### 3. Repository Status

- [x] **Repository**: `mycosoft-mas` (GitHub: MycosoftLabs/mycosoft-mas)
- [x] **Firmware Location**: `firmware/` directory
- [x] **Documentation Location**: `docs/firmware/` directory
- [x] **All files committed**: Ready for push

---

## 📋 Upgrade Steps

### Step 1: Verify Current Firmware

**Before upgrading, verify current firmware version:**

1. Connect device via USB
2. Open Serial Monitor (115200 baud)
3. Send: `{"cmd":"get_version"}` or `status`
4. Note current version

### Step 2: Backup Current Firmware

**If you have a working firmware you want to keep:**

1. Save current `.ino` file with version suffix
2. Document current pin configuration
3. Note any custom modifications

### Step 3: Prepare Arduino IDE

**Required Settings (from `ARDUINO_IDE_SETTINGS.md`):**

- **Board**: ESP32S3 Dev Module
- **USB CDC on boot**: Enabled
- **USB DFU on boot**: Enabled (requires USB OTG mode)
- **USB Firmware MSC on boot**: Disabled
- **USB Mode**: Hardware CDC and JTAG
- **JTAG Adapter**: Integrated USB JTAG
- **PSRAM**: OPI PSRAM
- **CPU Frequency**: 240 MHz
- **WiFi Core Debug Level**: None
- **Arduino runs on core**: 1
- **Events run on core**: 1
- **Flash Mode**: QIO @ 80 MHz
- **Flash Size**: 16 MB
- **Partition Scheme**: 16MB flash, 3MB app / 9.9MB FATFS
- **Upload Speed**: 921600
- **Upload Port**: UART0/Hardware CDC
- **Erase all flash before upload**: Disabled

### Step 4: Install Required Libraries

**For Arduino IDE:**

1. **ArduinoJson** (v7.x or later)
   - Sketch → Include Library → Manage Libraries
   - Search: "ArduinoJson" by Benoit Blanchon
   - Install version 7.x

2. **NeoPixelBus** (for NeoPixel control)
   - Sketch → Include Library → Manage Libraries
   - Search: "NeoPixelBus" by Makuna
   - Install latest version

**For PlatformIO:**

Libraries are automatically installed via `platformio.ini`:
```ini
lib_deps = 
    makuna/NeoPixelBus
    bblanchon/ArduinoJson@^7.0.0
```

### Step 5: Flash Side-A Production Firmware

1. Open `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`
2. Verify board settings (see Step 3)
3. Select correct COM port (Side-A USB)
4. Click **Upload**
5. Wait for "Done uploading" message
6. Open Serial Monitor (115200 baud)
7. Verify boot sequence:
   - Should see initialization messages
   - Should see "Ready" status
   - Should see telemetry every 10 seconds

### Step 6: Test Side-A Firmware

**Test Commands (send via Serial Monitor):**

1. **Machine Mode Initialization:**
   ```
   mode machine
   dbg off
   fmt json
   scan
   status
   ```

2. **LED Control:**
   ```
   led rgb 255 0 0
   led rgb 0 255 0
   led rgb 0 0 255
   led off
   ```

3. **Buzzer Control:**
   ```
   buzzer pattern coin
   buzzer pattern bump
   buzzer pattern power
   buzzer pattern 1up
   buzzer pattern morgio
   buzzer stop
   ```

4. **I2C Scan:**
   ```
   scan
   ```
   Should return peripheral list in NDJSON format

5. **Telemetry:**
   - Should automatically send every 10 seconds
   - Format: NDJSON with `type: "telemetry"`

### Step 7: Flash Side-B Router Firmware (Optional)

**⚠️ Note**: Side-B router is experimental. Most deployments use Side-A USB directly.

If using Side-B router:

1. Open `firmware/MycoBrain_SideB/MycoBrain_SideB.ino`
2. Verify board settings (same as Side-A)
3. Select correct COM port (Side-B USB)
4. Click **Upload**
5. Verify UART wiring:
   - Side-A TX → Side-B RX (GPIO17 → GPIO16)
   - Side-B TX → Side-A RX (GPIO17 → GPIO16)
   - Baud: 115200

### Step 8: Verify Service Integration

**Test with Python service:**

1. Start service: `python services/mycobrain/mycobrain_dual_service.py`
2. Verify device connection
3. Test API endpoints:
   ```bash
   # Get device status
   curl http://localhost:8003/devices
   
   # Send command
   curl -X POST http://localhost:8003/devices/{id}/command \
     -H "Content-Type: application/json" \
     -d '{"cmd":"ping"}'
   
   # Get telemetry
   curl http://localhost:8003/devices/{id}/telemetry
   ```

### Step 9: Verify Website Integration

**Test with website device manager:**

1. Open website device manager
2. Connect to device
3. Verify initialization sequence:
   - Machine mode enabled
   - Debug off
   - JSON format set
   - Peripheral scan complete
   - Status received
4. Test controls:
   - LED color picker
   - Buzzer patterns
   - MOSFET toggles
   - Sensor readings
5. Verify telemetry display:
   - Analog inputs (GPIO6/7/10/11)
   - Environmental sensors
   - MOSFET states

---

## 🔍 Post-Upgrade Verification

### Hardware Pin Verification

- [ ] Analog inputs read from GPIO6/7/10/11 (not GPIO34/35/36/39)
- [ ] NeoPixel controlled via GPIO15
- [ ] Buzzer controlled via GPIO16
- [ ] I2C working on GPIO4/5
- [ ] MOSFETs working on GPIO12/13/14

### Protocol Verification

- [ ] Plaintext commands work (`mode machine`, `dbg off`, `fmt json`, `scan`)
- [ ] JSON commands work (`{"cmd":"ping"}`, `{"cmd":"status"}`)
- [ ] Machine mode outputs NDJSON
- [ ] All responses are newline-delimited JSON
- [ ] Telemetry format matches expected schema

### Integration Verification

- [ ] Website initialization sequence works
- [ ] Peripheral discovery returns correct format
- [ ] LED control works (plaintext and JSON)
- [ ] Buzzer control works (patterns and custom)
- [ ] Telemetry streaming works in NDJSON format
- [ ] Status command returns machine mode format
- [ ] Service API endpoints respond correctly
- [ ] MINDEX integration working (if configured)
- [ ] NatureOS integration working (if configured)

---

## 🚨 Troubleshooting

### Device Not Responding

1. Check USB connection
2. Verify COM port selection
3. Check baudrate (115200)
4. Verify power supply
5. Try hardware reset (hold BOOT button, press RESET, release BOOT)

### Wrong Analog Readings

1. **CRITICAL**: Verify pins are GPIO6/7/10/11 (not GPIO34/35/36/39)
2. Check analog input wiring
3. Verify voltage levels (0-3.3V)
4. Test with multimeter

### NeoPixel Not Working

1. Verify GPIO15 connection
2. Check NeoPixelBus library installed
3. Try: `led rgb 255 0 0`
4. Check power supply (NeoPixels need good power)

### Buzzer Not Working

1. Verify GPIO16 connection
2. Try: `buzzer pattern coin`
3. Check buzzer wiring
4. Verify PWM functionality

### Machine Mode Not Working

1. Send: `mode machine`
2. Verify response: `{"type":"ack","cmd":"mode",...}`
3. Check NDJSON output format
4. Verify `fmt json` was sent

### Service Not Connecting

1. Check service is running
2. Verify COM port in service config
3. Check device is not in use by another program
4. Verify baudrate matches (115200)

---

## 📝 Version Information

### Current Firmware Versions

- **Side-A Production**: 1.0.0-production
- **Side-B Router**: 1.0.0-production
- **ScienceComms**: 1.0.0-dev (experimental)

### Verified With

- **Arduino-ESP32 Core**: ESP32-S3 (latest stable)
- **Board Revision**: MycoBrain V1 (dual ESP32-S3)
- **Arduino IDE**: 1.8.19 or later
- **PlatformIO**: Latest stable

### Git Information

- **Repository**: `mycosoft-mas`
- **Location**: `firmware/` directory
- **Documentation**: `docs/firmware/` directory
- **Status**: All files committed and ready

---

## ✅ Upgrade Complete Checklist

Before marking upgrade as complete, verify:

- [ ] All firmware files flashed successfully
- [ ] All hardware pins verified (GPIO6/7/10/11 for analog)
- [ ] Machine mode working (NDJSON output)
- [ ] Plaintext commands working
- [ ] JSON commands working (optional)
- [ ] LED control working
- [ ] Buzzer control working
- [ ] I2C scanning working
- [ ] Telemetry streaming working
- [ ] Service integration working
- [ ] Website integration working
- [ ] All documentation updated
- [ ] All READMEs on GitHub
- [ ] Version information documented
- [ ] Troubleshooting guide available

---

## 📚 Additional Resources

- **Main Firmware README**: `firmware/README.md`
- **Production Firmware Doc**: `docs/firmware/MYCOBRAIN_PRODUCTION_FIRMWARE.md`
- **Website Integration**: `docs/firmware/WEBSITE_INTEGRATION_UPDATES.md`
- **Critical Fixes**: `docs/firmware/CRITICAL_FIXES_SUMMARY.md`
- **Website Corrections**: `docs/firmware/WEBSITE_INTEGRATION_CORRECTIONS.md`
- **Arduino IDE Settings**: `firmware/ARDUINO_IDE_SETTINGS.md`

---

**Document Version**: 1.0.0  
**Last Updated**: December 30, 2024  
**Status**: ✅ Ready for Deployment


