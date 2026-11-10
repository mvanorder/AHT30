#include <utility>
#include <Wire.h>

#define BOOT_BUTTON_PIN 9
#define AHT30_ADDR 0x38    // AHT30 Sensor
#define SDA_PIN 22          // GPIO 22 -> Xiao ESP32-C6 board pin 4
#define SCL_PIN 23          // GPIO 23 -> Xiao ESP32-C6 board pin 5

static const byte AHT30_MEASURE_COMMAND[] = {0xAC, 0x33, 0x00};

struct RawData {
  byte status;
  byte sensorBytes[5];
  byte crc;
};

struct Status {
    bool busy : 1;
    bool workingMode_0 : 1; // Bit-fields cannot be standard arrays
    bool workingMode_1 : 1;
    bool crcPassed : 1;
    bool dataCalibrated : 1;
    bool cmpInterruptOk : 1;
    bool reserved_0 : 1;
    bool reserved_1 : 1;
};

bool debug = false;

void setup() {
  Serial.begin(9600);  // Start serial for output
  Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C bus with custom pins
  delay(5);
}

void resetScreen() {
  Serial.printf(
    "\x1b[H"      // Move the cursor to the top left
    "\x1b[J"      // Clear the screen
    "\x1b[0m"     // Reset formatting
  );
};

void printStatus(byte *statusByte) {
  struct Status status;
  memcpy(&status, statusByte, sizeof(status));
  Serial.print("Status:");
  if (debug) {
    Serial.printf(" 0x%02X", *statusByte);
  }
  Serial.printf("\r\n                 Busy: %i\r\n", status.busy);
  Serial.printf("             CMD mode: %i\r\n", status.workingMode_0);
  Serial.printf("             CYC mode: %i\r\n", status.workingMode_1);
  Serial.printf("Integrety test passed: %i\r\n", status.crcPassed);
  Serial.printf("     Data callibrated: %i\r\n", status.dataCalibrated);
  Serial.printf("     CMP interrupt ok: %i\r\n\n", status.cmpInterruptOk);
}

void readRhTemp(byte (*sensorBytes)[5], float &rh, float &temp) {
  uint32_t rhRaw, tempRaw;

  rhRaw =    ((*sensorBytes)[0] << 16 |
              (*sensorBytes)[1] << 8  |
              (*sensorBytes)[2]) >> 4;
  tempRaw = ((*sensorBytes)[2] & 0x0F) << 16 |
             (*sensorBytes)[3]         << 8  |
             (*sensorBytes)[4];

  rh = rhRaw * 100 / 0x100000 - 50;
  temp = tempRaw * 200 / 0x100000 - 50;
  temp = temp * 9 / 5 + 32; // Convert C to F
}

void readAht30(char (*buf)[7]) {
  char c;
  Wire.beginTransmission(AHT30_ADDR);
  Wire.write(AHT30_MEASURE_COMMAND, 4);
  delay(80);
  Wire.write(0x71);
  Wire.endTransmission();
  Wire.requestFrom(AHT30_ADDR, 7);

  if (debug) {
    Serial.print("DEBUG:");
  }
  for (uint8_t i = 0; i < 7; i++) {
    c = Wire.read();
    (*buf)[i] = c;
    if (debug) {
      Serial.printf(" 0x%02X", c);
    }
  }
  if (debug) {
    Serial.print("\r\n");
  }
}

void loop() {
  char buf[7];
  RawData *rawData = (RawData*)buf;

  uint32_t rhRaw, tempRaw;
  float rh, temp;
  char crc;

  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    debug = !debug;
  }

  resetScreen();
  readAht30(&buf);
  crc = buf[7];

  printStatus(&rawData->status);

  readRhTemp(&rawData->sensorBytes, rh, temp);
  Serial.printf("Temperature: %3.1f °F\r\n", temp);
  Serial.printf("Humidity:    %3.1f%%\r\n", rh);
  // Serial.printf(", crc: %i\r\n", crc);
  Serial.print("\r\n");
  
  delay(10000);
}
