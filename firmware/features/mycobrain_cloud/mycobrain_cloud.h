// MycoBrain global cloud uplink: AWS IoT Core over MQTT/TLS.
//
// Topics (thing = AWS IoT thing name, provisioned into NVS):
//   up   mycosoft/devices/{thing}/presence    retained JSON; LWT = {"status":"offline"}
//   up   mycosoft/devices/{thing}/mdp/frames  {"frame_b64": base64(raw MDP v1 payload)}
//   down mycosoft/devices/{thing}/cmd         {"frame_b64": ...} -> CommandHandler
//
// Certificates are never compiled in. They live in NVS namespace "mycocloud" and are
// written over USB serial with the "cloud ..." line commands (see handleSerialLine).
#pragma once

#include <Arduino.h>

namespace mycocloud {

using CommandHandler = void (*)(const uint8_t* mdp, uint16_t len);

// Loads NVS config. Returns false when the device has not been provisioned.
bool begin(CommandHandler onCommand, const char* firmwareVersion);

// Call every loop iteration. Handles reconnect with exponential backoff + jitter,
// flushes the offline ring buffer, and services the MQTT client.
void loop(uint32_t nowMs);

// Publishes a raw (COBS-decoded) MDP v1 payload. Buffers in RAM while offline;
// the oldest frame is dropped when the buffer is full.
bool publishMdp(const uint8_t* mdp, uint16_t len);

bool isProvisioned();
bool isConnected();

// USB serial provisioning. Returns true if the line was a "cloud" command.
//   cloud status
//   cloud set endpoint <host>      e.g. a314h9xu8rhvfs-ats.iot.us-east-1.amazonaws.com
//   cloud set thing <thing-name>
//   cloud set port <443|8883>      443 uses ALPN x-amzn-mqtt-ca (works where 8883 is blocked)
//   cloud pem <ca|cert|key>        then PEM lines, terminated by "cloud pem end"
//   cloud clear
bool handleSerialLine(const char* line);

}  // namespace mycocloud
