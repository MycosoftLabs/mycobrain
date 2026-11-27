# AWS infrastructure reference and legacy file register

Verified 2026-11-27; account 376129856177, us-east-1. Production servers are AWS-only. This source branch is not deployed. Canonical system map: MycosoftLabs/mycosoft-mas root `MYCOSOFT_AWS_INFRA_CURRENT.md`.

Source base: `40b315b9c71e4b7ea8c36a36b2f87fb576b445eb`. Supplied Grok patches are rebased by exact hunk context/three-way merge; unavailable MycoBrain base ef93c57 was reconciled against current main using complete matching context. Historical code bodies remain present.

## Configuration

MINDEX: `http://mindex-internal.mycosoft.com:8000`; MAS: `http://mas-internal.mycosoft.com:8001`; MycoBrain: `http://mycobrain-internal.mycosoft.com:8003`. Mycorrhizae uses `http://internal-mycosoft-core-int-21300503.us-east-1.elb.amazonaws.com:8002`; n8n MAS/MYCA use the same internal ALB ports5678/5679. Website is `https://mycosoft.com`; Origin is `https://origin-aws.mycosoft.com:8443`. These private backends are server-side only. Never put service secrets into NEXT_PUBLIC variables. Database access uses Secrets Manager application credentials and the TLS-verified RDS Proxy; see the canonical map.

Unknown optional LAN/hypervisor/device integrations need explicit configuration and separate verification; a retained literal in those bodies is not a discovered AWS replacement. Tests, source datasets, migrations and signed records retain evidence. Retired CLI guards only stop the standalone old SSH/Proxmox utility; importable library bodies remain. AWS-aware utilities were preserved. Production AWS services were not restarted or redeployed.

## GitHub workflow inventory

- `.github/workflows/ci.yml`: triggers `push:`, `pull_request:`; runners `ubuntu-latest`, `ubuntu-latest`; actions `actions/checkout@v4`, `actions/setup-python@v5`.
- `.github/workflows/cve-2026-31431-runner-precheck.yml`: triggers `workflow_call: {}`, `workflow_dispatch: {}`; runners `self-hosted`; actions .

## Complete matching-file register

Rows are a reviewed inventory, not evidence that an optional connector is used in production. Each replacement either points to the verified AWS service, retires the local topology instruction, or preserves a non-current evidence/configuration record.

| File | Disposition | Replacement / retained-reference meaning |
|---|---|---|
| `MQTT/CEO_DEPLOY_GUIDE.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `MQTT/deploy-mqtt-prod-vm.ps1` | KEEP / RETIRED_LOCAL_REFERENCE_OR_OPTIONAL_EXTERNAL_CONNECTOR | Retired local topology; use the verified AWS service map. No AWS counterpart is asserted for physical device fixtures, archival records, or optional hypervisor/network connectors. |
| `MQTT/deploy_mqtt_prod_guest.py` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `WHATS_NEW_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `agents/deploy/env/jetson_orin.env.example` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `agents/deploy/env/raspberry_pi.env.example` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/FCI_ENDPOINT_OPTIONS_FEB10_2026.md` | AWS / LEGACY_DOCUMENT_CORRECTED | http://192.168.0.188:8001 -> http://mas-internal.mycosoft.com:8001 |
| `docs/FIRMWARE_ARCHITECTURE_FEB10_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/GATEWAY_JETSON4_LILYGO_ARCHITECTURE_MAR07_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/JETSON_FIRMWARE_IMPLEMENTATION_GUIDE_MAR07_2026.md` | AWS / LEGACY_DOCUMENT_CORRECTED | http://192.168.0.187:8002 -> http://internal-mycosoft-core-int-21300503.us-east-1.elb.amazonaws.com:8002 |
| `docs/JETSON_MYCOBRAIN_HARDWARE_PLAN_MAR09_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/MQTT_TOPIC_SCHEMA_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/MYCOBRAIN_NATUREOS_DEVICE_MANAGER_IMPLEMENTATION_MAY21.md` | AWS / LEGACY_DOCUMENT_CORRECTED | http://192.168.0.196:8003 -> http://mycobrain-internal.mycosoft.com:8003 |
| `docs/MYCOSOFT_ECOSYSTEM_LEARNING_SUMMARY.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/RASPBERRY_PI_ADAPTER_GUIDE_MAY19_2026.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `docs/features/WiFiSense-Capability-Spec.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `mycobrain/devices/README.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `mycobrain/devices/_template/README.md` | RET / RETIRED_LOCAL_REFERENCE_OR_OPTIONAL_EXTERNAL_CONNECTOR | Retired local topology; use the verified AWS service map. No AWS counterpart is asserted for physical device fixtures, archival records, or optional hypervisor/network connectors. |
| `mycobrain/devices/mycobrain-jet-a/README.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `mycobrain/devices/mycobrain-jet-a/deploy.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `mycobrain/devices/mycobrain-pi-a/README.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `mycobrain/devices/mycobrain-pi-a/deploy.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `natureos/OPENCLAW_LIVE_CONTROL.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `natureos/WEBSITE_IMPLEMENTATION_SPEC.md` | AWS / LEGACY_DOCUMENT_CORRECTED | http://192.168.0.196:8003 -> http://mycobrain-internal.mycosoft.com:8003 |
| `natureos/api-contract.openapi.yaml` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `natureos/ui-flows.md` | RET / LEGACY_DOCUMENT_CORRECTED | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `scripts/probe-jetson.sh` | RET / RETIRED_LOCAL_DEPLOY_UTILITY | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `tools/python/auto_probe_all.py` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
| `tools/python/fleet_status.py` | RET / HISTORICAL_OR_EXTERNAL_REFERENCE | Use the AWS service/Secrets Manager configuration matrix; no LAN/VM/NAS production target. Retained text is historical, fixture data or an optional external connector. |
