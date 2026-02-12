package main

import (
	"bytes"
	"context"
	"crypto/ed25519"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/Azure/azure-kusto-go/azkustodata"
	"github.com/Azure/azure-kusto-go/azkustoingest"
	"github.com/Azure/azure-sdk-for-go/sdk/messaging/azeventhubs/v2"
	"github.com/fxamacker/cbor/v2"
	"golang.org/x/crypto/blake2b"
)

type Config struct {
	EventHubConnStr string
	EventHubName    string
	ConsumerGroup   string

	RegistryPath string

	ADXIngestURI  string
	ADXDatabase   string
	ADXRawTable   string
	ADXRawMapping string

	TenantID     string
	ClientID     string
	ClientSecret string

	BatchMaxEvents int
	BatchMaxMS     time.Duration
	DedupTTL       time.Duration
}

func mustEnv(k string) string {
	v := os.Getenv(k)
	if v == "" {
		log.Fatalf("missing env var %s", k)
	}
	return v
}

func parseEntityPath(connStr string) string {
	parts := strings.Split(connStr, ";")
	for _, p := range parts {
		if strings.HasPrefix(p, "EntityPath=") {
			return strings.TrimPrefix(p, "EntityPath=")
		}
	}
	return ""
}

func loadConfig() Config {
	cfg := Config{
		EventHubConnStr: mustEnv("IOTHUB_EVENTHUB_CONNECTION_STRING"),
		EventHubName:    os.Getenv("EVENTHUB_NAME"),
		ConsumerGroup:   os.Getenv("EVENTHUB_CONSUMER_GROUP"),
		RegistryPath:    mustEnv("DEVICE_REGISTRY_PATH"),
		ADXIngestURI:    mustEnv("ADX_INGEST_URI"),
		ADXDatabase:     mustEnv("ADX_DATABASE"),
		ADXRawTable:     mustEnv("ADX_RAW_TABLE"),
		ADXRawMapping:   mustEnv("ADX_RAW_MAPPING"),
		TenantID:        mustEnv("ADX_TENANT_ID"),
		ClientID:        mustEnv("ADX_CLIENT_ID"),
		ClientSecret:    mustEnv("ADX_CLIENT_SECRET"),
		BatchMaxEvents:  200,
		BatchMaxMS:      2 * time.Second,
		DedupTTL:        48 * time.Hour,
	}
	if cfg.ConsumerGroup == "" {
		cfg.ConsumerGroup = azeventhubs.DefaultConsumerGroup
	}
	if cfg.EventHubName == "" {
		cfg.EventHubName = parseEntityPath(cfg.EventHubConnStr)
	}
	if v := os.Getenv("BATCH_MAX_EVENTS"); v != "" {
		fmt.Sscanf(v, "%d", &cfg.BatchMaxEvents)
	}
	if v := os.Getenv("BATCH_MAX_MS"); v != "" {
		var ms int
		fmt.Sscanf(v, "%d", &ms)
		cfg.BatchMaxMS = time.Duration(ms) * time.Millisecond
	}
	if v := os.Getenv("DEDUP_TTL_MS"); v != "" {
		var ms int
		fmt.Sscanf(v, "%d", &ms)
		cfg.DedupTTL = time.Duration(ms) * time.Millisecond
	}
	return cfg
}

func loadRegistry(path string) (map[string][]byte, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	// Support either { "dev":"b64" } or { "dev":{ "publicKeyB64":"..." } }
	var raw map[string]any
	if err := json.Unmarshal(b, &raw); err != nil {
		return nil, err
	}
	out := map[string][]byte{}
	for dev, v := range raw {
		switch t := v.(type) {
		case string:
			pk, err := base64.StdEncoding.DecodeString(t)
			if err != nil {
				return nil, err
			}
			out[dev] = pk
		case map[string]any:
			s, _ := t["publicKeyB64"].(string)
			pk, err := base64.StdEncoding.DecodeString(s)
			if err != nil {
				return nil, err
			}
			out[dev] = pk
		default:
			continue
		}
	}
	return out, nil
}

// Envelope uses numeric keys. We'll decode into map[any]any for CBOR.
type Envelope map[any]any

