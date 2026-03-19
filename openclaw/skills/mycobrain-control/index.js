/**
 * MycoBrain Control Skill for OpenClaw
 *
 * Bridges OpenClaw tool invocations to MycoBrain hardware via serial JSON->MDP protocol.
 * Sends newline-delimited JSON to Side B ESP32-S3, which translates to MDP COMMAND
 * frames for Side A (sensors/actuators).
 *
 * Serial protocol (Jetson UART -> Side B):
 *   TX: {"tool":"mycobrain","action":"<cmd>","params":{...}}\n
 *   RX: {"status":"forwarded","cmd":"<cmd>"}\n  (immediate)
 *   RX: MDP binary frames (async telemetry/ack from Side A, forwarded by Side B)
 *
 * Compatible with: reSpeaker-claw voice pipeline, DuckyClaw edge agent,
 *                  SO-Arm/LeRobot patterns, SenseCraft Data Platform.
 */

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// Configuration: serial port to MycoBrain Side B
const SERIAL_PORT = process.env.MYCOBRAIN_SERIAL_PORT || '/dev/ttyTHS1';
const SERIAL_BAUD = parseInt(process.env.MYCOBRAIN_SERIAL_BAUD || '115200');

let port = null;
let parser = null;
let responseQueue = [];

function ensureConnection() {
  if (port && port.isOpen) return Promise.resolve();

  return new Promise((resolve, reject) => {
    port = new SerialPort({
      path: SERIAL_PORT,
      baudRate: SERIAL_BAUD,
      autoOpen: false
    });

    parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

    // Listen for JSON responses from Side B
    parser.on('data', (line) => {
      try {
        const data = JSON.parse(line.trim());
        if (responseQueue.length > 0) {
          const resolver = responseQueue.shift();
          resolver(data);
        }
      } catch (e) {
        // Not JSON — MDP binary telemetry forwarded as hex, ignore
      }
    });

    port.open((err) => {
      if (err) reject(new Error(`Failed to open ${SERIAL_PORT}: ${err.message}`));
      else resolve();
    });
  });
}

function sendCommand(action, params = {}) {
  return new Promise(async (resolve, reject) => {
    try {
      await ensureConnection();

      const cmd = JSON.stringify({ tool: 'mycobrain', action, params });
      const timeout = setTimeout(() => {
        // Remove from queue on timeout
        const idx = responseQueue.indexOf(resolve);
        if (idx !== -1) responseQueue.splice(idx, 1);
        resolve({ status: 'timeout', action });
      }, 5000);

      responseQueue.push((data) => {
        clearTimeout(timeout);
        resolve(data);
      });

      port.write(cmd + '\n');
    } catch (err) {
      reject(err);
    }
  });
}

// ---- Tool Handlers ----

async function sensor_read() {
  return sendCommand('read_sensors');
}

async function sensor_stream({ rate_hz = 1 }) {
  return sendCommand('stream_sensors', { rate_hz });
}

async function claw_grip() {
  return sendCommand('claw_grip');
}

async function claw_release() {
  return sendCommand('claw_release');
}

async function claw_position({ angle }) {
  return sendCommand('claw_position', { angle });
}

async function claw_status() {
  return sendCommand('claw_status');
}

async function output_control({ id, value }) {
  return sendCommand('output_control', { id, value });
}

async function led_set({ r, g, b }) {
  return sendCommand('output_control', { id: 'neopixel', value: 1, r, g, b });
}

async function buzzer({ freq = 1200, duration_ms = 200 }) {
  return sendCommand('output_control', { id: 'buzzer', value: 1, freq, duration_ms });
}

async function device_status() {
  return sendCommand('health');
}

async function lora_send({ payload, qos = 0 }) {
  return sendCommand('lora_send', { payload, qos });
}

async function drone_mission({ type, target_device_id }) {
  const missionCmds = {
    deploy: 'drone_deploy_payload',
    retrieve: 'drone_retrieve_payload',
    data_mule: 'drone_data_mule_start',
    rtl: 'drone_rtl',
    land: 'drone_land'
  };
  const cmd = missionCmds[type] || type;
  return sendCommand(cmd, { target_device_id });
}

// Export tool handlers for OpenClaw skill runtime
module.exports = {
  sensor_read,
  sensor_stream,
  claw_grip,
  claw_release,
  claw_position,
  claw_status,
  output_control,
  led_set,
  buzzer,
  device_status,
  lora_send,
  drone_mission
};
