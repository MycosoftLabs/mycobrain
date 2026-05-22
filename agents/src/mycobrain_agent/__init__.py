"""mycobrain-agent — unified host agent for every MycoBrain device.

One Python package runs on Jetson Orin, older Jetson, Raspberry Pi, or a
standalone PC over USB. Hardware-specific behavior lives in
``mycobrain_agent.adapters``. Everything else — MDP codec, MQTT client,
HTTP API on port 8787, OpenClaw proxy — is shared.
"""

__version__ = "1.0.0"
