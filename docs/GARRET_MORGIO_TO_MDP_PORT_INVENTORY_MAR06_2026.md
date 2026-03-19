# Garret Morgio Firmware → MDP Port Inventory

**Date**: March 6, 2026  
**Status**: Reference / Implementation Guide  
**Purpose**: Inventory of commands, features, and capabilities in Garret's morgio firmware that should be ported to MycoBrain_SideA_MDP without breaking MDP/COBS.

---

## Overview

Garret's firmware lives in `firmware/MycoBrain_SideA/` (`.ino` files: `MycoBrain_NeoPixel_Fixed_FIXED.ino`, `MycoBrain_SideA_WORKING_DEC29.ino`) and shares patterns with `firmware/side_a/` (Buzzer/Pixel modules). The target is `firmware/MycoBrain_SideA_MDP/` which uses MDP binary protocol over Serial.

**Constraint**: Do NOT change MDP/COBS binary protocol. Add only ASCII CLI commands (lines terminated by `\n` or `\r`); MDP uses 0x00-delimited frames.

---

## Source Locations

| Feature | Garret (.ino) | side_a (modules) |
|---------|---------------|------------------|
| poster, morgio, coin, bump, power, 1up | `MycoBrain_NeoPixel_Fixed_FIXED.ino` | `buzzer.cpp` (patterns) |
| probe, regs | `MycoBrain_NeoPixel_Fixed_FIXED.ino` | - |
| live, dbg, fmt, rate, led | `MycoBrain_NeoPixel_Fixed_FIXED.ino` | `pixel.cpp` (patterns) |
| I2C scan, i2c config | Both | - |

---

## Already Ported to MDP

| Command | Status |
|---------|--------|
| `help` | ✅ Done |
| `scan` | ✅ Done |
| `status` | ✅ Done |
| `i2c <sda> <scl> [hz]` | ✅ Done (re-init sensors) |

---

## To Port: CLI Commands

### 1. poster

**What**: Reprint SuperMorgIO ASCII POST screen.  
**Source**: `kPoster[]` in Garret .ino (lines 167–191).  
**Implementation**: Add `static const char kPoster[] = "..."` and `printPoster()`; call from `handleCliCommand` when `cmd == "poster"`.  
**Risk**: None. Pure Serial output.

### 2. morgio, coin, bump, power, 1up

**What**: Retro buzzer SFX.  
**Source**: Garret uses `tone(BUZZER_PIN, hz)` / `noTone()` with blocking `delay()`. side_a has non-blocking `Buzzer::playPattern()` with `ToneStep` arrays.  
**MDP**: Uses `ledcWriteTone(0, freq)` on BUZZER_PIN 16. No blocking in MDP loop (would starve BSEC).  
**Options**:
- **A (simple)**: Copy Garret’s blocking `playTone()` + `sfx*()` into CLI handler. These run once per command; short (≈0.5–2 s). Acceptable for CLI.
- **B (clean)**: Add non-blocking pattern state machine; call `updatePattern()` in `loop()`. More code, no blocking.  
**Recommendation**: A for MVP; commands are rare.

**Tone sequences (from Garret)**:
- `sfxCoin`: E6 35ms, B5 25ms
- `sfxBump`: C5 40ms, REST 10ms, C5 25ms
- `sfxPowerUp`: C5, E5, G5, C6
- `sfx1Upish`: E5, G5, A5, C6
- `sfxSuperMorgIOBoot`: full jingle (C5, E5, G5, C6, D5, F5, A5, D6, E5, G5, B5, A5, G5, E5, C5, D5, G5, C5)

**Note**: Garret uses `tone()` (Arduino); MDP uses `ledcWriteTone()`. Need `initBuzzer()` before first use (already exists).

### 3. live on|off

**What**: Toggle periodic live sensor output to Serial.  
**Source**: `gLive`, `gLivePeriodMs`, `gLastLiveMs`; in loop, if live and interval elapsed, print readings.  
**MDP**: Add `g_live_on`, `g_live_period_ms`, `g_last_live_ms`. In `loop()`, if `g_live_on` and `millis() - g_last_live_ms >= g_live_period_ms`, print sensor lines (or JSON), then update `g_last_live_ms`.  
**Risk**: Can mix with MDP frames on same Serial. Use distinct formatting (e.g. `# live` prefix) so host can filter. Or document that live is for Serial Monitor only.

### 4. probe amb|env [n]

**What**: Read BME688 chip_id (reg 0xD0) and variant_id (reg 0xF0) n times (default 3).  
**Source**: `printBmeIdentity(addr, repeats)`; `i2cRead8(addr, 0xD0, chip)` and `i2cRead8(addr, 0xF0, var)`.  
**MDP**: Add `i2cRead8()` (Wire), `printBmeIdentity()`, and CLI parsing. Slots: amb=0x77, env=0x76.  
**Risk**: None. Read-only I2C.

