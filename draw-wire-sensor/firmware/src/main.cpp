/**
 * Draw Wire Crack Monitoring Sensor
 * 
 * This code uses Rop Gonggrijps heltec_esp32_lora_v3 library and is based on 
 * the LoRaWAN_TTN.ino example, which is licensed under the MIT License.
 * 
 * If your NVS partition does not have stored TTN / LoRaWAN provisioning
 * information in it yet, you will be prompted for them on the serial port and
 * they will be stored for subsequent use.
 *
 * See https://github.com/ropg/LoRaWAN_ESP32
*/

#include <Arduino.h>
#include <Preferences.h>
#include <heltec_unofficial.h>
#include <LoRaWAN_ESP32.h>

// Pause between sends in seconds, so this is every 20 minutes. (Delay will be
// longer if regulatory or TTN Fair Use Policy requires it.)
#define MINIMUM_DELAY (20*60)

#define LENGTH 49400 //mm*1000, calibration length

// Function declarations
void goToSleep();
void radioInit();
void radioSend();
void getSensorData();
void encodePayload(uint8_t *uplinkData);
void printSensorData();
int16_t readRawAngle();
int32_t getPosition();
void resetZeroPosition();
float MCP9808readTemp();
uint8_t calculateCRC(uint8_t data[], uint8_t length);
void SHT45readTempHum(float &temperature, float &humidity);
void displayInfoFrame();
void check_nvs_data();
void calibration();


TwoWire I2C_Sensors = TwoWire(1);
LoRaWANNode* node;
int16_t state;
Preferences preferences;

const char* namespaceName = "crackMon";

// Pins
#define SDA_PIN 41
#define SCL_PIN 42
#define SW1_PIN 20
#define SW2_PIN 19

//AS5600 Hall Sensor
#define AS5600_I2C_ADDRESS 0x36
#define RAW_ANGLE_MSB 0x0C
#define RAW_ANGLE_LSB 0x0D

//MCP9808
#define MCP9808_ADDRESS 0x1F
#define MCP9808_TEMP_REGISTER 0x05
const float calibration_offset = -2;

//SHT45
#define SHT45_ADDRESS 0x44
#define CMD_MEASURE_HIGH_PRECISION 0xFD
#define CMD_SOFT_RESET 0x94

// OLED
bool display_flag = true; //set to false to save energy

// Sensor Data
float temp_pcb = -100.0;
float temp_ext = -100.0;
float hum_ext = 255;
int32_t pos = -100;
int16_t angle = -100;
int8_t bat = -100;

int16_t zero_pos = 100;

int16_t start_angle = 0; 
int16_t end_angle = 4096;
const int16_t gap_angle = 150;

void setup() {
  heltec_setup();
  heltec_ve(true);

  heltec_led(250);
  delay(50);
  heltec_led(0);

  delay(10);
  I2C_Sensors.begin(SDA_PIN, SCL_PIN, 400000);
  delay(100);
  pinMode(SW1_PIN,INPUT_PULLUP);
  pinMode(SW2_PIN,INPUT_PULLUP);
  check_nvs_data();
  if(digitalRead(SW1_PIN) == false) calibration();
  if(digitalRead(SW2_PIN) == false) resetZeroPosition();
  if(display_flag == false) display.displayOff();

  radioInit();
  getSensorData();
  if(display_flag == true) delay(2000);
  radioSend();

  goToSleep();    // Does not return, program starts over next round
}

void loop() {
  delay(100);
  // This is never called. There is no repetition: we always go back to
  // deep sleep one way or the other at the end of setup()
  getSensorData();
}

void radioInit() {
  both.println("Radio init");
  state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    both.println("Radio did not initialize. We'll try again later.");
    goToSleep();
  }

  node = persist.manage(&radio);

  if (!node->isActivated()) {
    both.println("Could not join network. We'll try again later.");
    heltec_led(30);
    delay(500);
    heltec_led(0);
    goToSleep();
  }
  // Manages uplink intervals to the TTN Fair Use Policy
  // https://avbentem.github.io/airtime-calculator/ttn/eu868/20
  node->setDutyCycle(true, 1250);
}

void radioSend(){
  uint8_t uplinkData[9];
  encodePayload(uplinkData);
  uint8_t downlinkData[256];
  size_t lenDown = sizeof(downlinkData);

  state = node->sendReceive(uplinkData, sizeof(uplinkData), 1, downlinkData, &lenDown);

  if(state == RADIOLIB_ERR_NONE) {
    both.println("Message sent, no downlink received.");
  } else if (state > 0) {
    both.println("Message sent, downlink received.");
  } else {
    both.printf("sendReceive returned error %d, we'll try again later.\n", state);
  }
}