func blake2b256(data []byte) []byte {
	sum := blake2b.Sum256(data)
	return sum[:]
}

func canonicalUnsignedCBOR(env Envelope) ([]byte, error) {
	// clone without keys 10 and 11
	unsigned := make(map[any]any, len(env))
	for k, v := range env {
		if ki, ok := k.(uint64); ok {
			if ki == 10 || ki == 11 {
				continue
			}
		}
		unsigned[k] = v
	}

	encOpts := cbor.CoreDetEncOptions() // deterministic encoding
	em, err := encOpts.EncMode()
	if err != nil {
		return nil, err
	}
	return em.Marshal(unsigned)
}

func verifyEnvelope(env Envelope, pubKey []byte) (bool, string) {
	hVal, ok := env[uint64(10)]
	if !ok {
		return false, "missing hash"
	}
	sigVal, ok := env[uint64(11)]
	if !ok {
		return false, "missing signature"
	}

	h, ok := hVal.([]byte)
	if !ok || len(h) != 32 {
		return false, "bad hash type/len"
	}
	sig, ok := sigVal.([]byte)
	if !ok || len(sig) != 64 {
		return false, "bad sig type/len"
	}

	unsignedBytes, err := canonicalUnsignedCBOR(env)
	if err != nil {
		return false, "cbor re-encode failed"
	}
	h2 := blake2b256(unsignedBytes)
	if !bytes.Equal(h, h2) {
		return false, "hash mismatch"
	}

	msg := append([]byte("MYCO1"), h2...)
	if !ed25519.Verify(ed25519.PublicKey(pubKey), msg, sig) {
		return false, "bad signature"
	}
	return true, ""
}

func uuidBytesToString(b []byte) string {
	if len(b) != 16 {
		return base64.StdEncoding.EncodeToString(b)
	}
	hex := fmt.Sprintf("%x", b)
	return fmt.Sprintf("%s-%s-%s-%s-%s", hex[0:8], hex[8:12], hex[12:16], hex[16:20], hex[20:32])
}

func toVerbose(env Envelope) map[string]any {
	dev, _ := env[uint64(1)].(string)
	protoU, _ := env[uint64(2)].(uint64)
	msgID, _ := env[uint64(3)].([]byte)
	tMs, _ := env[uint64(4)].(int64)
	seq, _ := env[uint64(5)].(uint64)
	mono, _ := env[uint64(6)].(uint64)

	var geo map[string]any
	if gAny, ok := env[uint64(7)]; ok {
		if g, ok := gAny.(map[any]any); ok {
			lat := int64(0)
			lon := int64(0)
			acc := uint64(0)
			if v, ok := g[uint64(0)].(int64); ok {
				lat = v
			}
			if v, ok := g[uint64(1)].(int64); ok {
				lon = v
			}
			if v, ok := g[uint64(2)].(uint64); ok {
				acc = v
			}
			geo = map[string]any{
				"lat":    float64(lat) / 1e7,
				"lon":    float64(lon) / 1e7,
				"lat_e7": lat,
				"lon_e7": lon,
				"acc_m":  acc,
			}
		}
	}

	pack := []any{}
	if rAny, ok := env[uint64(8)]; ok {
		if arr, ok := rAny.([]any); ok {
			for _, item := range arr {
				rm, ok := item.(map[any]any)
				if !ok {
					continue
				}
				sid := rm[uint64(0)]
				vi := rm[uint64(1)]
				vs := rm[uint64(2)]
				u := rm[uint64(3)]
				q := rm[uint64(4)]
				// compute v if possible
				var vFloat any = nil
				viI, ok1 := vi.(int64)
				vsU, ok2 := vs.(uint64)
				if ok1 && ok2 {
					vFloat = float64(viI) / float64(pow10(int(vsU)))
				}
				pack = append(pack, map[string]any{
					"id": sid, "vi": vi, "vs": vs, "v": vFloat, "u": u, "q": q,
				})
			}
		}
	}

	var hashB64, sigB64 string
	if h, ok := env[uint64(10)].([]byte); ok {
		hashB64 = base64.StdEncoding.EncodeToString(h)
	}
	if s, ok := env[uint64(11)].([]byte); ok {
		sigB64 = base64.StdEncoding.EncodeToString(s)
	}

	return map[string]any{
		"hdr": map[string]any{
			"deviceId":  dev,
			"proto":     protoName(uint8(protoU)),
			"msgId":     uuidBytesToString(msgID),
			"msgId_b64": base64.StdEncoding.EncodeToString(msgID),
		},
		"ts": map[string]any{
			"ms":      tMs,
			"utc":     time.UnixMilli(tMs).UTC().Format(time.RFC3339Nano),
			"mono_ms": mono,
		},
		"seq":      seq,
		"geo":      geo,
		"pack":     pack,
		"meta":     env[uint64(9)],
		"hash_b64": hashB64,
		"sig_b64":  sigB64,
		"_compact": true,
		"v":        env[uint64(0)],
	}
}