### 5. regs amb|env

**What**: Single read of chip_id and variant_id.  
**Source**: Same as probe with n=1.  
**MDP**: Call `printBmeIdentity(addr, 1)`.

### 6. dbg on|off

**What**: Toggle callback debug prints.  
**Source**: `gDebug`; in BSEC callback, if debug, print.  
**MDP**: Add `g_dbg`; in `slotCallbackCommon` (or callbacks), if `g_dbg` and throttled (e.g. `g_last_dbg_ms`), print.  
**Risk**: High print rate can slow loop; use throttle (e.g. 3 s).

### 7. fmt lines|json

**What**: Output format for live/status: human-readable lines vs NDJSON.  
**Source**: `gFmt` (FMT_LINES, FMT_NDJSON); `printOneSensorLine` vs JSON line.  
**MDP**: Add `g_fmt`; use in live output and possibly `printStatus()`.

### 8. rate amb|env lp|ulp

**What**: Set BSEC sample rate (LP or ULP) per sensor and re-init.  
**Source**: `s.sampleRate = BSEC_SAMPLE_RATE_LP | ULP`; `updateSubscription(..., s.sampleRate)`; then re-init slot.  
**MDP**: `slotInit` currently uses `BSEC_SAMPLE_RATE_LP`. Add `sampleRate` to `SensorSlot`, parse `lp`/`ulp`, set rate, call slot re-init (Wire already active).  
**Risk**: Medium. Re-init must not corrupt MDP tx/rx. Keep init short.

### 9. led mode off|state|manual

**What**: LED mode. off=all off; state=reflect sensor status (green/yellow/blue/red); manual=use `led rgb` values.  
**Source**: `gLedMode`, `ledStateUpdate()` in loop.  
**MDP**: MDP has `output_control` for neopixel (r,g,b). CLI `led mode` would need a state machine in loop for `state` mode. Optional for first port.

### 10. led rgb <r> <g> <b>

**What**: Set NeoPixel to manual RGB (and switch to manual mode).  
**MDP**: Trivial: `pixels.setPixelColor(0, pixels.Color(r,g,b)); pixels.show();`. Already supported via MDP `output_control`; CLI is convenience.

---

## To Port: Features / Behaviors

### LED state machine (state mode)

**What**: Blink/color by sensor state:
- No sensors: red pulse
- Begin fail: red blink
- Init phase (waiting data): blue pulse
- Sub fail: yellow
- All ok: green  

**Source**: `ledStateUpdate()` in Garret.  
**MDP**: Call from `loop()` when `g_led_mode == LEDMODE_STATE`. Use existing `pixels` object.

### Boot poster

**What**: Print poster once at boot (after Serial init).  
**Source**: `printPoster()` in setup.  
**MDP**: Optional. Garret has `BOOT_SERIAL_WAIT_MS` for CDC; MDP may not need it. Add `printPoster()` at end of `setup()` if desired.

---

## Implementation Priority

| Priority | Command / Feature | Effort | Value |
|----------|-------------------|--------|-------|
| 1 | poster | Low | Nostalgic, useful for demos |
| 2 | morgio, coin, bump, power, 1up | Low | Fun, debugging |
| 3 | led rgb | Low | Convenience |
| 4 | probe, regs | Low | Diagnostics |
| 5 | live on\|off | Medium | Development |
| 6 | dbg on\|off | Low | Development |
| 7 | fmt lines\|json | Low | Integration |
| 8 | rate amb\|env lp\|ulp | Medium | Power/data tuning |
| 9 | led mode off\|state\|manual | Medium | UX |

---

## Hardware Compatibility

| Item | Garret | MDP | Match |
|------|--------|-----|-------|
| I2C SDA | 5 | 5 | ✅ |
| I2C SCL | 4 | 4 | ✅ |
| NeoPixel pin | 15 | 15 | ✅ |
| Buzzer pin | 16 | 16 | ✅ |
| BME AMB addr | 0x77 | 0x77 | ✅ |
| BME ENV addr | 0x76 | 0x76 | ✅ |

---

## MDP Commands (No Change)

These stay as-is; do not add duplicate CLI logic that conflicts:
- `hello`, `health`, `read_sensors`, `stream_sensors`
- `output_control` (mosfet, buzzer, neopixel)
- `estop`, `clear_estop`
- `enable_peripheral`, `disable_peripheral`

---

## Reference: BME688 Registers

- `0xD0`: chip_id (0x61 for BME68x)
- `0xF0`: variant_id (BME688 vs BME680 etc.)

---

## Related Documents

- [MDP Protocol Contracts](./MDP_PROTOCOL_CONTRACTS_MAR07_2026.md)
- [Firmware Architecture](./FIRMWARE_ARCHITECTURE_FEB10_2026.md)