void getSensorData() {
  uint8_t uplinkData[8];
  bat = heltec_battery_percent(-1);
  temp_pcb = MCP9808readTemp();
  pos = getPosition();
  SHT45readTempHum(temp_ext, hum_ext);
  if(display_flag) displayInfoFrame();

  // Generate sample hex data for decoding test in TTN dashboard
  //encodePayload(uplinkData);
  // for (int i = 0; i < sizeof(uplinkData); i++) {
  //   if (uplinkData[i] < 0x10) {
  //     Serial.print("0");
  //   }
  //   Serial.print(uplinkData[i], HEX);
  // }
  // Serial.println();
}

void encodePayload(uint8_t *uplinkData) {
    int16_t v2 = (int16_t)(temp_ext * 100);
    int16_t v3 = (uint8_t) hum_ext;
    int16_t v4 = (int16_t)(temp_pcb * 100);
    uplinkData[0] = pos >> 8;
    uplinkData[1] = pos & 0xFF;
    uplinkData[2] = v2 >> 8;
    uplinkData[3] = v2 & 0xFF;
    uplinkData[4] = v3;
    uplinkData[5] = v4 >> 8;
    uplinkData[6] = v4 & 0xFF;
    uplinkData[7] = bat;
    uplinkData[8] = angle >> 8;
    uplinkData[9] = angle & 0xFF;
}

void goToSleep() {
  // allows recall of the session after deepsleep
  persist.saveSession(node);
  // Calculate minimum duty cycle delay (per FUP & law!)
  uint32_t interval = node->timeUntilUplink();
  // And then pick it or our MINIMUM_DELAY, whichever is greater
  uint32_t delayMs = max(interval, ((uint32_t)MINIMUM_DELAY * 1000));
  if(display_flag){
    both.printf("Going to deep sleep now. Next TX in %i min\n", delayMs/(1000*60));
    delay(1000);  // So message prints
  }
  // and off to bed we go
  heltec_deep_sleep((delayMs-millis())/1000); // -millis() to subtract the process time
}

int16_t readRawAngle() {
    I2C_Sensors.beginTransmission(AS5600_I2C_ADDRESS);
    I2C_Sensors.write(RAW_ANGLE_MSB);
    if (I2C_Sensors.endTransmission(false) != 0) {
        both.println("AS5600 com. error_1!");
        return 0xFFFF;
    }

    if (I2C_Sensors.requestFrom(AS5600_I2C_ADDRESS, 2) != 2) {
        both.println("AS5600 com. error_2!");
        return 0xFFFF; 
    }

    uint8_t msb = I2C_Sensors.read();
    uint8_t lsb = I2C_Sensors.read();
    uint16_t raw_angle = (msb << 8) | lsb;
    // Serial.print("Raw Angle: ");
    // Serial.println(raw_angle);
    return raw_angle;
}

int32_t getPosition(){
  int32_t angle_ = readRawAngle() - start_angle;
  if (angle_ < -gap_angle) angle_ = angle_ + 4096;
  angle = angle_ + start_angle;
  int32_t raw_pos = map(angle_, 0, end_angle, 0, LENGTH);
  return (raw_pos - zero_pos);
}

void resetZeroPosition() {
  if (!preferences.begin(namespaceName, false)) { // false = Read-Write
    Serial.println("Failed to open preferences in write mode");
    delay(1000);
    return;
  }
  zero_pos = 0;
  zero_pos = getPosition();
  both.print("New zero_pos: ");
  both.println(zero_pos);
  preferences.putInt("zero_pos", zero_pos);
  preferences.end();
  delay(1000);
}

float MCP9808readTemp() {
  I2C_Sensors.beginTransmission(MCP9808_ADDRESS);
  I2C_Sensors.write(MCP9808_TEMP_REGISTER);
  if (I2C_Sensors.endTransmission() != 0) {
    both.println("MCP9808 com error!");
    return NAN;
  }

  I2C_Sensors.requestFrom(MCP9808_ADDRESS, 2);
  if (I2C_Sensors.available() == 2) {
    uint8_t msb = I2C_Sensors.read();
    uint8_t lsb = I2C_Sensors.read();

    int16_t rawTemp = ((msb << 8) | lsb) & 0x0FFF;
    if (msb & 0x10) { //if negative
      rawTemp -= 4096;
    }

    float temperature = rawTemp * 0.0625 + 1;
    return temperature + calibration_offset;
  }

  return NAN;
}

uint8_t calculateCRC(uint8_t data[], uint8_t length) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

