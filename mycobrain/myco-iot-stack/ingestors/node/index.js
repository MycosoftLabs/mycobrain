/* eslint-disable no-console */
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { EventHubConsumerClient } = require("@azure/event-hubs");
const cbor = require("cbor");
const { verify } = require("@noble/ed25519");
const { LRUCache } = require("lru-cache");
require("dotenv").config();

const { KustoConnectionStringBuilder, Client: KustoClient } = require("azure-kusto-data");
const {
  IngestClient: KustoIngestClient,
  IngestionProperties,
  DataFormat,
  IngestionMappingKind,
} = require("azure-kusto-ingest");

function mustGet(name) {
  const v = process.env[name];
  if (!v) throw new Error(`Missing env var ${name}`);
  return v;
}

function loadRegistry(filePath) {
  const p = path.resolve(__dirname, filePath);
  const txt = fs.readFileSync(p, "utf-8");
  const obj = JSON.parse(txt);
  return obj; // { deviceId: { publicKeyB64: "..." } } or { deviceId: "..." }
}

function b64ToU8(b64) {
  return new Uint8Array(Buffer.from(b64, "base64"));
}

function blake2b256(buf) {
  // Node supports outputLength for blake2b512 in recent versions.
  // If outputLength isn't supported in your Node, use digest() and slice(0, 32).
  const h = crypto.createHash("blake2b512", { outputLength: 32 });
  h.update(buf);
  return Buffer.from(h.digest());
}

function uuidBytesToHex(b) {
  const buf = Buffer.from(b);
  // basic UUID formatting (8-4-4-4-12)
  const hex = buf.toString("hex");
  return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
}

function protoName(p) {
  switch (p) {
    case 1: return "lorawan";
    case 2: return "mqtt";
    case 3: return "ble";
    case 4: return "lte";
    default: return "other";
  }
}

/**
 * Convert compact numeric-key envelope to verbose object for storage/querying.
 * Keys follow spec/myco-envelope-v1.md
 */
function toVerbose(env) {
  const geo = env[7];
  const readings = env[8] || [];
  const lat = geo ? geo[0] / 1e7 : null;
  const lon = geo ? geo[1] / 1e7 : null;

  const pack = readings.map((r) => {
    const sid = r[0];
    const vi = r[1];
    const vs = r[2];
    const unit = r[3];
    const q = r[4];
    const v = Number(vi) / Math.pow(10, Number(vs));
    return { id: sid, vi, vs, v, u: unit, q };
  });

  return {
    hdr: {
      deviceId: env[1],
      proto: protoName(env[2]),
      msgId: uuidBytesToHex(env[3]),
      msgId_b64: Buffer.from(env[3]).toString("base64"),
    },
    ts: {
      ms: Number(env[4]),
      utc: new Date(Number(env[4])).toISOString(),
      mono_ms: Number(env[6]),
    },
    seq: Number(env[5]),
    geo: geo ? { lat, lon, lat_e7: geo[0], lon_e7: geo[1], acc_m: geo[2] } : null,
    pack,
    meta: env[9] || {},
    hash_b64: env[10] ? Buffer.from(env[10]).toString("base64") : null,
    sig_b64: env[11] ? Buffer.from(env[11]).toString("base64") : null,
    _compact: true,
    v: Number(env[0]),
  };
}

/**
 * Verify envelope:
 * 1) strip h/z
 * 2) canonical CBOR encode
 * 3) blake2b-256 and compare with h
 * 4) verify Ed25519 over "MYCO1"||h
 */
