# MycoBrain Jetson Deploy (Reference)

**Date:** March 7, 2026  
**Canonical Source:** MAS repo `deploy/jetson/` (mycosoft-mas) — deploy scripts, systemd units, env examples live in MAS.

---

## Purpose

This directory documents Jetson deployment for MycoBrain firmware. **Actual deployment is done from the MAS repo**, which contains:

- `install_jetson_services.sh` — installs on-device operator or gateway router
- `mycobrain-ondevice-operator.service` — systemd unit for Jetson 16GB
- `mycobrain-gateway-router.service` — systemd unit for Jetson 4GB
- `ondevice-operator.env.example` — env for on-device operator
- `gateway-router.env.example` — env for gateway router

---

## Deploy Flow

1. **Clone MAS repo** to Jetson: `/opt/mycosoft/mas`
2. **Flash MycoBrain firmware** (from mycobrain repo): Side A + Side B
3. **Install services** from MAS:
   ```bash
   sudo bash /opt/mycosoft/mas/deploy/jetson/install_jetson_services.sh ondevice
   # or
   sudo bash /opt/mycosoft/mas/deploy/jetson/install_jetson_services.sh gateway
   ```
4. **Configure env** at `/etc/mycosoft/ondevice-operator.env` or `gateway-router.env`

---

## Device Roles and Jetson Tiers

| Tier | Device | Jetson | Service |
|------|--------|--------|---------|
| 1 | Mushroom 1 | Jetson 16GB (Orin NX Super) | `mycobrain-ondevice-operator.service` |
| 2 | Hyphae 1 | Jetson Nano/Xavier NX | `mycobrain-ondevice-operator.service` |
| 3 | Gateway | Jetson Nano 4GB | `mycobrain-gateway-router.service` |

---

## Related Docs (mycobrain repo)

- [Jetson Firmware Implementation Guide](../docs/JETSON_FIRMWARE_IMPLEMENTATION_GUIDE_MAR07_2026.md)
- [Jetson Production Deploy](../docs/JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md)
- [Gateway Build Plan](../docs/MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md)
- [Firmware and Jetson Index](../docs/FIRMWARE_AND_JETSON_INDEX_MAR07_2026.md)
