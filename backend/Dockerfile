FROM python:3.13-slim

WORKDIR /app

COPY mqtt_to_influxdb.py /app/mqtt_to_influxdb.py

RUN pip install pip \
    && pip install paho-mqtt influxdb-client python-dateutil

CMD ["python", "-u", "mqtt_to_influxdb.py"]
