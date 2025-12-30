# MycoBrain Command Reference

## Operating Modes

### `mode <human|machine>`
Switch between human-readable and machine-readable output modes.

- **human**: Banners, help text, readable formatting
- **machine**: NDJSON only, no banners, JSON acks for all commands

```bash
mode machine   # Enable NDJSON mode
mode human     # Enable readable mode
```

### `dbg <on|off>`
Enable/disable debug output.

```bash
dbg on         # Enable debug
dbg off        # Disable debug
```

## System Commands

### `help`
Display help text (human mode only).

### `status`
Show complete system status as JSON.

```json
{
  "type": "telemetry",
  "ts": 12345,
  "board_id": "AABBCCDDEEFF",
  "firmware": "MycoBrain-ScienceComms",
  "version": "1.0.0",
  "led": {"r": 0, "g": 0, "b": 0, "brightness": 128},
  "buzzer": {"frequency": 0, "pattern": null, "busy": false},
  "outputs": {...}
}
```

## LED Commands (GPIO15)

### `led rgb <r> <g> <b>`
Set LED color (0-255 for each channel).

```bash
led rgb 255 0 0      # Red
led rgb 0 255 0      # Green
led rgb 0 0 255      # Blue
led rgb 255 255 255  # White
```

### `led off`
Turn off LED.

### `led status`
Get LED status as JSON.

### `led pattern <name>`
Start a pattern animation.

**Patterns:**
- `rainbow` - Cycle through rainbow colors
- `pulse` - Blink on/off
- `sweep` - HSV color sweep
- `beacon` - Brief flash

```bash
led pattern rainbow
```

## Buzzer Commands (GPIO16)

### `buzz tone <hz> <ms>`
Play a tone at specified frequency for duration.

```bash
buzz tone 1000 500   # 1kHz for 500ms
buzz tone 440 1000   # A4 note for 1 second
```

### `buzz pattern <name>`
Play a named sound pattern.

**Patterns:**
- `coin` - Mario coin sound
- `bump` - Mario bump sound
- `power` - Mario power-up
- `1up` - Mario 1-UP
- `morgio` - Custom jingle
- `alert` - Alert beeps
- `warning` - Warning beeps
- `success` - Success melody
- `error` - Error sound

```bash
buzz pattern coin
buzz pattern morgio
```

### `buzz stop`
Stop buzzer immediately.

## Legacy Pattern Aliases

These work as direct commands:
```bash
coin     # Play coin sound
morgio   # Play morgio jingle
1up      # Play 1-UP sound
```

## Optical Modem Commands (LiFi TX)

### `optx start <profile> [params]`
Start optical data transmission.

**Profiles:**
- `camera_ook` - On-Off Keying (5-20 Hz, camera-friendly)
- `camera_manchester` - Manchester encoding (more robust)
- `beacon` - Simple beacon pattern
- `morse` - Morse code

**Parameters:**
- `rate_hz=<n>` - Symbol rate (default: 10)
- `payload_b64=<base64>` - Data to transmit (required)
- `repeat=<true|false>` - Loop transmission

```bash
optx start camera_ook payload_b64=SGVsbG8= rate_hz=10 repeat=true
```

### `optx pattern <name>`
Run a visual pattern (not data transmission).

**Patterns:**
- `pulse` - On/off blinking
- `sweep` - Color sweep
- `beacon` - Brief flashes

```bash
optx pattern sweep
```

### `optx stop`
Stop optical transmission.

### `optx status`
Get modem status as JSON.

```json
{
  "transmitting": true,
  "profile": "camera_ook",
  "bytes_sent": 5,
  "bits_sent": 40,
  "rate_hz": 10
}
```

## Acoustic Modem Commands (FSK TX)

### `aotx start <profile> [params]`
Start acoustic data transmission.

**Profiles:**
- `simple_fsk` - 2-tone FSK with preamble + CRC
- `morse` - Morse code

**Parameters:**
- `f0=<hz>` - Mark frequency (default: 1800)
- `f1=<hz>` - Space frequency (default: 2400)
- `symbol_ms=<ms>` - Symbol duration (default: 30)
- `payload_b64=<base64>` - Data to transmit (required)
- `repeat=<true|false>` - Loop transmission

