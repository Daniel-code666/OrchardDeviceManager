#!/usr/bin/env python3
"""Genera respuestas autenticadas para el transporte serie de la Fase 4."""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
from pathlib import Path
from typing import Any


def _field(value: str) -> str:
    return f"{len(value.encode('utf-8'))}:{value}\n"


def canonical_payload(response: dict[str, Any]) -> bytes:
    values = (
        str(response["contractVersion"]),
        response["deviceId"],
        response["requestId"],
        response["decision"],
        response.get("userId", ""),
        response.get("gardenId", ""),
        response.get("errorCode", ""),
    )
    return "".join(_field(value) for value in values).encode("utf-8")


def build_response(
    request: dict[str, Any],
    decision: str,
    user_id: str = "",
    garden_id: str = "",
    invalid_authentication: bool = False,
) -> dict[str, Any]:
    if request.get("type") != "claim_request" or request.get("contractVersion") != 1:
        raise ValueError("La solicitud no cumple el contrato de Fase 4")

    error_code = ""
    if decision == "rejected":
        error_code = "claim_rejected"
    elif decision == "expired":
        error_code = "claim_expired"
    elif decision != "approved":
        raise ValueError("Decision no soportada")

    if decision == "approved" and (not user_id or not garden_id):
        raise ValueError("Una aprobacion requiere user-id y garden-id")

    response: dict[str, Any] = {
        "type": "claim_response",
        "contractVersion": 1,
        "deviceId": request["deviceId"],
        "requestId": request["requestId"],
        "decision": decision,
        "userId": user_id if decision == "approved" else "",
        "gardenId": garden_id if decision == "approved" else "",
        "errorCode": error_code,
    }
    tag = hmac.new(
        request["claimCode"].encode("utf-8"),
        canonical_payload(response),
        hashlib.sha256,
    ).hexdigest()
    if invalid_authentication:
        tag = ("0" if tag[0] != "0" else "1") + tag[1:]
    response["authenticationTag"] = tag
    return response


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Firma una respuesta de vinculacion para pegarla en el monitor serie."
    )
    parser.add_argument("--request-file", type=Path, required=True)
    parser.add_argument("--decision", choices=("approved", "rejected", "expired"), required=True)
    parser.add_argument("--user-id", default="")
    parser.add_argument("--garden-id", default="")
    parser.add_argument("--invalid-authentication", action="store_true")
    arguments = parser.parse_args()

    request = json.loads(arguments.request_file.read_text(encoding="utf-8"))
    response = build_response(
        request,
        arguments.decision,
        arguments.user_id,
        arguments.garden_id,
        arguments.invalid_authentication,
    )
    print(json.dumps(response, separators=(",", ":"), ensure_ascii=True))


if __name__ == "__main__":
    main()

