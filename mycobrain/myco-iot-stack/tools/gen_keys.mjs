#!/usr/bin/env node
import { mkdirSync, writeFileSync } from "node:fs";
import { randomBytes } from "node:crypto";
import * as ed from "@noble/ed25519";

function b64(u8) { return Buffer.from(u8).toString("base64"); }

function parseArgs() {
  const args = process.argv.slice(2);
  const out = { count: 1, out: "../devices.json", prefix: "myco-node-" };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--count") out.count = Number(args[++i]);
    else if (args[i] === "--out") out.out = args[++i];
    else if (args[i] === "--prefix") out.prefix = args[++i];
  }
  return out;
}

const { count, out, prefix } = parseArgs();

const pub = {};
const priv = {};

for (let i = 1; i <= count; i++) {
  const deviceId = `${prefix}${String(i).padStart(3, "0")}`;
  const sk = ed.utils.randomPrivateKey();
  const pk = await ed.getPublicKey(sk);

  // noble uses 32-byte private key. For Monocypher signing we typically store a 64-byte secret key.
  // You can derive a 64-byte secret key from seed + public key:
  const sk64 = new Uint8Array(64);
  sk64.set(sk, 0);
  sk64.set(pk, 32);

  pub[deviceId] = { publicKeyB64: b64(pk) };
  priv[deviceId] = { privateKeyB64: b64(sk64), seed32B64: b64(sk) };
}

writeFileSync(out, JSON.stringify(pub, null, 2));
writeFileSync(out.replace(".json", "_private.json"), JSON.stringify(priv, null, 2));
console.log(`Wrote ${out} and ${out.replace(".json", "_private.json")}`);
