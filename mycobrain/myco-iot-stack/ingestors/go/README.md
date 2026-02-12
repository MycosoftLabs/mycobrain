# Go ingestor (IoT Hub built-in endpoint -> verify -> ADX)

This is a parallel implementation to the Node ingestor.

It uses:
- `github.com/Azure/azure-sdk-for-go/sdk/messaging/azeventhubs/v2` to consume from Event Hubs-compatible endpoint
- `github.com/fxamacker/cbor/v2` for CBOR decode + deterministic re-encode
- `crypto/ed25519` for signature verification
- `github.com/Azure/azure-kusto-go/azkustoingest` for ADX ingestion

## Run
```bash
go mod tidy
go run .
```

## Env
See `env.example`.

