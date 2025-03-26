# Digital Dial Indicator Sensor Firmware
## Used librarys
- heltec_esp32_lora_v3 <https://github.com/ropg/heltec_esp32_lora_v3>
- LoRaWAN_ESP32 <https://github.com/ropg/LoRaWAN_ESP32>

## Programm flow
<img src="./flowchart.drawio.svg" alt="flowchart" width="500"/>

## Future Improvements
- Implement manual send and receive and light sleep in between to significantly reduce the power consumption like discussed here: <https://github.com/jgromes/RadioLib/discussions/964>

## Troubleshooting
- Add just firmware folder to to PlatformIO workspace
- If the project is not listed in the PIO environment list -> F1 -> PlatformIO: Rebuild IntelliSense Index