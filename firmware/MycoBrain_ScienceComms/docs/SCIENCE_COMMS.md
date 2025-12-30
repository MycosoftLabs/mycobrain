# MycoBrain Science Communications Guide

## Overview

The MycoBrain Science Communications firmware enables two novel communication channels:

1. **Optical Modem (LiFi)** - Data transmission via LED blinking
2. **Acoustic Modem (FSK)** - Data transmission via buzzer tones

These can be used for:
- Communicating with devices that have cameras or photodiodes
- Communicating with devices that have microphones
- Cross-air-gap data transfer
- Experimental stimulus delivery to organisms

## Optical Modem (LiFi)

### Theory of Operation

The optical modem encodes binary data as LED blink patterns. A camera or photodiode on the receiving device can decode the signal.

### Encoding Profiles

#### Camera OOK (On-Off Keying)
- Simple: LED on = 1, LED off = 0
- Best for: smartphone cameras, webcams
- Rate: 5-20 Hz (matches camera frame rates)
- Pros: Simple, easy to decode
- Cons: DC offset issues, susceptible to ambient light

#### Camera Manchester
- LED transition indicates bit value
- Rising edge = 0, Falling edge = 1
- Best for: Robust camera detection
- Rate: Half of OOK (each bit takes 2 symbols)
- Pros: Self-clocking, no DC offset
- Cons: Lower throughput

### Protocol

```
[PREAMBLE][DATA][CRC16]
    8 bits  N bits  16 bits
```

- **Preamble**: Alternating 1-0-1-0... for synchronization
- **Data**: Your payload
- **CRC16**: Error detection (MODBUS polynomial)

### Example: Camera Transmission

```bash
# Switch to machine mode
mode machine

# Encode "Hello" to base64: SGVsbG8=
# Start transmission at 10 Hz
optx start camera_ook payload_b64=SGVsbG8= rate_hz=10 repeat=true

# Check status
optx status

# Stop when done
optx stop
```

### Receiving Tips

1. Point camera at LED
2. Use slow-motion (240 fps) for best results
3. Process video frames for brightness changes
4. Look for preamble pattern to sync
5. Decode Manchester or OOK based on profile

## Acoustic Modem (FSK)

### Theory of Operation

The acoustic modem encodes binary data as frequency shifts in audio tones. A microphone on the receiving device captures the audio, and software demodulates it.

### Encoding: Simple FSK

- Two frequencies represent 0 and 1
- Default: f0=1800 Hz (mark), f1=2400 Hz (space)
- Symbol duration: 30ms typical

### Protocol

```
[PREAMBLE][DATA][CRC16]
   16 sym   N bits  16 bits
```

### Example: FSK Transmission

```bash
# Switch to machine mode
mode machine

# Encode "Hello" to base64: SGVsbG8=
# Start FSK transmission
aotx start simple_fsk payload_b64=SGVsbG8=

# With custom frequencies
aotx start simple_fsk payload_b64=SGVsbG8= f0=1000 f1=2000 symbol_ms=50

# Check status
aotx status

# Stop
aotx stop
```

### Receiving Tips

1. Record audio from microphone
2. Band-pass filter around f0 and f1
3. Detect preamble for sync
4. Measure dominant frequency per symbol window
5. Decode as 0/1 based on frequency
6. Verify CRC16

### Recommended Settings by Environment

| Environment | f0 | f1 | Symbol MS |
|------------|----|----|-----------|
| Quiet room | 1800 | 2400 | 30 |
| Noisy | 2500 | 3500 | 50 |
| Long range | 1000 | 1500 | 100 |

## Stimulus Engine

For scientific experiments with organisms (fungi, bacteria, plants).

### Key Difference from Modems

- **Modem mode**: Designed for data encoding/decoding
- **Stimulus mode**: Designed for repeatable, timed patterns

### Light Stimulus Example

```bash
# Red flashing for fungal response experiment
stim light pulse r=255 g=0 b=0 on=1000 off=1000 cycles=100

# Blue ramp for circadian experiments
stim light ramp r=0 g=0 b=255 ramp=5000 cycles=10

# Fast strobe (caution: may cause seizures in humans)
stim light strobe r=255 g=255 b=255
```

### Sound Stimulus Example

```bash
# 1 kHz tone pulses
stim sound pulse freq=1000 on=500 off=500 cycles=50

# Frequency sweep (may affect insect behavior)
stim sound sweep freq=500 freq_end=5000 on=2000 cycles=10

# Chirp pattern
stim sound chirp freq=1000 freq_end=4000 on=1000 cycles=20
```

## Data Encoding

### Base64 Encoding

All payloads use base64 encoding:

| Text | Base64 |
|------|--------|
| Hello | SGVsbG8= |
| Test | VGVzdA== |
| 1234 | MTIzNA== |

Python example:
```python
import base64
payload = base64.b64encode(b"Hello World").decode()
# Result: SGVsbG8gV29ybGQ=
```

### CRC16 Calculation

The firmware uses MODBUS CRC16:
- Polynomial: 0xA001
- Initial: 0xFFFF

This is automatically calculated and appended to transmissions.

## Practical Applications

### 1. Cross-Air-Gap Communication
Transfer data between isolated systems using optical or acoustic channels.

### 2. IoT Mesh Backup
When RF fails, use light/sound as backup channel.

### 3. Underwater Communication
Acoustic modem works in water (with appropriate housing).

### 4. Biological Experiments
Study organism response to controlled light/sound stimuli.

### 5. Art Installations
Interactive displays that communicate via ambient light/sound.

## Performance Specifications

| Feature | Optical | Acoustic |
|---------|---------|----------|
| Max data rate | ~20 bps (camera) | ~33 bps (30ms symbols) |
| Range | 1-10 m (line of sight) | 1-50 m (depending on volume) |
| Error detection | CRC16 | CRC16 |
| Encoding | OOK, Manchester | FSK |
| Receiver | Camera, photodiode | Microphone |

## Future Enhancements

- **RX modes**: Photodiode/microphone input for bidirectional communication
- **ggwave compatibility**: Full ggwave multi-tone encoding
- **Forward error correction**: Reed-Solomon or convolutional coding
- **Spatial modulation**: Multi-LED arrays for parallel transmission