async function verifyEnvelope(env, pubKeyU8) {
  const h = env[10];
  const sig = env[11];
  if (!h || !sig) return { ok: false, reason: "missing hash or signature" };

  // Build unsigned object (shallow clone, excluding keys 10 and 11)
  const unsigned = {};
  for (const k of Object.keys(env)) {
    const kn = Number(k);
    if (kn === 10 || kn === 11) continue;
    unsigned[kn] = env[kn];
  }

  const unsignedBytes = cbor.Encoder.encodeCanonical(unsigned);
  const h2 = blake2b256(unsignedBytes);

  if (!Buffer.from(h).equals(h2)) {
    return { ok: false, reason: "hash mismatch" };
  }

  const msg = Buffer.concat([Buffer.from("MYCO1"), h2]);
  const ok = await verify(new Uint8Array(sig), new Uint8Array(msg), pubKeyU8);
  return ok ? { ok: true } : { ok: false, reason: "bad signature" };
}

function mkAdxClients() {
  const ingestUri = mustGet("ADX_INGEST_URI");
  const clusterUri = ingestUri.replace(/^https:\/\/ingest-/, "https://").replace(".kusto.windows.net", ".kusto.windows.net");

  const tenant = process.env.ADX_TENANT_ID;
  const clientId = process.env.ADX_CLIENT_ID;
  const secret = process.env.ADX_CLIENT_SECRET;

  if (!tenant || !clientId || !secret) {
    throw new Error("This starter uses AAD app-key auth. For Managed Identity, adapt to DefaultAzureCredential + token auth.");
  }

  const kcsbIngest = KustoConnectionStringBuilder.withAadApplicationKeyAuthentication(
    ingestUri,
    clientId,
    secret,
    tenant
  );

  const kcsbData = KustoConnectionStringBuilder.withAadApplicationKeyAuthentication(
    clusterUri,
    clientId,
    secret,
    tenant
  );

  const ingestClient = new KustoIngestClient(kcsbIngest);
  const kustoClient = new KustoClient(kcsbData);

  return { ingestClient, kustoClient };
}

async function ingestRawBatch(ingestClient, records) {
  if (!records.length) return;
  const db = mustGet("ADX_DATABASE");
  const table = mustGet("ADX_RAW_TABLE");
  const mapping = mustGet("ADX_RAW_MAPPING");

  // MultiJSON: each line is one JSON record
  const jsonl = records.map((r) => JSON.stringify(r)).join("\n");

  const props = new IngestionProperties({
    database: db,
    table,
    format: DataFormat.MULTIJSON,
    ingestionMappingReference: mapping,
    ingestionMappingKind: IngestionMappingKind.JSON,
  });

  const { Readable } = require("stream");
  const stream = Readable.from([jsonl]);

  if (typeof ingestClient.ingestFromStream === "function") {
    await ingestClient.ingestFromStream(stream, props);
  } else {
    // Fallback: write to a temp file and ingestFromFile (older versions)
    const os = require("os");
    const tmp = path.join(os.tmpdir(), `myco_raw_${Date.now()}_${Math.random().toString(16).slice(2)}.jsonl`);
    fs.writeFileSync(tmp, jsonl, "utf-8");
    try {
      await ingestClient.ingestFromFile(tmp, props);
    } finally {
      try { fs.unlinkSync(tmp); } catch (_) {}
    }
  }
}

