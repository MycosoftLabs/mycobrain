"""Inert orchestration and OpenClaw replies; no adapters, serial, MQTT or HTTP."""
import asyncio
import importlib.util
import json
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
ENTRY = ROOT / "src/mycobrain_agent/__main__.py"
CLIENT = ROOT / "src/mycobrain_agent/openclaw/client.py"


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class SyntaxTests(unittest.TestCase):
    def test_entrypoint_compiles(self):
        compile(ENTRY.read_text(), str(ENTRY), "exec")

    def test_openclaw_compiles(self):
        compile(CLIENT.read_text(), str(CLIENT), "exec")


class FakeBridge:
    def __init__(self, payload=None, linked=True, missing_frame=False):
        self.registry = types.SimpleNamespace(record=types.SimpleNamespace(
            side_a=types.SimpleNamespace(linked=linked)))
        self.payload, self.missing_frame, self.calls = payload, missing_frame, []

    async def send_command(self, **kwargs):
        self.calls.append(kwargs)
        return None if self.missing_frame else types.SimpleNamespace(payload=self.payload)


class OpenClawTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.module = load(CLIENT, "openclaw_recovery_fixture")
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.settings = types.SimpleNamespace(openclaw_enabled=True,
            openclaw_audit_path=str(Path(self.temp.name) / "audit.jsonl"))

    def client(self, bridge=None):
        return self.module.OpenClawClient(self.settings, serial_bridge=bridge)

    async def test_unbound_default_is_unavailable_without_poll(self):
        client = self.client()
        self.assertFalse(client.available)
        self.assertFalse((await client.status())["available"])
        with self.assertRaises(self.module.OpenClawUnavailable):
            await client.action("grip", {}, "fixture", "fixture")

    async def test_bridge_existence_without_link_is_not_available(self):
        bridge = FakeBridge({"success": True}, linked=False)
        self.assertFalse(self.client(bridge).available)
        self.assertEqual(bridge.calls, [])

    async def test_link_flag_must_be_boolean_true(self):
        self.assertFalse(self.client(FakeBridge({}, linked="false")).available)

    async def test_explicit_binding_still_respects_disabled_setting(self):
        self.settings.openclaw_enabled = False
        client = self.client(FakeBridge({"success": True}))
        self.assertFalse(client.available)

    async def test_missing_frame_is_not_success(self):
        bridge = FakeBridge(missing_frame=True)
        result = await self.client(bridge).action("grip", {}, "fixture", "fixture")
        self.assertIs(result["ok"], False)

    async def test_nonobject_and_missing_success_are_not_success(self):
        for value in (None, [], "ok", 1, {}, {"success": "true"}, {"success": 1}, {"success": False}):
            with self.subTest(value=value):
                result = await self.client(FakeBridge(value)).action("grip", {}, "fixture", "fixture")
                self.assertIs(result["ok"], False)

    async def test_explicit_success_ack_is_labeled_unverified(self):
        bridge = FakeBridge({"success": True, "fixture": "ack"})
        result = await self.client(bridge).action("grip", {}, "fixture-request", "fixture-user")
        self.assertIs(result["ok"], True)
        self.assertEqual(result["evidence"], "device_acknowledgement_unverified")
        self.assertEqual(bridge.calls[0]["cmd"], "claw_grip")
        lines = [json.loads(line) for line in Path(self.settings.openclaw_audit_path).read_text().splitlines()]
        self.assertEqual([line["phase"] for line in lines], ["received", "started", "acknowledged"])
        self.assertEqual(len({line["id"] for line in lines}), 1)

    async def test_audit_unavailable_prevents_dispatch(self):
        bridge = FakeBridge({"success": True})
        client = self.client(bridge)
        with patch.object(Path, "open", side_effect=OSError("fixture audit failure")):
            with self.assertRaises(self.module.OpenClawUnreachable):
                await client.action("grip", {}, "fixture", "fixture")
        self.assertEqual(bridge.calls, [])

    async def test_transport_failure_does_not_report_completion(self):
        bridge = FakeBridge()
        async def fail(**_kwargs):
            raise TimeoutError("private transport detail")
        bridge.send_command = fail
        with self.assertRaisesRegex(self.module.OpenClawUnreachable, "command_unavailable"):
            await self.client(bridge).action("grip", {}, "fixture", "fixture")
        audit = [json.loads(line) for line in Path(self.settings.openclaw_audit_path).read_text().splitlines()]
        self.assertEqual(audit[-1]["phase"], "failed")
        self.assertNotIn("private transport", json.dumps(audit))

    async def test_estop_clears_only_with_positive_ack(self):
        bridge = FakeBridge({"success": True})
        client = self.client(bridge)
        await client.action("estop", {}, "fixture", "fixture")
        self.assertTrue(client._estop_latched)
        bridge.payload = {"success": False}
        await client.action("clear_estop", {}, "fixture", "fixture")
        self.assertTrue(client._estop_latched)
        bridge.payload = {"success": True}
        await client.action("clear_estop", {}, "fixture", "fixture")
        self.assertFalse(client._estop_latched)

    async def test_latch_rechecked_after_waiting_for_action_lock(self):
        bridge = FakeBridge({"success": True})
        client = self.client(bridge)
        await client._action_lock.acquire()
        task = asyncio.create_task(client.action("grip", {}, "fixture", "fixture"))
        await asyncio.sleep(0)
        client._estop_latched = True
        client._action_lock.release()
        with self.assertRaises(self.module.OpenClawLocked):
            await task
        self.assertEqual(bridge.calls, [])

    async def test_invalid_action_or_params_never_dispatches(self):
        bridge = FakeBridge({"success": True})
        client = self.client(bridge)
        for action, params in [("unknown", {}), ("grip", []), ("open", {})]:
            with self.subTest(action=action):
                with self.assertRaises((ValueError, self.module.OpenClawRetired)):
                    await client.action(action, params, "fixture", "fixture")
        self.assertEqual(bridge.calls, [])

    async def test_status_missing_or_nonobject_reply_not_cached_as_ready(self):
        for payload in (None, [], "ok"):
            with self.subTest(payload=payload):
                client = self.client(FakeBridge(payload))
                result = await client.status()
                self.assertFalse(result["ready"])
                self.assertIsNone(client._last_status)