```bash
aotx start simple_fsk payload_b64=SGVsbG8= f0=1800 f1=2400 symbol_ms=30
```

### `aotx pattern <name>`
Run an audio pattern.

**Patterns:**
- `sweep` - Linear frequency sweep
- `chirp` - Exponential frequency sweep
- `pulse_train` - Pulsed tones

```bash
aotx pattern sweep
```

### `aotx stop`
Stop acoustic transmission.

### `aotx status`
Get modem status as JSON.

## Stimulus Commands

Used for repeatable experiments with organisms.

### `stim light <pattern> [params]`
Start light stimulus.

**Patterns:**
- `pulse` / `flash` - On/off blinking
- `ramp` - Brightness ramp up/down
- `strobe` - Fast strobe

**Parameters:**
- `r=<n>`, `g=<n>`, `b=<n>` - Color (0-255)
- `on=<ms>` - On duration
- `off=<ms>` - Off duration
- `ramp=<ms>` - Ramp time (for ramp pattern)
- `cycles=<n>` - Number of cycles (0 = infinite)

```bash
stim light pulse r=255 g=0 b=0 on=1000 off=1000 cycles=10
stim light ramp r=0 g=255 b=0 ramp=2000 cycles=5
```

### `stim sound <pattern> [params]`
Start sound stimulus.

**Patterns:**
- `tone` / `pulse` - On/off tones
- `sweep` - Linear frequency sweep
- `chirp` - Exponential frequency sweep

**Parameters:**
- `freq=<hz>` - Frequency (or start frequency for sweep)
- `freq_end=<hz>` - End frequency (for sweep)
- `on=<ms>` - On duration / sweep duration
- `off=<ms>` - Off duration between cycles
- `cycles=<n>` - Number of cycles (0 = infinite)

```bash
stim sound pulse freq=1000 on=500 off=500 cycles=10
stim sound sweep freq=500 freq_end=2000 on=2000 cycles=3
```

### `stim stop`
Stop all stimuli.

### `stim status`
Get stimulus status as JSON.

## Peripheral Commands

### `periph scan`
Scan I2C bus for devices.

```json
{"type": "periph_scan", "found": 2}
```

### `periph list`
List all known peripherals.

```json
{
  "type": "periph_list",
  "count": 2,
  "devices": [...]
}
```

### `periph describe <address>`
Get detailed descriptor for a peripheral.

```bash
periph describe 0x76   # BME688 at address 0x76
```

### `periph hotplug <on|off>`
Enable/disable hotplug monitoring.

```bash
periph hotplug on     # Enable periodic re-scanning
periph hotplug off    # Disable
```

## Output Commands (GPIO12/13/14)

### `out set <channel> <state>`
Set digital output state.

```bash
out set 1 1    # OUT_1 high
out set 1 0    # OUT_1 low
out set 2 1    # OUT_2 high
out set 3 1    # OUT_3 high
```

### `out pwm <channel> <value> [freq]`
Set PWM output.

```bash
out pwm 1 128        # 50% duty cycle at default frequency
out pwm 2 255 1000   # 100% at 1kHz
out pwm 3 64 500     # 25% at 500Hz
```

### `out status`
Get output status as JSON.

## JSON Command Format

Commands can also be sent as JSON objects:

```json
{"cmd": "led.rgb", "r": 255, "g": 0, "b": 0}
```

## NDJSON Output Format

In machine mode, all output is NDJSON (one JSON object per line):

### Acknowledgment
```json
{"type": "ack", "cmd": "led", "message": "Color set", "ts": 12345}
```

### Error
```json
{"type": "err", "cmd": "led", "error": "Invalid color", "ts": 12345}
```

### Telemetry
```json
{"type": "telemetry", "ts": 12345, "board_id": "...", "led": {...}, ...}
```

### Peripheral Report
```json
{"type": "periph", "board_id": "...", "address": "0x76", "peripheral_type": "bme688", ...}
```

