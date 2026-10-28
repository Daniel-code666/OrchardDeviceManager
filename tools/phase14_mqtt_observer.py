#!/usr/bin/env python3
"""Captura evidencia MQTT saliente de varios dispositivos sin almacenar secretos."""

import argparse
import json
import signal
import sys
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

TOPICS = (
    "gardens/+/telemetry",
    "gardens/+/actions-response",
    "gardens/+/inventory",
    "gardens/+/control-status",
)
SENSITIVE_KEYS = {
    "password", "wifipassword", "provisioningtoken", "bootstrapkey",
    "mqttuser", "mqttpassword", "authenticationtag",
}


def sanitize(value):
    if isinstance(value, dict):
        return {
            key: "[REDACTED]" if key.lower() in SENSITIVE_KEYS else sanitize(item)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [sanitize(item) for item in value]
    return value


def utc_now():
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=1884)
    parser.add_argument("--user", required=True)
    parser.add_argument("--password", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--duration", type=int, default=0,
                        help="Segundos; 0 mantiene la captura hasta Ctrl+C")
    args = parser.parse_args()

    try:
        import paho.mqtt.client as mqtt
    except ImportError as error:
        raise SystemExit("Instala paho-mqtt: python -m pip install paho-mqtt") from error

    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    counts = Counter()
    stop = False

    def request_stop(_signal=None, _frame=None):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f"huerta-phase14-{int(time.time())}", clean_session=True)
    client.username_pw_set(args.user, args.password)

    def on_connect(active_client, _userdata, _flags, reason_code, _properties):
        if reason_code != 0:
            print(f"Conexion MQTT rechazada: {reason_code}", file=sys.stderr)
            return
        for topic in TOPICS:
            active_client.subscribe(topic, qos=0)
        print(f"Observando {len(TOPICS)} topics; evidencia: {output}")

    def on_message(_client, _userdata, message):
        raw = message.payload.decode("utf-8", errors="replace")
        try:
            payload = sanitize(json.loads(raw))
        except json.JSONDecodeError:
            payload = {"invalidJson": True, "size": len(message.payload)}
        record = {
            "receivedAt": utc_now(), "topic": message.topic,
            "retained": bool(message.retain), "qos": message.qos, "payload": payload,
        }
        with output.open("a", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(record, separators=(",", ":"), ensure_ascii=False) + "\n")
        counts[message.topic] += 1
        print(f"{record['receivedAt']} | {message.topic}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()
    started = time.monotonic()
    try:
        while not stop and (args.duration == 0 or time.monotonic() - started < args.duration):
            time.sleep(0.2)
    finally:
        client.disconnect()
        client.loop_stop()
    print(f"Captura finalizada: {sum(counts.values())} mensajes")
    for topic, count in sorted(counts.items()):
        print(f"  {topic}: {count}")


if __name__ == "__main__":
    main()
