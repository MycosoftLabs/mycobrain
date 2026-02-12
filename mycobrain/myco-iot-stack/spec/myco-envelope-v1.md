# MycoEnvelope v1 (Compact CBOR + Ed25519)

This spec defines a **single envelope** that can be carried over LoRaWAN, MQTT, BLE, LTE, etc.
The envelope is:

- **Compact**: numeric keys, ints instead of floats where possible
- **Deterministic**: encoding is deterministic so that signatures are stable across implementations
- **Signed**: Ed25519 signature binds the content to the device identity

## 1. Deterministic CBOR rules

We use:
- Definite-length maps/arrays
- Keys are small integers and must be encoded in ascending order
- No floats (use scaled integers) to avoid cross-encoder float canonicalization issues

## 2. Top-level CBOR map (keys 0..11)

All keys are **unsigned integers**.

| Key | Name | Type | Description |
|---:|---|---|---|
| 0 | v | uint | schema version, `1` |
| 1 | d | tstr | deviceId (string) |
| 2 | p | uint | protocol enum (1=lorawan,2=mqtt,3=ble,4=lte,5=other) |
| 3 | m | bstr(16) | msgId (UUID bytes) |
| 4 | t | int | device UTC time in **epoch milliseconds** |
| 5 | s | uint | sequence number (monotonic) |
| 6 | n | uint | monotonic time in milliseconds (since boot) |
| 7 | g | map | geo (optional; omit entirely if unknown) |
| 8 | r | array | readings array |
| 9 | x | map | metadata (optional) |
| 10 | h | bstr(32) | hash of the **unsigned envelope** |
| 11 | z | bstr(64) | Ed25519 signature over `"MYCO1" || h` |

### Geo map `g`

| Key | Name | Type | Description |
|---:|---|---|---|
| 0 | lat_e7 | int | latitude * 1e7 |
| 1 | lon_e7 | int | longitude * 1e7 |
| 2 | acc_m | uint | accuracy meters |

### Reading object (each element of `r`)

Each reading is a CBOR map:

| Key | Name | Type | Description |
|---:|---|---|---|
| 0 | id | uint or tstr | sensor id code (recommended uint) |
| 1 | vi | int | scaled integer value |
| 2 | vs | uint | decimal scale (0..9). Actual value = vi / 10^vs |
| 3 | u | uint or tstr | unit code (recommended uint) |
| 4 | q | uint | quality (0=ok, 1=warn, 2=bad) |

## 3. Hash + signature

Let `U` be the deterministic CBOR bytes of the envelope **without** keys 10(h) and 11(z).

- `h = BLAKE2b-256(U)`  (32 bytes)
- `z = Ed25519-SIGN(privKey, "MYCO1" || h)`  (64 bytes)

Verification:
1) Re-encode unsigned envelope deterministically -> bytes `U`
2) Compute `h' = BLAKE2b-256(U)`
3) Check `h' == h`
4) Verify `Ed25519-VERIFY(pubKey, "MYCO1" || h, z)`

> If you prefer SHA-256 instead of BLAKE2b-256, swap the hash function in both device + cloud; the rest is identical.

## 4. Size notes
- Signature is 64 bytes, hash is 32 bytes.
- If you must fit very small links (e.g., LoRa DR0), consider:
  - dropping `h` and signing `U` directly, or
  - using a truncated hash (`h8 = first 8 bytes`) and signing full `U`.

## 5. Enumerations
See `spec/sensor_codes.md` for a starter sensor/unit codebook.

