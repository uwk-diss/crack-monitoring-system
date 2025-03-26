#include <Arduino.h>
#include <Preferences.h>

/**
 * Digital Dial Indicator Crack Monitoring Sensor
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


// Pause between sends in seconds, so this is every 20 minutes. (Delay will be
// longer if regulatory or TTN Fair Use Policy requires it.)
#define MINIMUM_DELAY (20*60)


#include <heltec_unofficial.h>
#include <LoRaWAN_ESP32.h>

// Function declarations
void goToSleep();
void radioInit();
void radioSend();
void getSensorData();
void encodePayload(uint8_t *uplinkData);
void printSensorData();
void resetZeroPosition();
uint8_t calculateCRC(uint8_t data[], uint8_t length);
void SHT45readTempHum(float &temperature, float &humidity);
void displayInfoFrame();
void check_nvs_zero_pos();
float getShahePos();
static void IRAM_ATTR shahe_clock_isr();



TwoWire I2C_Sensors = TwoWire(1);
LoRaWANNode* node;
int16_t state;
Preferences preferences;

const char* namespaceName = "crackMon";
const char* key = "zero_pos";

// Pins
#define SDA_PIN 41
#define SCL_PIN 42

#define SW1_PIN 20
#define SW2_PIN 19

#define SHAHE_DATA_PIN  (gpio_num_t) 3
#define SHAHE_CLOCK_PIN (gpio_num_t) 2

//SHT45
#define SHT45_ADDRESS 0x44
#define CMD_MEASURE_HIGH_PRECISION 0xFD
#define CMD_SOFT_RESET 0x94

// Shahe
#define TOTAL_BITS 23
volatile uint8_t bitCount = 0;
volatile int32_t position_raw =  0;
volatile bool dataReady = false;
float zero_pos;
uint16_t clockTimeout = 1000;
uint32_t lastClockTime = 0;

// OLED
bool display_flag = true;

// Sensor Data
float temp_pcb = -100.0;
float temp_ext = -100.0;
float hum_ext = -100.0;
float pos = -100;
int8_t bat = -100;


void setup() {
  heltec_setup();
  heltec_ve(true);
  pinMode(SHAHE_DATA_PIN, INPUT);
  pinMode(SHAHE_CLOCK_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SHAHE_CLOCK_PIN), shahe_clock_isr, FALLING);  
  delay(100);
  // I2C_Sensors.begin(SDA_PIN, SCL_PIN, 400000);
  if(display_flag == false) display.displayOff();

  radioInit();
  getSensorData();
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
    delay(500);
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
  uint8_t uplinkData[3];
  encodePayload(uplinkData);
  uint8_t downlinkData[256];
  size_t lenDown = sizeof(downlinkData);

  state = node->sendReceive(uplinkData, sizeof(uplinkData), 1, downlinkData, &lenDown);

  if(state == RADIOLIB_ERR_NONE) {
    Serial.println("Message sent, no downlink received.");
  } else if (state > 0) {
    Serial.println("Message sent, downlink received.");
  } else {
    Serial.printf("sendReceive returned error %d, we'll try again later.\n", state);
  }
}

void getSensorData() {
  int i;
  float position;
  float previous_pos;

  bat = heltec_battery_percent(-1);
  pos = getShahePos();
  //SHT45readTempHum(temp_ext, hum_ext);
  if(display_flag) {
    displayInfoFrame();
  }
}

void encodePayload(uint8_t *uplinkData) {
  int16_t v1 = (int16_t)(pos * 100);
  uplinkData[0] = v1 >> 8;
  uplinkData[1] = v1 & 0xFF;
  uplinkData[2] = bat;
}

void goToSleep() {
  // allows recall of the session after deepsleep
  persist.saveSession(node);
  // Calculate minimum duty cycle delay (per FUP & law!)
  uint32_t interval = node->timeUntilUplink();
  // And then pick it or our MINIMUM_DELAY, whichever is greater
  uint32_t delayMs = max(interval, (uint32_t)MINIMUM_DELAY * 1000);

  if (display_flag){
    both.printf("Going to deep sleep now, Next TX in %i s\n", delayMs/1000);
    delay(5000);  // So message prints
  }
  
  heltec_deep_sleep(delayMs/1000);
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
    // Messbefehl senden
    I2C_Sensors.beginTransmission(SHT45_ADDRESS);
    I2C_Sensors.write(CMD_MEASURE_HIGH_PRECISION);
    if (I2C_Sensors.endTransmission() != 0) {
        both.println("SHT45 com error!");
        temperature = NAN;
        humidity = NAN;
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

  String posStr = "Pos: " + String(pos, 2) + " mm";
  display.drawString(0, y_pos, posStr);
  y_pos += 12;

  String batStr = "BAT: " + String(bat) + " %";
  display.drawString(0, y_pos, batStr);
  y_pos += 12;

  // String extStr = "EXT: " + String(temp_ext, 1) + " °C, " + String(hum_ext, 0) + " %";
  // display.drawString(0, y_pos, extStr);

  display.display();
}

void IRAM_ATTR shahe_clock_isr() {
  static uint32_t data;
  noInterrupts();
  if((micros() - lastClockTime) > clockTimeout) {
    data = 0;
    bitCount = 0;
  }
  if (bitCount < 21) {   
    if (digitalRead(SHAHE_DATA_PIN) ==  HIGH) data |= 1 << (bitCount);
  }
  if (bitCount == 21) {
    data = data >> 1;
    if (digitalRead(SHAHE_DATA_PIN) == HIGH ) data = 0 - data;
    position_raw = data;
    data = 0;
    dataReady = true;
  }
  lastClockTime = micros();
  bitCount = bitCount + 1;
  interrupts();
}

float getShahePos() {
  int i = 0;
  float position;
  float previous_pos;
  while(!dataReady);
  position = (float)position_raw/100.0;
  dataReady = false;
  return position;
}