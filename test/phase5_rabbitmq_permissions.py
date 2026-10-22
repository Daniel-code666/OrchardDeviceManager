import os
import threading
import time
import uuid

import paho.mqtt.client as mqtt


HOST = os.environ.get("MQTT_TEST_HOST", "127.0.0.1")
PORT = int(os.environ.get("MQTT_TEST_PORT", "1884"))
USERNAME = os.environ["MQTT_TEST_USER"]
PASSWORD = os.environ["MQTT_TEST_PASSWORD"]
DEVICE_ID = os.environ.get("MQTT_TEST_DEVICE_ID", "huerta-esp32-01")


def topic(device_id: str, suffix: str) -> str:
    return f"gardens/{device_id}/{suffix}"


def new_client(label: str):
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"phase5-{label}-{uuid.uuid4().hex[:8]}",
    )
    client.username_pw_set(USERNAME, PASSWORD)
    return client


def assert_allowed_topics():
    connected = threading.Event()
    subscribed = threading.Event()
    disconnected = threading.Event()
    client = new_client("allowed")

    def on_connect(current, _userdata, _flags, reason_code, _properties):
        if reason_code == 0:
            connected.set()
            current.subscribe(topic(DEVICE_ID, "actions"), qos=0)

    def on_subscribe(_client, _userdata, _mid, reason_codes, _properties):
        if reason_codes and not reason_codes[0].is_failure:
            subscribed.set()

    def on_disconnect(_client, _userdata, _flags, _reason_code, _properties):
        disconnected.set()

    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_disconnect = on_disconnect
    client.connect(HOST, PORT, 10)
    client.loop_start()
    try:
        assert connected.wait(5), "La identidad individual no pudo conectarse"
        assert subscribed.wait(5), "No pudo suscribirse a su topic actions"
        client.publish(topic(DEVICE_ID, "telemetry"), b'{"phase":5}', qos=1).wait_for_publish(5)
        client.publish(topic(DEVICE_ID, "actions-response"), b'{"ok":true}', qos=1).wait_for_publish(5)
        time.sleep(0.5)
        assert not disconnected.is_set(), "RabbitMQ rechazo un topic permitido"
    finally:
        client.loop_stop()
        client.disconnect()


def assert_foreign_topic_disconnect(operation: str):
    connected = threading.Event()
    disconnected = threading.Event()
    client = new_client(f"foreign-{operation}")

    def on_connect(current, _userdata, _flags, reason_code, _properties):
        if reason_code != 0:
            return
        connected.set()
        suffix = "actions" if operation == "read" else "telemetry"
        foreign = topic("otro-dispositivo", suffix)
        if operation == "read":
            current.subscribe(foreign, qos=0)
        else:
            current.publish(foreign, b'{"forbidden":true}', qos=1)

    def on_disconnect(_client, _userdata, _flags, _reason_code, _properties):
        disconnected.set()

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect(HOST, PORT, 10)
    client.loop_start()
    try:
        assert connected.wait(5), "No se conecto para ejecutar la prueba negativa"
        assert disconnected.wait(5), f"RabbitMQ permitio {operation} sobre un topic ajeno"
    finally:
        client.loop_stop()
        client.disconnect()


assert_allowed_topics()
assert_foreign_topic_disconnect("read")
assert_foreign_topic_disconnect("write")
print("PASS: la identidad MQTT solo puede usar los topics de su DeviceId")

