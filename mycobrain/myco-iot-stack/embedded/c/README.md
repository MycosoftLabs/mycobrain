# Embedded C (ESP32 / STM32) codec

This folder contains:
- `myco_cbor.c/.h` : tiny CBOR writer for integers, maps, arrays, byte/text strings
- `myco_envelope.c/.h` : builds the envelope, computes hash, creates Ed25519 signature
- `example_main.c` : example usage

## Dependencies (recommended)

### Ed25519 + BLAKE2b
We recommend **Monocypher** because it includes:
- Ed25519 sign/verify
- BLAKE2b (we use BLAKE2b-256)

Docs: https://monocypher.org/manual/

You need to add these files (from Monocypher) to your build:
- `monocypher.h`
- `monocypher.c`

and enable the optional Ed25519 header:
- `optional/monocypher-ed25519.h`

Alternatively, you can adapt `myco_crypto_*` hooks to libsodium, TweetNaCl, etc.

### ESP32 (ESP-IDF)
- Add Monocypher as a component, or compile it into your project.
- Use the example function calls directly.

### STM32
- Compile Monocypher as part of your project (works with GCC/Clang).
- If you have hardware RNG, use it to generate keys (or provision keys at manufacturing).

## Key provisioning
- Private key is 64 bytes for Monocypher signing (`crypto_ed25519_sign` expects 64-byte secret key).
- Public key is 32 bytes.

You can generate keys using `tools/gen_keys.mjs` and provision them securely.

