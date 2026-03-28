#include <Arduino.h>
#include <Wire.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long SCAN_INTERVAL_MS = 5000;

unsigned long last_scan_ms = 0;

void printAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
}

void printResultCode(uint8_t code) {
  Serial.print("result=");
  Serial.print(code);
  switch (code) {
    case 0:
      Serial.print(" (ACK)");
      break;
    case 1:
      Serial.print(" (data too long)");
      break;
    case 2:
      Serial.print(" (address NACK)");
      break;
    case 3:
      Serial.print(" (data NACK)");
      break;
    case 4:
      Serial.print(" (other error)");
      break;
    default:
      Serial.print(" (unknown)");
      break;
  }
}

void runScan() {
  int found_count = 0;

  Serial.println();
  Serial.println("[I2C] scanning addresses 0x01-0x7E...");

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t result = Wire.endTransmission();

    if (result == 0) {
      Serial.print("[I2C] found device at ");
      printAddress(address);
      Serial.println();
      ++found_count;
      continue;
    }

    if (result == 4) {
      Serial.print("[I2C] error at ");
      printAddress(address);
      Serial.print(" -> ");
      printResultCode(result);
      Serial.println();
    }
  }

  if (found_count == 0) {
    Serial.println("[I2C] no devices found");
  } else {
    Serial.print("[I2C] total devices found: ");
    Serial.println(found_count);
  }

  Serial.println("[I2C] next scan in 5 seconds");
}
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  Serial.println();
  Serial.println("ESP32-S3 I2C scanner demo");
  Serial.print("SDA -> GPIO");
  Serial.println(BH1750_SDA_PIN);
  Serial.print("SCL -> GPIO");
  Serial.println(BH1750_SCL_PIN);
  Serial.println("Scanning full 7-bit I2C address range");

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  delay(50);

  runScan();
  last_scan_ms = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_scan_ms < SCAN_INTERVAL_MS) {
    delay(20);
    return;
  }

  last_scan_ms = now;
  runScan();
}
