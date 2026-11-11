/**
 * @file AHT30.ino
 * @brief AHT30 Sensor demo for the Seeed Studio Xiao ESP32-C6 board.
 * @details This code reads temperature and humidity data from the AHT30 sensor and prints it to
 *          the serial monitor.
 * @author Malcolm VanOrder (mvanorder1390@gmail.com)
 * @date 2026-11-10
 * @version 1.0
 * @copyright Copyright (c) 2026
 *
 * Hardware Requirements:
 * - ESP32
 * - AHT30 temperature and humidity sensor connected to pins 22 (SDA) and 23 (SCL)
 */
#include <utility>
#include <Wire.h>

#define BOOT_BUTTON_PIN 9
#define AHT30_ADDR 0x38     // AHT30 Sensor
#define AHT30_DATA_LENGTH 7 // Sensor response data length
#define SDA_PIN 22          // GPIO 22 -> Xiao ESP32-C6 board pin 4
#define SCL_PIN 23          // GPIO 23 -> Xiao ESP32-C6 board pin 5

static const byte AHT30_MEASURE_COMMAND[] = {0xAC, 0x33, 0x00};

/**
 * @struct RawData
 * @brief Represents the raw data received from the AHT30 sensor.
 * @details The structure contains the status byte, sensor data bytes, and CRC byte.
 */
struct RawData {
  byte status;
  byte sensorBytes[5];
  byte crc;
};

/**
 * @struct Status
 * @brief Represents the status of the AHT30 sensor using bit-fields.
 * @details Each bit in the status byte corresponds to a specific status flag of the sensor.
 */
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

void resetScreen() {
  /**
   * Reset the serial monitor screen
   */
  Serial.printf(
    "\x1b[H"      // Move the cursor to the top left
    "\x1b[J"      // Clear the screen
    "\x1b[0m"     // Reset formatting
  );
};

void printStatus(byte *statusByte) {
  /**
   * Print the status of the AHT30 sensor
   *
   * @param statusByte pointer to the status byte from the sensor data
   */
  struct Status status;
  memcpy(&status, statusByte, sizeof(status));
  Serial.print("Status:");
  if (debug) {
    Serial.printf(" 0x%02X", *statusByte);
  }
  Serial.printf("\r\n"
                "                 Busy: %i\r\n", status.busy);
  Serial.printf("             CMD mode: %i\r\n", status.workingMode_0);
  Serial.printf("             CYC mode: %i\r\n", status.workingMode_1);
  Serial.printf("Integrety test passed: %i\r\n", status.crcPassed);
  Serial.printf("     Data callibrated: %i\r\n", status.dataCalibrated);
  Serial.printf("     CMP interrupt ok: %i\r\n\n", status.cmpInterruptOk);
}

void readRhTemp(byte (*sensorBytes)[5], float &rh, float &temp) {
  /**
   * Read humidity and temperature data from the AHT30 sensor
   *
   * @param sensorBytes pointer to the sensor data bytes
   * @param rh reference to the humidity value
   * @param temp reference to the temperature value
   */
  uint32_t rhRaw, tempRaw;

  rhRaw =   ((*sensorBytes)[0]  << 16 |
             (*sensorBytes)[1]  << 8  |
             (*sensorBytes)[2]) >> 4;
  tempRaw = ((*sensorBytes)[2] & 0x0F) << 16 |
             (*sensorBytes)[3]         << 8  |
             (*sensorBytes)[4];

  rh = rhRaw * 100 / 0x100000;
  temp = tempRaw * 200 / 0x100000 - 50;
  temp = temp * 9 / 5 + 32; // Convert C to F
}

void readAht30(RawData *rawDataStruct) {
  /**
   * Read data from the AHT30 sensor
   *
   * @param rawDataStruct pointer to struct for data from sensor
   */
  
  // Cast rawDataStruct pointer as a byte array pointer to use in the for loop
  byte (*buf)[AHT30_DATA_LENGTH] = (byte (*)[AHT30_DATA_LENGTH])rawDataStruct;

  // Request data from the sensor
  Wire.beginTransmission(AHT30_ADDR);
  Wire.write(AHT30_MEASURE_COMMAND, 4);
  delay(80);
  Wire.write(0x71);
  Wire.endTransmission();
  Wire.requestFrom(AHT30_ADDR, AHT30_DATA_LENGTH);

  if (debug) {
    Serial.print("DEBUG:");
  }

  for (uint8_t i = 0; i < AHT30_DATA_LENGTH; i++) {
    (*buf)[i] = Wire.read();
    if (debug) {
      Serial.printf(" 0x%02X", (byte)(*buf)[i]);
    }
  }
  if (debug) {
    Serial.print("\r\n");
  }
}

unsigned char calcCrc8(unsigned char *message, unsigned char num) {
  /**
   * Calculate the CRC8 checksum for the given message
   *
   * @param message pointer to the message bytes
   * @param num number of bytes in the message
   * @return calculated CRC8 checksum
   */
  unsigned char i;
  unsigned char byte;
  unsigned char crc = 0xFF;
  for (byte = 0; byte < num; byte++) {
    crc ^= (message[byte]);
    for (i = 8; i > 0; --i) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

bool checkCrc(RawData *data) {
  /**
   * Check the CRC8 checksum of the received data
   *
   * @param data pointer to the raw data struct
   * @return true if CRC is valid, false otherwise
   */
  unsigned char calculatedCrc = calcCrc8((unsigned char *)data, 6);
  if (debug) {
    Serial.printf("Calculated CRC: 0x%02X, Received CRC: 0x%02X\r\n", calculatedCrc, data->crc);
  }
  // return (calculatedCrc == *crc) ? true : false;
  return calculatedCrc == data->crc;
}

void setup() {
  /**
   * ESP32 setup
   */
  Serial.begin(115200);         // Start serial for output
  delay(5);                     // Wait 5ms before initializing I2C
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP); // Set boot button pin as input
  Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C bus with custom pins
}

void loop() {
  /**
   * Main loop to read data from the AHT30 sensor and print it to the serial monitor
   */
  struct RawData rawData;
  float rh, temp;

  // Use the boot button to toggle debug output
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    debug = !debug;
  }

  resetScreen();
  readAht30(&rawData);

  if (!checkCrc(&rawData)) {
    Serial.println("Data integrity check failed. Please check the sensor connection.");
    delay(5000);
    return;
  }

  printStatus(&rawData.status);

  readRhTemp(&rawData.sensorBytes, rh, temp);
  Serial.printf("Temperature: %3.1f °F\r\n", temp);
  Serial.printf("Humidity:    %3.1f%%\r\n\n", rh);

  delay(10000);
}
