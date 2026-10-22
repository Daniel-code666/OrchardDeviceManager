import os
import threading
import time

import paho.mqtt.client as mqtt


HOST = os.environ.get("MQTT_TEST_HOST", "127.0.0.1")
PORT = int(os.environ.get("MQTT_TEST_PORT", "1884"))
USERNAME = os.environ["MQTT_TEST_USER"]
PASSWORD = os.environ["MQTT_TEST_PASSWORD"]

DEVICE_A = "huerta-simulada-a"
DEVICE_B = "huerta-simulada-b"


def topic(device_id: str, suffix: str) -> str:
    return f"gardens/{device_id}/{suffix}"


messages_a: list[tuple[str, bytes]] = []
messages_b: list[tuple[str, bytes]] = []
connected_a = threading.Event()
connected_b = threading.Event()


def create_client(client_id: str, messages: list[tuple[str, bytes]], connected: threading.Event):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    client.username_pw_set(USERNAME, PASSWORD)

    def on_connect(current_client, _userdata, _flags, reason_code, _properties):
        if reason_code == 0:
            connected.set()
        else:
            raise RuntimeError(f"MQTT connect failed: {reason_code}")

    def on_message(_client, _userdata, message):
        messages.append((message.topic, message.payload))

    client.on_connect = on_connect
    client.on_message = on_message
    return client


client_a = create_client("mqtt-huerta-simulada-a", messages_a, connected_a)
client_b = create_client("mqtt-huerta-simulada-b", messages_b, connected_b)

try:
    client_a.connect(HOST, PORT, 10)
    client_b.connect(HOST, PORT, 10)
    client_a.loop_start()
    client_b.loop_start()

    assert connected_a.wait(5), "Device A did not connect"
    assert connected_b.wait(5), "Device B did not connect"

    client_a.subscribe(topic(DEVICE_A, "actions"), qos=0)
    client_a.subscribe(topic(DEVICE_A, "actions-response"), qos=0)
    client_b.subscribe(topic(DEVICE_B, "actions"), qos=0)
    client_b.subscribe(topic(DEVICE_B, "actions-response"), qos=0)
    time.sleep(0.5)

    client_a.publish(topic(DEVICE_B, "actions"), b'{"target":"B"}').wait_for_publish()
    client_b.publish(topic(DEVICE_B, "actions-response"), b'{"source":"B"}').wait_for_publish()
    client_a.publish(topic(DEVICE_A, "telemetry"), b'{"device":"A"}').wait_for_publish()
    client_b.publish(topic(DEVICE_B, "telemetry"), b'{"device":"B"}').wait_for_publish()
    time.sleep(1.0)

    assert not any(received_topic == topic(DEVICE_B, "actions") for received_topic, _ in messages_a), (
        "Device A received Device B's action"
    )
    assert any(received_topic == topic(DEVICE_B, "actions") for received_topic, _ in messages_b), (
        "Device B did not receive its action"
    )
    assert not any(received_topic == topic(DEVICE_B, "actions-response") for received_topic, _ in messages_a), (
        "Device A received Device B's response"
    )
    assert any(received_topic == topic(DEVICE_B, "actions-response") for received_topic, _ in messages_b), (
        "Device B did not receive its response"
    )

    print("PASS: two MQTT device identities remained isolated")
finally:
    client_a.loop_stop()
    client_b.loop_stop()
    client_a.disconnect()
    client_b.disconnect()
