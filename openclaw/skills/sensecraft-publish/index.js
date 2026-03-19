/**
 * SenseCraft Publish Skill for OpenClaw
 *
 * Subscribes to MycoBrain telemetry (via mycobrain-control skill or serial)
 * and publishes to Seeed SenseCraft Data Platform via MQTT.
 *
 * SenseCraft MQTT API:
 *   Broker: sensecap-openstream.seeed.cc:1883
 *   Protocol: MQTT 3.1.1
 *   Auth: Organization ID + API Key from sensecap.seeed.cc portal
 *
 * Data format maps MycoBrain BME688 telemetry to SenseCraft measurement channels:
 *   Channel 1: Temperature (°C)
 *   Channel 2: Humidity (%)
 *   Channel 3: Pressure (hPa)
 *   Channel 4: IAQ Index
 *   Channel 5: CO2 Equivalent (ppm)
 *   Channel 6: VOC Equivalent (ppm)
 *   Channel 7: Gas Resistance (Ohm)
 *
 * Also compatible with SenseCAP M2 LoRaWAN gateway receiving MycoBrain
 * LoRa packets directly (no Jetson required for that path).
 *
 * Environment variables:
 *   SENSECRAFT_ORG_ID      - Organization ID from sensecap.seeed.cc
 *   SENSECRAFT_API_KEY     - API key for MQTT authentication
 *   SENSECRAFT_DEVICE_EUI  - Device EUI for data association
 *   SENSECRAFT_BROKER      - MQTT broker (default: sensecap-openstream.seeed.cc)
 *   SENSECRAFT_PORT        - MQTT port (default: 1883)
 */

const mqtt = require('mqtt');

const BROKER = process.env.SENSECRAFT_BROKER || 'sensecap-openstream.seeed.cc';
const PORT = parseInt(process.env.SENSECRAFT_PORT || '1883');
const ORG_ID = process.env.SENSECRAFT_ORG_ID || '';
const API_KEY = process.env.SENSECRAFT_API_KEY || '';
const DEVICE_EUI = process.env.SENSECRAFT_DEVICE_EUI || '';

let client = null;
let connected = false;

function ensureMqtt() {
  if (client && connected) return Promise.resolve();

  return new Promise((resolve, reject) => {
    if (!ORG_ID || !API_KEY) {
      reject(new Error('SENSECRAFT_ORG_ID and SENSECRAFT_API_KEY required'));
      return;
    }

    client = mqtt.connect(`mqtt://${BROKER}:${PORT}`, {
      username: ORG_ID,
      password: API_KEY,
      clientId: `mycobrain-${DEVICE_EUI || Date.now()}`,
      keepalive: 60,
      reconnectPeriod: 5000
    });

    client.on('connect', () => {
      connected = true;
      resolve();
    });

    client.on('error', (err) => {
      connected = false;
      reject(err);
    });

    client.on('close', () => {
      connected = false;
    });

    // Timeout after 10 seconds
    setTimeout(() => {
      if (!connected) reject(new Error('MQTT connection timeout'));
    }, 10000);
  });
}

/**
 * Convert MycoBrain telemetry to SenseCraft data format.
 */
function formatTelemetry(telemetry) {
  const channels = [];
  const bme = telemetry.bme688 || {};

  // Process both BME688 sensors (ambient 'a' and environment 'b')
  for (const [slot, data] of Object.entries(bme)) {
    const prefix = slot === 'a' ? 'amb' : 'env';
    if (data.temperature_c !== undefined)
      channels.push({ channel: `${prefix}_temperature`, value: data.temperature_c, unit: '°C' });
    if (data.humidity_pct !== undefined)
      channels.push({ channel: `${prefix}_humidity`, value: data.humidity_pct, unit: '%' });
    if (data.pressure_hpa !== undefined)
      channels.push({ channel: `${prefix}_pressure`, value: data.pressure_hpa, unit: 'hPa' });
    if (data.iaq !== undefined)
      channels.push({ channel: `${prefix}_iaq`, value: data.iaq, unit: '' });
    if (data.co2_equivalent !== undefined)
      channels.push({ channel: `${prefix}_co2`, value: data.co2_equivalent, unit: 'ppm' });
    if (data.voc_equivalent !== undefined)
      channels.push({ channel: `${prefix}_voc`, value: data.voc_equivalent, unit: 'ppm' });
    if (data.gas_ohm !== undefined)
      channels.push({ channel: `${prefix}_gas_resistance`, value: data.gas_ohm, unit: 'Ohm' });
  }

  // Claw status if present
  if (telemetry.claw) {
    channels.push({ channel: 'claw_position', value: telemetry.claw.position, unit: 'deg' });
    channels.push({ channel: 'claw_closed', value: telemetry.claw.is_closed ? 1 : 0, unit: '' });
  }

  // Analog inputs
  if (telemetry.analog) {
    for (const [key, val] of Object.entries(telemetry.analog)) {
      channels.push({ channel: key, value: val, unit: 'raw' });
    }
  }

  return {
    device_eui: DEVICE_EUI,
    timestamp: Date.now(),
    channels
  };
}

async function sensecraft_publish() {
  // First read sensors via mycobrain-control skill
  const { sensor_read } = require('../mycobrain-control/index.js');
  const telemetry = await sensor_read();

  await ensureMqtt();

  const topic = `/device/${DEVICE_EUI}/data`;
  const payload = formatTelemetry(telemetry);

  return new Promise((resolve, reject) => {
    client.publish(topic, JSON.stringify(payload), { qos: 1 }, (err) => {
      if (err) reject(err);
      else resolve({ status: 'published', channels: payload.channels.length, topic });
    });
  });
}

async function sensecraft_status() {
  return {
    connected,
    broker: BROKER,
    port: PORT,
    org_id: ORG_ID ? '***' + ORG_ID.slice(-4) : 'not set',
    device_eui: DEVICE_EUI || 'not set'
  };
}

module.exports = {
  sensecraft_publish,
  sensecraft_status
};