func pow10(n int) int64 {
	p := int64(1)
	for i := 0; i < n; i++ {
		p *= 10
	}
	return p
}

func protoName(p uint8) string {
	switch p {
	case 1:
		return "lorawan"
	case 2:
		return "mqtt"
	case 3:
		return "ble"
	case 4:
		return "lte"
	default:
		return "other"
	}
}

// naive TTL dedupe
type Deduper struct {
	mu  sync.Mutex
	m   map[string]time.Time
	ttl time.Duration
}

func NewDeduper(ttl time.Duration) *Deduper {
	return &Deduper{m: map[string]time.Time{}, ttl: ttl}
}

func (d *Deduper) Seen(key string) bool {
	d.mu.Lock()
	defer d.mu.Unlock()
	now := time.Now()
	if len(d.m) > 500000 {
		for k, t := range d.m {
			if now.Sub(t) > d.ttl {
				delete(d.m, k)
			}
		}
	}
	if t, ok := d.m[key]; ok {
		if now.Sub(t) <= d.ttl {
			return true
		}
	}
	d.m[key] = now
	return false
}

func mkIngestClient(cfg Config) (*azkustoingest.Ingestion, error) {
	kcsb := azkustodata.NewConnectionStringBuilder(cfg.ADXIngestURI).
		WithAadAppKey(cfg.ClientID, cfg.ClientSecret, cfg.TenantID)

	ing, err := azkustoingest.New(kcsb)
	if err != nil {
		return nil, err
	}
	return ing, nil
}

func ingestRawBatch(ctx context.Context, ing *azkustoingest.Ingestion, cfg Config, records []map[string]any) error {
	if len(records) == 0 {
		return nil
	}

	// MultiJSON: each line is one JSON record
	var buf bytes.Buffer
	for i, r := range records {
		if i > 0 {
			buf.WriteByte('\n')
		}
		b, _ := json.Marshal(r)
		buf.Write(b)
	}

	_, err := ing.FromReader(ctx, &buf,
		azkustoingest.Database(cfg.ADXDatabase),
		azkustoingest.Table(cfg.ADXRawTable),
		azkustoingest.FileFormat(azkustoingest.MultiJSON),
		azkustoingest.IngestionMappingRef(cfg.ADXRawMapping, azkustoingest.JSON),
	)
	return err
}

