"""Paho-based async MQTT client publishing the canonical topic schema.

Topics defined in ``docs/MQTT_TOPIC_SCHEMA_MAY19_2026.md``. Connects via
WSS in prod (Cloudflare-fronted) or plain MQTT on LAN.
"""

from __future__ import annotations

import asyncio
import json
import socket
from typing import TYPE_CHECKING
from urllib.parse import urlparse

import structlog
from paho.mqtt import client as mqtt

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.registry import DeviceRegistry

log = structlog.get_logger("mqtt_client")


class MqttClient:
    def __init__(self, registry: "DeviceRegistry", settings: "Settings") -> None:
        self.registry = registry
        self.settings = settings
        self.client = mqtt.Client(
            client_id=settings.resolved_mqtt_client_id,
            protocol=mqtt.MQTTv5,
            transport="websockets" if settings.mqtt_url.startswith("ws") else "tcp",
        )
        if settings.mqtt_password:
            self.client.username_pw_set(settings.mqtt_username, settings.mqtt_password)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        self._loop_task: asyncio.Task[None] | None = None

    async def run(self) -> None:
        url = urlparse(self.settings.mqtt_url)
        scheme = url.scheme.lower()
        host = url.hostname or "localhost"
        port = url.port or (443 if scheme in {"wss", "https"} else 1883)
        will_topic = f"mycosoft/devices/{self.registry.device_id}/presence"
        will_payload = json.dumps(
            {
                "device_id": self.registry.device_id,
                "online": False,
                "last_seen": _now_iso(),
            }
        )
        self.client.will_set(will_topic, will_payload, qos=1, retain=True)
        if scheme in {"wss", "https"}:
            self.client.tls_set()
        try:
            self.client.connect(host, port, keepalive=30)
        except (OSError, socket.gaierror) as exc:
            log.warning("mqtt_connect_failed", host=host, port=port, error=str(exc))
            await asyncio.sleep(10)
            return await self.run()

        loop = asyncio.get_running_loop()
        self._loop_task = loop.run_in_executor(None, self.client.loop_forever)
        await self._loop_task  # type: ignore[func-returns-value]

    async def publish_presence(self) -> None:
        topic = f"mycosoft/devices/{self.registry.device_id}/presence"
        info = self.registry.info()
        status = self.registry.status()
        payload = {
            "device_id": self.registry.device_id,
            "host_kind": info["host_kind"],
            "host_ip": self.settings.host_ip,
            "agent_url": f"http://{self.settings.host_ip}:{self.settings.http_port}" if self.settings.host_ip else None,
            "agent_version": info["agent_version"],
            "side_a_fw": info["side_a"]["fw_version"],
            "side_b_fw": info["side_b"]["fw_version"],
            "openclaw_available": self.settings.openclaw_enabled,
            "online": True,
            "last_seen": _now_iso(),
            "status": status,
        }
        self.client.publish(topic, json.dumps(payload), qos=1, retain=True)

    async def publish_telemetry(self, payload: dict) -> None:
        topic = f"mycosoft/devices/{self.registry.device_id}/telemetry"
        self.client.publish(topic, json.dumps(payload), qos=0)

    async def publish_event(self, payload: dict) -> None:
        topic = f"mycosoft/devices/{self.registry.device_id}/events"
        self.client.publish(topic, json.dumps(payload), qos=1)

    async def publish_openclaw_state(self, payload: dict) -> None:
        topic = f"mycosoft/devices/{self.registry.device_id}/openclaw/state"
        self.client.publish(topic, json.dumps(payload), qos=1, retain=True)

    # ---- Paho callbacks ----

    def _on_connect(self, client, userdata, flags, reason_code, properties=None) -> None:
        log.info("mqtt_connected", reason=str(reason_code))
        client.subscribe(f"mycosoft/devices/{self.registry.device_id}/cmd", qos=1)

    def _on_disconnect(self, client, userdata, reason_code, properties=None) -> None:
        log.warning("mqtt_disconnected", reason=str(reason_code))

    def _on_message(self, client, userdata, msg) -> None:
        log.info("mqtt_cmd_received", topic=msg.topic, len=len(msg.payload))
        # Inbound commands handled by the HTTP layer's command processor;
        # post-MVP we wire this into a shared queue.


def _now_iso() -> str:
    import datetime as _dt

    return _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
