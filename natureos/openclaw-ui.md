# OpenClaw Control Panel — UI Spec

For `mycosoft.com/natureos/devices/{id}` when `info.openclaw.available === true`.

Sequence and contracts: see `../docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`.

## Panel layout

```
┌───────────────────────────────────────────────────────────────────────┐
│  OpenClaw                                            [● Ready] [Estop]│
├───────────────────────────────────────────────────────────────────────┤
│  Position                                                              │
│  joint_1 ▓▓▓▓▓░░░░░  0.34 rad                                          │
│  joint_2 ▓▓░░░░░░░░ -0.12 rad                                          │
│  Gripper [Closed]   Holding [no]                                       │
├───────────────────────────────────────────────────────────────────────┤
│  Quick actions                                                         │
│  [ Home ]  [ Open ]  [ Close ]  [ Grasp ]  [ Release ]                 │
│                                                                        │
│  Calibrate ▾   Joint control ▾                                         │
├───────────────────────────────────────────────────────────────────────┤
│  Recent actions                                                        │
│  14:18:30  morgan@   grasp (force 5N)     2.0s   ✓                     │
│  14:14:01  morgan@   home                 1.1s   ✓                     │
│  ...                                                                   │
└───────────────────────────────────────────────────────────────────────┘
```

## Live data

The panel subscribes to two streams server-side, multiplexed over `WS /api/devices/{id}/stream`:

- `mycosoft/devices/{id}/openclaw/state` (retained) for joint position + holding state
- `mycosoft/devices/{id}/openclaw/action` for the audit timeline

On initial render the panel calls `GET /api/devices/{id}/openclaw/status` (proxies to agent `:8787/openclaw/status`) once.

## Buttons → API mapping

| Button | Call |
|--------|------|
| Home | `POST /api/devices/{id}/openclaw/action` body `{action:"home"}` |
| Open | `…body {action:"open"}` |
| Close | `…body {action:"close", params:{force_n:5}}` (force from a control on the panel) |
| Grasp | `…body {action:"grasp", params:{force_n:5, timeout_ms:4000}}` |
| Release | `…body {action:"release"}` |
| Estop | `…body {action:"estop"}` then panel locks |
| Clear Estop | `…body {action:"clear_estop"}` (only enabled while red banner is up) |

## States

| State | Visual |
|-------|--------|
| Ready | Green dot, all buttons enabled |
| Busy | Spinning indicator on the action button, others greyed |
| Estop latched | Red banner over panel: "Estop active. [Clear estop]" — only Clear is enabled |
| Unavailable | Panel hidden entirely (set by `info.openclaw.available === false`) |
| OpenClaw unreachable | Yellow banner: "Claw process not responding. Last seen ##s ago." |

## Permissions

- Read state — any authenticated NatureOS user
- Action — requires `openclaw:action` scope on the operator's JWT (granted to ops + engineering)
- Calibrate — requires `openclaw:admin` scope (granted to engineering only)

## Optional advanced drawer

- Live joint sliders that POST `move_to` actions on release (rate-limited)
- ONNX model swap (if the host has CUDA inference for predictive grasp)
- Force-feedback curve viewer

These are gated by `host_kind === "jetson_orin"` since they're only fast enough there.
