"""RP-02 authorization regression tests; all commands and JWKS responses are fixtures."""

from datetime import datetime, timedelta, timezone
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock

import jwt
import pytest
from cryptography.hazmat.primitives.asymmetric import rsa
from fastapi import Depends, FastAPI
from fastapi.testclient import TestClient

from mycobrain_agent.http import auth_middleware


@pytest.fixture(scope="module")
def signing_keys():
    # Ephemeral test keys; never loaded from or written to a credential store.
    return [rsa.generate_private_key(public_exponent=65537, key_size=2048) for _ in range(2)]


def token_for(key=None, *, expired=False):
    claims = {
        "sub": "auth-regression-fixture",
        "exp": datetime.now(timezone.utc) + timedelta(seconds=-30 if expired else 300),
    }
    return jwt.encode(claims, key, algorithm="RS256" if key else "none", headers={"kid": "fixture-key"})


def command_client(*, jwks_url=None, auth_mode="jwt", pair_token=None):
    app = FastAPI()
    app.state.settings = SimpleNamespace(
        auth_mode=auth_mode, natureos_jwks_url=jwks_url, pair_token=pair_token,
    )
    calls = []

    @app.post("/command", dependencies=[Depends(auth_middleware.require_auth)])
    async def fake_command():
        calls.append("fixture-only")
        return {"accepted": True}

    return TestClient(app), calls


def fixture_jwks(monkeypatch, key):
    public = jwt.algorithms.RSAAlgorithm.to_jwk(key.public_key(), as_dict=True)
    public.update(kid="fixture-key", alg="RS256")
    http = SimpleNamespace(get=AsyncMock(return_value=SimpleNamespace(json=lambda: {"keys": [public]})))
    context = MagicMock()
    context.__aenter__ = AsyncMock(return_value=http)
    context.__aexit__ = AsyncMock(return_value=None)
    factory = MagicMock(return_value=context)
    monkeypatch.setattr(auth_middleware.httpx, "AsyncClient", factory)
    return http


@pytest.mark.parametrize("jwks_url", [None, ""])
@pytest.mark.parametrize("signed", [False, True])
def test_missing_jwks_rejects_before_command_or_http(monkeypatch, signing_keys, jwks_url, signed):
    http_factory = MagicMock(side_effect=AssertionError("missing configuration must not fetch JWKS"))
    monkeypatch.setattr(auth_middleware.httpx, "AsyncClient", http_factory)
    client, calls = command_client(jwks_url=jwks_url)
    token = token_for(signing_keys[0] if signed else None)
    response = client.post("/command", headers={"Authorization": f"Bearer {token}"})
    assert response.status_code == 401
    assert calls == []
    http_factory.assert_not_called()


@pytest.mark.parametrize("kind", ["unsigned", "wrong-key", "expired"])
def test_configured_verifier_rejects_invalid_signature_or_claims(monkeypatch, signing_keys, kind):
    fixture_jwks(monkeypatch, signing_keys[0])
    client, calls = command_client(jwks_url="https://fixture.invalid/jwks")
    key = None if kind == "unsigned" else signing_keys[1] if kind == "wrong-key" else signing_keys[0]
    token = token_for(key, expired=kind == "expired")
    response = client.post("/command", headers={"Authorization": f"Bearer {token}"})
    assert response.status_code == 401
    assert calls == []


def test_valid_configured_fixture_reaches_only_fake_command(monkeypatch, signing_keys):
    http = fixture_jwks(monkeypatch, signing_keys[0])
    client, calls = command_client(jwks_url="https://fixture.invalid/jwks")
    response = client.post("/command", headers={"Authorization": f"Bearer {token_for(signing_keys[0])}"})
    assert response.status_code == 200
    assert calls == ["fixture-only"]
    http.get.assert_awaited_once_with("https://fixture.invalid/jwks")


def test_missing_bearer_rejects_before_fake_command():
    client, calls = command_client(jwks_url="https://fixture.invalid/jwks")
    assert client.post("/command").status_code == 401
    assert calls == []


def test_explicit_none_mode_is_unchanged():
    client, calls = command_client(auth_mode="none")
    assert client.post("/command").status_code == 200
    assert calls == ["fixture-only"]


@pytest.mark.parametrize("supplied,expected", [(None, 401), ("incorrect-fixture", 401), ("pair-fixture", 200)])
def test_explicit_pair_token_mode_is_unchanged(supplied, expected):
    client, calls = command_client(auth_mode="pair_token", pair_token="pair-fixture")
    headers = {"X-Pair-Token": supplied} if supplied else {}
    assert client.post("/command", headers=headers).status_code == expected
    assert len(calls) == (1 if expected == 200 else 0)
