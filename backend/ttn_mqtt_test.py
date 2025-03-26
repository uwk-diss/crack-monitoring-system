import paho.mqtt.client as mqtt
import json

# TTN MQTT Verbindungsparameter
TTN_MQTT_HOST = "eu1.cloud.thethings.network"  # Passe den Host entsprechend deiner Region an
TTN_MQTT_PORT = 1883  # Verwende 8883 für TLS
APPLICATION_ID = "crack-monitoring"
ACCESS_KEY = "NNSXS.6BJWC6457FYVX2EOVTUJXAO2G5TTTX3ZUVSFYAI.GWFPB6777H2SJVKMY5XOVWQPF4UGZNGJ2UY2WKJSIYGPZZSB72DA"
DEVICE_ID = "crackmon-o1"

# MQTT-Themen
# Für TTN v3 wird das Thema im Format 'v3/{application_id}@ttn/devices/{device_id}/up'
TOPIC = f"v3/{APPLICATION_ID}@ttn/devices/{DEVICE_ID}/up"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Verbunden mit TTN MQTT Broker")
        client.subscribe(TOPIC)
        print(f"Abonniert auf Thema: {TOPIC}")
    else:
        print(f"Verbindung fehlgeschlagen mit Code {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        print("Nachricht empfangen:")
        print(json.dumps(data, indent=4))
    except Exception as e:
        print(f"Fehler beim Verarbeiten der Nachricht: {e}")

def main():
    client = mqtt.Client(client_id=f"{APPLICATION_ID}@ttn", clean_session=True)

    # TTN v3 verwendet die Application ID als Benutzername und den Access Key als Passwort
    client.username_pw_set(username=f"{APPLICATION_ID}@ttn", password=ACCESS_KEY)

    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(TTN_MQTT_HOST, TTN_MQTT_PORT, keepalive=60)
    except Exception as e:
        print(f"Verbindungsfehler: {e}")
        return

    # Starte die Netzwerk-Schleife
    client.loop_forever()

if __name__ == "__main__":
    main()