void SHT45readTempHum(float &temperature, float &humidity) {
    I2C_Sensors.beginTransmission(SHT45_ADDRESS);
    I2C_Sensors.write(CMD_MEASURE_HIGH_PRECISION);
    if (I2C_Sensors.endTransmission() != 0) {
        //both.println("SHT45 com error!");
        temperature = -100;
        humidity = -100;
        return;
    }
    delay(10); // Wait for the measurement

    I2C_Sensors.requestFrom(SHT45_ADDRESS, 6);
    uint8_t tempMSB = I2C_Sensors.read();
    uint8_t tempLSB = I2C_Sensors.read();
    uint8_t tempCRC = I2C_Sensors.read();
    uint8_t humMSB = I2C_Sensors.read();
    uint8_t humLSB = I2C_Sensors.read();
    uint8_t humCRC = I2C_Sensors.read();

    // CRC-check
    uint8_t tempData[] = {tempMSB, tempLSB};
    if (calculateCRC(tempData, 2) != tempCRC) {
        both.println("Temp CRC error");
        temperature = NAN;
    }

    // CRC-check
    uint8_t humData[] = {humMSB, humLSB};
    if (calculateCRC(humData, 2) != humCRC) {
        both.println("Hum CRC error");
        humidity = NAN;
    }

    uint16_t rawTemp = (tempMSB << 8) | tempLSB;
    uint16_t rawHum = (humMSB << 8) | humLSB;

    temperature = -45.0 + 175.0 * (rawTemp / 65535.0);
    humidity = -6.0 + 125.0 * (rawHum / 65535.0);
}

void displayInfoFrame() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  int16_t y_pos = 0;

  String posStr = "Pos: " + String((float)pos/1000.0, 3) + " mm";
  display.drawString(0, y_pos, posStr);
  y_pos += 12;

  // String angleStr = "Angle: " + String(angle);
  // display.drawString(0, y_pos, angleStr);
  // y_pos += 12;

  String batStr = "BAT: " + String(bat) + " %";
  display.drawString(0, y_pos, batStr);
  y_pos += 12;

  String pcbStr = "PCB: " + String(temp_pcb, 1) + " °C";
  display.drawString(0, y_pos, pcbStr);
  y_pos += 12;

  String extStr = "EXT: " + String(temp_ext, 1) + " °C, " + String(hum_ext, 0) + " %";
  display.drawString(0, y_pos, extStr);

  display.display();
}

void check_nvs_data() {
  // Open the Namespace in READONLY mode
  if (!preferences.begin(namespaceName, true)) { // true = Read-Only
    Serial.println("Failed to open preferences");
    return;
  }

  // Check if the start_angle exists 
  if (preferences.isKey("start_angle")) {
    // "start_angle" exists, read the value
    start_angle = preferences.getInt("start_angle", start_angle);
    end_angle = preferences.getInt("end_angle", end_angle);
    zero_pos = preferences.getInt("zero_pos", zero_pos);
    Serial.print("nvs start_angle: ");
    Serial.println(start_angle);
    Serial.print("nvs end_angle: ");
    Serial.println(end_angle);
    Serial.print("nvs zero_pos: ");
    Serial.println(zero_pos);
  } else {
    both.println("No nvs calibration data");
    preferences.end();
    calibration();
  }

  // Close Preferences
  preferences.end();
}

void calibration() {
  both.println("Calibration started!");
  // Wait until SW2 is released
  while(digitalRead(SW2_PIN) == false) {
    delay(20);
  }
  both.println("Press SW2 to set start pos.");
  while(digitalRead(SW2_PIN) == true) {
    delay(20);
  }
  if (!preferences.begin(namespaceName, false)) {
    both.println("Failed to open preferences in write mode");
    delay(1000);
    return;
  }
  start_angle = readRawAngle();
  preferences.putInt("start_angle", start_angle);
  both.print("New start angle: ");
  both.println(start_angle);

  delay(500);
  while(digitalRead(SW2_PIN) == false) {
    delay(20);
  }
  both.println("Press SW2 to set end pos.");
  while(digitalRead(SW2_PIN) == true) {
    delay(20);
  }
  end_angle = readRawAngle() - start_angle;
  if (end_angle < 0) end_angle = end_angle + 4096;
  preferences.putInt("end_angle", end_angle);
  both.print("New end angle: ");
  both.println(end_angle);

  delay(500);
  while(digitalRead(SW2_PIN) == false) {
    delay(20);
  }
  both.println("Press SW2 to set zero pos.");
  while(digitalRead(SW2_PIN) == true) {
    delay(20);
  }
  zero_pos = 0;
  zero_pos = getPosition();
  preferences.putInt("zero_pos", zero_pos);
  both.print("New zero pos: ");
  both.println(zero_pos);
  preferences.end();

  delay(5000);
  both.println("Calibration done!");
}