func main() {
	cfg := loadConfig()
	if cfg.EventHubName == "" {
		log.Fatal("Could not determine Event Hub name. Set EVENTHUB_NAME or ensure EntityPath exists in connection string.")
	}

	reg, err := loadRegistry(cfg.RegistryPath)
	if err != nil {
		log.Fatal(err)
	}

	ing, err := mkIngestClient(cfg)
	if err != nil {
		log.Fatal(err)
	}
	defer ing.Close()

	ctx := context.Background()

	consumer, err := azeventhubs.NewConsumerClientFromConnectionString(cfg.EventHubConnStr, cfg.EventHubName, cfg.ConsumerGroup, nil)
	if err != nil {
		log.Fatal(err)
	}
	defer consumer.Close(ctx)

	props, err := consumer.GetEventHubProperties(ctx, nil)
	if err != nil {
		log.Fatal(err)
	}

	deduper := NewDeduper(cfg.DedupTTL)

	batch := make([]map[string]any, 0, cfg.BatchMaxEvents)
	var batchMu sync.Mutex

	flush := func() {
		batchMu.Lock()
		toSend := batch
		batch = make([]map[string]any, 0, cfg.BatchMaxEvents)
		batchMu.Unlock()

		if len(toSend) == 0 {
			return
		}

		ingCtx, cancel := context.WithTimeout(ctx, 2*time.Minute)
		defer cancel()

		if err := ingestRawBatch(ingCtx, ing, cfg, toSend); err != nil {
			log.Printf("ADX ingest error: %v", err)
			return
		}
		log.Printf("Ingested raw batch: %d", len(toSend))
	}

	ticker := time.NewTicker(cfg.BatchMaxMS)
	defer ticker.Stop()

	// Start one goroutine per partition (simple). For large hubs, replace with azeventhubs.Processor.
	for _, pid := range props.PartitionIDs {
		pid := pid
		go func() {
			pc, err := consumer.NewPartitionClient(pid, &azeventhubs.PartitionClientOptions{
				StartPosition: azeventhubs.StartPosition{Latest: true},
			})
			if err != nil {
				log.Printf("partition %s: %v", pid, err)
				return
			}
			defer pc.Close(ctx)

			for {
				recvCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
				events, err := pc.ReceiveEvents(recvCtx, 100, nil)
				cancel()
				if err != nil {
					if azeventhubs.IsError(err, azeventhubs.ErrorCodeTimeout) {
						continue
					}
					log.Printf("receive err partition %s: %v", pid, err)
					time.Sleep(time.Second)
					continue
				}

				for _, ev := range events {
					body := ev.Body
					var env Envelope
					dec := cbor.NewDecoder(bytes.NewReader(body))
					if err := dec.Decode(&env); err != nil {
						continue
					}

					dev, _ := env[uint64(1)].(string)
					msgID, _ := env[uint64(3)].([]byte)
					dedupKey := dev + ":" + base64.StdEncoding.EncodeToString(msgID)
					if deduper.Seen(dedupKey) {
						continue
					}

					pub := reg[dev]
					if len(pub) != ed25519.PublicKeySize {
						continue
					}

					ok, reason := verifyEnvelope(env, pub)
					if !ok {
						log.Printf("bad env %s: %s", dev, reason)
						continue
					}

					verbose := toVerbose(env)
					raw := map[string]any{
						"ingestedAt": time.Now().UTC().Format(time.RFC3339Nano),
						"enqueuedTime": func() any {
							if ev.EnqueuedTime != nil {
								return ev.EnqueuedTime.UTC().Format(time.RFC3339Nano)
							}
							return nil
						}(),
						"deviceId": dev,
						"msgId":    verbose["hdr"].(map[string]any)["msgId"],
						"proto":    verbose["hdr"].(map[string]any)["proto"],
						"seq":      verbose["seq"],
						"timeUtc":  verbose["ts"].(map[string]any)["utc"],
						"monoMs":   verbose["ts"].(map[string]any)["mono_ms"],
						"lat": func() any {
							if verbose["geo"] == nil {
								return nil
							}
							return verbose["geo"].(map[string]any)["lat"]
						}(),
						"lon": func() any {
							if verbose["geo"] == nil {
								return nil
							}
							return verbose["geo"].(map[string]any)["lon"]
						}(),
						"accM": func() any {
							if verbose["geo"] == nil {
								return nil
							}
							return verbose["geo"].(map[string]any)["acc_m"]
						}(),
						"body":       verbose,
						"rawCborB64": base64.StdEncoding.EncodeToString(body),
						"hashB64":    verbose["hash_b64"],
						"sigB64":     verbose["sig_b64"],
					}

					batchMu.Lock()
					batch = append(batch, raw)
					shouldFlush := len(batch) >= cfg.BatchMaxEvents
					batchMu.Unlock()
					if shouldFlush {
						flush()
					}
				}
			}
		}()
	}

	log.Printf("Consuming partitions: %v", props.PartitionIDs)

	for range ticker.C {
		flush()
	}
}
