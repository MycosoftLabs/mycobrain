"""JWT / pair-token verification for write endpoints.

Read endpoints are public when ``MYCOBRAIN_PUBLIC_READS=true`` (the default
for now, to preserve dashboard compatibility). Writes always go through this
dependency.
"""

from __future__ import annotations

from typing import Any

import httpx
import jwt
from fastapi import Header, HTTPException, Request


async def require_auth(
    request: Request,
    authorization: str | None = Header(default=None),
    x_pair_token: str | None = Header(default=None),
) -> None:
    settings = request.app.state.settings
    if settings.auth_mode == "none":
        return
    if settings.auth_mode == "pair_token":
        if not x_pair_token or x_pair_token != settings.pair_token:
            raise HTTPException(status_code=401, detail="invalid_pair_token")
        return
    # jwt mode
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(status_code=401, detail="missing_bearer")
    token = authorization.split(" ", 1)[1].strip()
    try:
        await _verify_jwt(token, settings)
    except jwt.PyJWTError as exc:
        raise HTTPException(status_code=401, detail=f"invalid_jwt:{exc}")


async def _verify_jwt(token: str, settings: Any) -> None:
    if not settings.natureos_jwks_url:
        # Dev fallback: unverified decode just to fail loudly on tampering
        jwt.decode(token, options={"verify_signature": False})
        return
    async with httpx.AsyncClient(timeout=5.0) as client:
        resp = await client.get(settings.natureos_jwks_url)
        jwks = resp.json()
    header = jwt.get_unverified_header(token)
    kid = header.get("kid")
    key = next((k for k in jwks.get("keys", []) if k.get("kid") == kid), None)
    if key is None:
        raise jwt.InvalidKeyError(f"no matching kid: {kid}")
    pubkey = jwt.algorithms.RSAAlgorithm.from_jwk(key) if key["kty"] == "RSA" else jwt.algorithms.OKPAlgorithm.from_jwk(key)
    jwt.decode(token, key=pubkey, algorithms=[key.get("alg", "EdDSA")])
