#!/usr/bin/env python3
"""Genera comandos autenticados de recuperación para el transporte serie local."""

import argparse
import hashlib
import hmac
import json
import secrets


def append_field(value: str) -> str:
    return f"{len(value)}:{value}\n"


def canonical_payload(command: dict) -> bytes:
    values = (
        str(command["contractVersion"]),
        command["deviceId"],
        command["requestId"],
        str(command["commandVersion"]),
        command["action"],
    )
    return "".join(append_field(value) for value in values).encode("utf-8")


def build_command(device_id: str, key_hex: str, action: str,
                  command_version: int, request_id: str | None = None) -> dict:
    key = bytes.fromhex(key_hex)
    if len(key) != 32:
        raise ValueError("La clave local debe contener 32 bytes (64 caracteres hex).")
    if action not in {"revoke_mqtt", "unlink"}:
        raise ValueError("Acción no soportada.")
    if command_version <= 0:
        raise ValueError("command-version debe ser mayor que cero.")

    command = {
        "type": "recovery_command",
        "contractVersion": 1,
        "deviceId": device_id,
        "requestId": request_id or secrets.token_hex(16),
        "commandVersion": command_version,
        "action": action,
    }
    command["authenticationTag"] = hmac.new(
        key, canonical_payload(command), hashlib.sha256
    ).hexdigest()
    return command


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--local-key", required=True)
    parser.add_argument("--action", choices=("revoke_mqtt", "unlink"), required=True)
    parser.add_argument("--command-version", type=int, required=True)
    parser.add_argument("--request-id")
    parser.add_argument("--invalid-authentication", action="store_true")
    args = parser.parse_args()

    command = build_command(args.device_id, args.local_key, args.action,
                            args.command_version, args.request_id)
    if args.invalid_authentication:
        command["authenticationTag"] = "0" * 64
    print(json.dumps(command, separators=(",", ":")))


if __name__ == "__main__":
    main()

