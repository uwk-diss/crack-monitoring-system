# How to Deploy the Crack Monitoring Backend
This manual includes the registration of the Sensors and the setup of the MQTT-bridge, InfluxDB database and Grafana using Docker.

## 1. Register End Devices in The Things Network

1. Sign up at https://www.thethingsnetwork.org/login/  
2. Register each device with the following configuration:  
   - **Input method:** Enter end device specifics manually  
   - **Frequency plan:** Europe 868‑870 MHz (SF9 for RX2 recommended)  
   - **LoRaWAN version:** 1.1.0  
   - **Regional Parameters version:** RP001 Regional Parameters 1.1 revision B  
   - **JoinEUI:** All zeros → Confirm  
   - **DevEUI:** Generate  
   - **AppKey:** Generate  
   - **NwkKey:** Generate  
   - **End device ID:** e.g., `crackmon-01`  

3. Update `grafana/provisioning/dashboards/dashboards/CrackMonitoring.json` so device IDs (`crackmon-01`, `crackmon-02`, etc.) match your registered sensor IDs.

Detailed reference: https://github.com/jgromes/RadioLib/wiki/LoRaWAN:-Device-setup-on-TTN

### Create Payload Formatter

Adapt this JavaScript function so it fits the Payload and paste it in TTN Console → Payload Formats → Decoder:

```js
function decodeUplink(input) {
    const bytes = input.bytes;
    function int16(msb, lsb) {
        let value = (msb << 8) | lsb;
        return value & 0x8000 ? value - 0x10000 : value;
    }
    return {
        data: {
            pos: int16(bytes[0], bytes[1]) / 1000,
            temp_ext: int16(bytes[2], bytes[3]) / 100,
            hum_ext: bytes[4],
            temp_pcb: int16(bytes[5], bytes[6]) / 100,
            battery: bytes[7],
            angle: int16(bytes[8], bytes[9])
        }
    };
}
```

Test with hex payload `F6D90A271E0B156323`. Expected output:

```json
{
  "pos": -2.343,
  "angle": 8960,
  "temp_ext": 25.99,
  "hum_ext": 30,
  "temp_pcb": 28.37,
  "battery": 99
}
```

## 2. Create `.env` File

In the backend directory create a file named `.env` with these variables:

```bash
MQTT_USERNAME=your_ttn_mqtt_username
MQTT_PASSWORD=your_ttn_mqtt_token

INFLUXDB_INIT_USERNAME=admin
INFLUXDB_INIT_PASSWORD=<your-password>
INFLUXDB_INIT_ADMIN_TOKEN=<your-token>

GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=<your-password>
```

Generate a secure InfluxDB token with:

```bash
python3 - <<<'import secrets; print(secrets.token_hex(32))'
```

## 3. Build and Start Docker Containers

```bash
docker-compose build
docker-compose up -d
```

## 4. Test Downlink

Simulate uplink in TTN Console using payload `F6D90A271E0B156323`
```bash
docker logs mqtt_to_influxdb
```

Check InfluxDB at http://localhost:8086

## 5. Configure Grafana
1. At <http://localhost:3000/> in *Connections/Data sources* select *InfluxDB* and manually add the influxdb token
2. In the dashboard edit each panel and click refresh in the 'Query inspector' to initially load the data
3. Add your own panels

## Troubleshooting
Check logs:
```
docker logs mqtt_to_influxdb
docker logs grafana
docker logs influxdb
```

Reset all volumes (deletes all saved data/changes in InfluxDB and Grafana container!):
```
docker-compose down -v
```

Remove container before recreating:
```
docker container rm mqtt_to_influxdb
```

Rebuild single container after changes:
```
docker compose build mqtt_to_influxdb
docker compose up -d  mqtt_to_influxdb
```

RaspberryPi: Add `platform: linux/arm64` under Grafana and InfluxDB services in `docker-compose.yml`.

## File Transfer to Raspberry Pi

```bash
scp -r ./backend pi@10.0.0.71:/home/pi/
scp ./mqtt_to_influxdb.py pi@10.0.0.71:/home/pi/backend/
```

## Backup & Restore InfluxDB

```bash
# Backup
docker ps
docker exec -it <container_id> influx backup /opt/influxdb/data/backup
docker cp <container_id>:/opt/influxdb/data/backup ~/backend/backup

# Transfer to local
scp -r pi@raspberrypi.local:/home/pi/backend/backup ~/influx_backup

# Restore
docker cp ~/influx_backup/ <container_id>:/home/user/
docker exec -it <container_id> influx restore /home/user/
```