class EntrypointTests(unittest.IsolatedAsyncioTestCase):
    def composition(self, *, server_wait=False, server_error=False):
        events, openclaw_args = [], []
        settings = types.SimpleNamespace(adapter="fixture", http_port=0,
            http_host="fixture", log_level="INFO")
        class Service:
            def __init__(self, **_kwargs):
                self.name = type(self).__name__
            async def run(self):
                events.append((self.name, "started"))
                try:
                    await asyncio.Event().wait()
                finally:
                    events.append((self.name, "cancelled"))
        class Bridge(Service):
            pass
        class Mqtt(Service):
            pass
        class Heartbeat(Service):
            pass
        class Server:
            should_exit = False
            def __init__(self, _config):
                pass
            async def serve(self):
                try:
                    if server_wait:
                        await asyncio.Event().wait()
                    else:
                        await asyncio.sleep(0)
                    if server_error:
                        raise RuntimeError("fixture server failure")
                finally:
                    events.append(("server", "finished"))
        def openclaw(**kwargs):
            openclaw_args.append(kwargs)
            return types.SimpleNamespace(available=False)
        modules = {
            "mycobrain_agent.adapters": types.SimpleNamespace(load_adapter=lambda _settings: object()),
            "mycobrain_agent.config": types.SimpleNamespace(Settings=lambda: settings),
            "mycobrain_agent.core.registry": types.SimpleNamespace(DeviceRegistry=lambda **_kw: object()),
            "mycobrain_agent.core.serial_bridge": types.SimpleNamespace(SerialBridge=Bridge),
            "mycobrain_agent.core.mqtt_client": types.SimpleNamespace(MqttClient=Mqtt),
            "mycobrain_agent.core.heartbeat": types.SimpleNamespace(HeartbeatService=Heartbeat),
            "mycobrain_agent.openclaw.client": types.SimpleNamespace(OpenClawClient=openclaw),
            "mycobrain_agent.http.server": types.SimpleNamespace(build_app=lambda **_kw: object()),
            "uvicorn": types.SimpleNamespace(Config=lambda *_a, **_kw: object(), Server=Server),
        }
        import sys
        with patch.dict(sys.modules, modules):
            subject = load(ENTRY, "entrypoint_recovery_fixture")
        return subject, settings, events, openclaw_args

    async def test_composes_fake_services_but_leaves_openclaw_unbound_and_cleans_tasks(self):
        subject, settings, events, args = self.composition()
        loop = asyncio.get_running_loop()
        with patch.object(loop, "add_signal_handler"):
            await subject._run(settings)
        self.assertEqual(args, [{"settings": settings}])
        for name in ("Bridge", "Mqtt", "Heartbeat"):
            self.assertIn((name, "cancelled"), events)
        self.assertTrue(callable(subject.main))

    async def test_external_cancellation_cleans_owned_tasks(self):
        subject, settings, events, _ = self.composition(server_wait=True)
        loop = asyncio.get_running_loop()
        with patch.object(loop, "add_signal_handler"):
            task = asyncio.create_task(subject._run(settings))
            await asyncio.sleep(0)
            await asyncio.sleep(0)
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task
        for name in ("Bridge", "Mqtt", "Heartbeat"):
            self.assertIn((name, "cancelled"), events)
        self.assertIn(("server", "finished"), events)

    async def test_server_failure_propagates_after_cleanup(self):
        subject, settings, events, _ = self.composition(server_error=True)
        loop = asyncio.get_running_loop()
        with patch.object(loop, "add_signal_handler"):
            with self.assertRaisesRegex(RuntimeError, "fixture server failure"):
                await subject._run(settings)
        self.assertIn(("Bridge", "cancelled"), events)


if __name__ == "__main__":
    unittest.main()