async function main() {
  const eventHubConnStr = mustGet("IOTHUB_EVENTHUB_CONNECTION_STRING");
  const consumerGroup = process.env.EVENTHUB_CONSUMER_GROUP || "$Default";

  const registryPath = mustGet("DEVICE_REGISTRY_PATH");
  const registry = loadRegistry(registryPath);

  const dedupTtl = Number(process.env.DEDUP_TTL_MS || 48 * 3600 * 1000);
  const dedupe = new LRUCache({ max: 500_000, ttl: dedupTtl });

  const batchMax = Number(process.env.BATCH_MAX_EVENTS || 200);
  const batchMaxMs = Number(process.env.BATCH_MAX_MS || 2000);

  const { ingestClient } = mkAdxClients();

  const client = new EventHubConsumerClient(consumerGroup, eventHubConnStr);

  let batch = [];
  let batchTimer = null;

  async function flush() {
    if (batchTimer) clearTimeout(batchTimer);
    batchTimer = null;
    const toSend = batch;
    batch = [];
    try {
      await ingestRawBatch(ingestClient, toSend);
      console.log(`Ingested raw batch: ${toSend.length}`);
    } catch (e) {
      console.error("ADX ingest failed:", e);
    }
  }

  function scheduleFlush() {
    if (batchTimer) return;
    batchTimer = setTimeout(() => flush().catch(console.error), batchMaxMs);
  }

  console.log("Starting consumer...");

  const subscription = client.subscribe(
    {
      processEvents: async (events, context) => {
        for (const ev of events) {
          // Body may be a string, Buffer, or object depending on sender.
          let bodyBuf = null;
          if (Buffer.isBuffer(ev.body)) {
            bodyBuf = ev.body;
          } else if (ev.body instanceof Uint8Array) {
            bodyBuf = Buffer.from(ev.body);
          } else if (typeof ev.body === "string") {
            bodyBuf = Buffer.from(ev.body, "utf-8");
          } else if (ev.body && typeof ev.body === "object") {
            // Might already be parsed JSON. Convert to canonical JSON bytes.
            bodyBuf = Buffer.from(JSON.stringify(ev.body), "utf-8");
          } else {
            console.warn("Unknown body type, skipping");
            continue;
          }

          // Try CBOR decode first (compact envelope)
          let env = null;
          try {
            env = cbor.decodeFirstSync(bodyBuf);
          } catch (_e) {
            // not CBOR - try JSON
            try {
              env = JSON.parse(bodyBuf.toString("utf-8"));
            } catch (e2) {
              console.warn("Body is neither CBOR nor JSON, skipping:", e2.message);
              continue;
            }
          }

          // If JSON, expect it to already have hdr/sig/etc. This starter focuses on CBOR compact.
          if (env && typeof env === "object" && env.hdr) {
            console.warn("Got verbose JSON envelope; implement verify for JSON if needed. Skipping for now.");
            continue;
          }

          const deviceId = env[1];
          const msgIdB64 = Buffer.from(env[3]).toString("base64");
          const dedupKey = `${deviceId}:${msgIdB64}`;
          if (dedupe.has(dedupKey)) continue;

          const regEntry = registry[deviceId];
          const pubB64 = typeof regEntry === "string" ? regEntry : regEntry?.publicKeyB64;
          if (!pubB64) {
            console.warn(`No pubkey for device ${deviceId}, skipping`);
            continue;
          }

          const ok = await verifyEnvelope(env, b64ToU8(pubB64));
          if (!ok.ok) {
            console.warn(`Bad envelope from ${deviceId}: ${ok.reason}`);
            continue;
          }

          dedupe.set(dedupKey, true);

          const verbose = toVerbose(env);
          const rawRecord = {
            ingestedAt: new Date().toISOString(),
            // Event Hubs enqueued time is on ev.enqueuedTimeUtc in @azure/event-hubs
            enqueuedTime: ev.enqueuedTimeUtc ? ev.enqueuedTimeUtc.toISOString() : null,
            deviceId,
            msgId: verbose.hdr.msgId,
            proto: verbose.hdr.proto,
            seq: verbose.seq,
            timeUtc: verbose.ts.utc,
            monoMs: verbose.ts.mono_ms,
            lat: verbose.geo ? verbose.geo.lat : null,
            lon: verbose.geo ? verbose.geo.lon : null,
            accM: verbose.geo ? verbose.geo.acc_m : null,
            body: verbose,
            rawCborB64: bodyBuf.toString("base64"),
            hashB64: verbose.hash_b64,
            sigB64: verbose.sig_b64,
          };

          batch.push(rawRecord);
          scheduleFlush();
          if (batch.length >= batchMax) await flush();

          // checkpointing can be added via blob checkpoint store if desired.
        }
      },
      processError: async (err, context) => {
        console.error("EventHub error:", err);
      },
    },
    { startPosition: "latest" }
  );

  process.on("SIGINT", async () => {
    console.log("Stopping...");
    await flush().catch(() => {});
    await subscription.close();
    await client.close();
    process.exit(0);
  });
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
