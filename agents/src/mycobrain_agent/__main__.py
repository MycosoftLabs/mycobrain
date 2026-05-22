"""Entry point: ``python -m mycobrain_agent``.

Wires together the configured adapter, core services, and the HTTP server.
This module deliberately keeps almost no logic — it composes singletons and
hands control to uvicorn. See the modules under ``core/``, ``adapters/``,
``http/`` for the real code.
"""

from __future__ import annotations

import asyncio
import logging
import signal

import structlog
import uvicorn

from mycobrain_agent.adapters import load_adapter
from mycobrain_agent.config import Settings
from mycobrain_agent.core.heartbeat import HeartbeatService
from mycobrain_agent.core.mqtt_client import MqttClient
from mycobrain_agent.core.registry import DeviceRegistry
from mycobrain_agent.core.serial_bridge import SerialBridge
from mycobrain_agent.http.server import build_app
from mycobrain_agent.openclaw.client import OpenClawClient


def _configure_logging(settings: Settings) -> None:
    logging.basicConfig(level=settings.log_level.upper())
    structlog.configure(
        wrapper_class=structlog.make_filtering_bound_logger(
            getattr(logging, settings.log_level.upper(), logging.INFO)
        ),
        processors=[
            structlog.processors.add_log_level,
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.processors.JSONRenderer(),
        ],
    )


async def _run(settings: Settings) -> None:
    log = structlog.get_logger("mycobrain_agent")
    log.info("starting", adapter=settings.adapter, http_port=settings.http_port)

    adapter = load_adapter(settings)
    registry = DeviceRegistry(adapter=adapter, settings=settings)
    serial_bridge = SerialBridge(adapter=adapter, registry=registry, settings=settings)
    mqtt = MqttClient(registry=registry, settings=settings)
    openclaw = OpenClawClient(settings=settings)
    heartbeat = HeartbeatService(registry=registry, settings=settings)

    services = [serial_bridge, mqtt, heartbeat]

    # Wire HTTP app
    app = build_app(
        settings=settings,
        registry=registry,
        serial_bridge=serial_bridge,
        mqtt=mqtt,
        openclaw=openclaw,
    )

    config = uvicorn.Config(
        app,
        host=settings.http_host,
        port=settings.http_port,
        log_level=settings.log_level.lower(),
        access_log=False,
        lifespan="on",
    )
    server = uvicorn.Server(config)

    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        log.info("shutdown_requested")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            # Windows: signal handlers via add_signal_handler are unsupported.
            pass

    # Start background services
    bg_tasks = [asyncio.create_task(svc.run(), name=svc.__class__.__name__) for svc in services]

    # Run HTTP server until stop_event or server completes
    server_task = asyncio.create_task(server.serve(), name="uvicorn")

    done, pending = await asyncio.wait(
        [server_task, asyncio.create_task(stop_event.wait())],
        return_when=asyncio.FIRST_COMPLETED,
    )

    log.info("stopping")
    server.should_exit = True
    for task in bg_tasks + list(pending):
        task.cancel()
    await asyncio.gather(*bg_tasks, return_exceptions=True)
    await asyncio.gather(*pending, return_exceptions=True)
    log.info("stopped")


def main() -> None:
    settings = Settings()
    _configure_logging(settings)
    asyncio.run(_run(settings))


if __name__ == "__main__":
    main()
