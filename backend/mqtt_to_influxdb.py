import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from dateutil import parser as date_parser
import json
import os
import datetime

MQTT_BROKER = os.getenv("MQTT_BROKER")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_USERNAME = os.getenv("MQTT_USERNAME")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")
MQTT_TOPIC = f"v3/{MQTT_USERNAME}@ttn/devices/+/up"

INFLUXDB_URL = os.getenv("INFLUXDB_URL")
INFLUXDB_TOKEN = os.getenv("INFLUXDB_TOKEN")
INFLUXDB_ORG = os.getenv("INFLUXDB_ORG")
INFLUXDB_BUCKET = os.getenv("INFLUXDB_BUCKET")

print("Start InfluxDB Client")
influx_client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
write_api = influx_client.write_api(write_options=SYNCHRONOUS)

def on_connect(client, obj, flags, reason_code, properties):
    """
    Callback für die erfolgreiche Verbindung zum MQTT-Broker.
    
    Args:
        client: Der MQTT-Client.
        obj: Benutzerdefinierte Daten.
        flags: MQTT-Flags (z.B. Session vorhanden).
        reason_code: Grundcode für die Verbindung.
        properties: MQTT-Eigenschaften.
    """
    if flags.session_present:
        print("Session ist bereits vorhanden.")
    else:
        print("Keine vorherige Session vorhanden.")
    
    if reason_code == 0:
        print("Erfolgreich mit MQTT-Broker verbunden.")
    else:
        print(f"Fehler beim Verbinden mit MQTT-Broker. Grund: {reason_code}")
    
    client.subscribe(MQTT_TOPIC)
    print(f"Abonniert auf Thema: {MQTT_TOPIC}")

def on_message(client, obj, msg):
    """
    Callback für eingehende MQTT-Nachrichten.
    
    Args:
        client: Der MQTT-Client.
        obj: Benutzerdefinierte Daten.
        msg: Die empfangene Nachricht.
    """
    try:
        payload = json.loads(msg.payload.decode())
        device_id = payload["end_device_ids"]["device_id"]
        received_at = payload["received_at"]
        decoded_payload = payload["uplink_message"]["decoded_payload"]

        pos = decoded_payload.get("pos", None)
        pos = float(pos) if pos is not None else None

        temp_ext = decoded_payload.get("temp_ext", None)
        temp_ext = float(temp_ext) if temp_ext is not None else None

        temp_pcb = decoded_payload.get("temp_pcb", None)
        temp_pcb = float(temp_pcb) if temp_pcb is not None else None

        hum_ext = decoded_payload.get("hum_ext", None)
        hum_ext = float(hum_ext) if hum_ext is not None else None

        battery = decoded_payload.get("battery", None)
        battery = int(battery) if battery is not None else None

        angle = decoded_payload.get("angle", None)
        angle = int(angle) if angle is not None else None


        timestamp = date_parser.isoparse(received_at)
        print(f"Daten von {device_id} gespeichert: pos={pos}, angle={angle}, temp_ext={temp_ext}, temp_pcb={temp_pcb}, hum_ext={hum_ext}, battery={battery}, Timestamp: {timestamp}")
        
        point = (
            Point("crackmon_sensor_data")
            .tag("device_id", device_id)
            .field("battery", battery)
            .field("hum_ext", hum_ext)
            .field("pos", pos)
            .field("temp_ext", temp_ext)
            .field("temp_pcb", temp_pcb)
            .field("angle", angle)
            .time(timestamp)
        )

        write_api.write(bucket=INFLUXDB_BUCKET, org=INFLUXDB_ORG, record=point)
    
    except Exception as e:
        print(f"Fehler beim Verarbeiten der Nachricht: {e}")

def on_disconnect(client, obj, flags, reason_code, properties):
    """
    Callback für die Trennung vom MQTT-Broker.
    
    Args:
        client: Der MQTT-Client.
        obj: Benutzerdefinierte Daten.
        reason_code: Grundcode für die Trennung.
        properties: MQTT-Eigenschaften.
    """
    if reason_code == 0:
        print("Erfolgreich vom MQTT-Broker getrennt.")
    else:
        print(f"Unerwartete Trennung vom MQTT-Broker. Grund: {reason_code}")

print("Start MQTT Client")
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.on_disconnect = on_disconnect

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

mqtt_client.loop_forever()
