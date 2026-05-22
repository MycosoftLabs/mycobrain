# `mycobrain/devices/` — Per-Device Source of Truth

One folder per physical MycoBrain in operation. The folder holds:

- `README.md` — identity, role, host, current firmware build, link to NatureOS device card
- `deploy.md` — SSH/flash/install runbook specific to this device
- `calibration.json` — saved calibration profile (offsets, gains, pin remap)
- `status.md` — current operational state, last probe result, owner contact
- `secrets.env.example` — env template (passwords / API keys redacted)

## Active devices (2026-05-21)

| Folder | device_id | Host | Status |
|--------|-----------|------|--------|
| [`mycobrain-jet-a/`](mycobrain-jet-a/) | mycobrain-jet-a-001 | Jetson at `192.168.0.228` | LIVE |
| [`mycobrain-pi-a/`](mycobrain-pi-a/) | mycobrain-pi-a-001 | Pi at `192.168.0.123` | LIVE |
| [`mycobrain-jet-b/`](mycobrain-jet-b/) | mycobrain-jet-b-001 | older Jetson, offline | DEAD — cold-boot pending |
| [`mycobrain-bench/`](mycobrain-bench/) | mycobrain-bench-001 | Windows PC via COM4 | LIVE — legacy JSON firmware |

## Adding a new device

```bash
cp -r mycobrain/devices/_template mycobrain/devices/mycobrain-<nickname>
# edit README.md → identity
# edit deploy.md → host-specific bring-up
# pair with NatureOS via the wizard at mycosoft.com/natureos/devices?pair=1
```

The pairing wizard issues a long-lived JWT, drops it into the agent's `/etc/mycobrain/agent.env`, and the device shows up on the fleet grid within 30s.

## Source of truth

Within this folder hierarchy:

- The **device's identity** comes from Side A's HELLO frame (`device_id`, `role`, fw version). The `README.md` here records that snapshot.
- The **NatureOS device card** at `mycosoft.com/natureos/devices/<device_id>` is the live view. This folder is what humans (and Claude) read when planning ops.
- When a device dies / is retired, archive the folder under `_retired/` rather than deleting — the audit chain stays intact.
