#include <Arduino.h>
#include <Wire.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long READ_INTERVAL_MS = 1500;
constexpr unsigned long MEASUREMENT_DELAY_MS = 180;
constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONT_HIGH_RES = 0x10;

bool sensor_ready = false;
unsigned long last_read_ms = 0;

uint8_t probeDevice(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission();
}

void printAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
}

void print_i2c_result(const char* label, uint8_t code) {
  Serial.print("[BH1750] ");
  Serial.print(label);
  Serial.print(" -> I2C result code: ");
  Serial.println(code);
}

bool send_command(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  uint8_t result = Wire.endTransmission();
  if (result != 0) {
    print_i2c_result("command failed", result);
    return false;
  }
  return true;
}

bool initialize_sensor(uint8_t address) {
  if (!send_command(address, BH1750_POWER_ON)) {
    return false;
  }
  Serial.println("[BH1750] power on command ok");

  if (!send_command(address, BH1750_CONT_HIGH_RES)) {
    return false;
  }
  Serial.println("[BH1750] continuous high-resolution mode ok");

  delay(MEASUREMENT_DELAY_MS);
  Serial.print("[BH1750] initial measurement wait complete: ");
  Serial.print(MEASUREMENT_DELAY_MS);
  Serial.println(" ms");
  return true;
}

bool scanBusForTarget(uint8_t target_address) {
  bool target_found = false;
  int found_count = 0;

  Serial.println("[BH1750] running scanner-compatible bus scan...");

  for (uint8_t address = 1; address < 127; ++address) {
    uint8_t result = probeDevice(address);
    if (result == 0) {
      ++found_count;
      Serial.print("[BH1750] found device at ");
      printAddress(address);
      Serial.println();
      if (address == target_address) {
        target_found = true;
      }
    }
  }

  Serial.print("[BH1750] scan total devices found: ");
  Serial.println(found_count);
  if (!target_found) {
    Serial.print("[BH1750] target address not found: ");
    printAddress(target_address);
    Serial.println();
  }

  return target_found;
}

bool read_lux(float& lux_out, uint16_t& raw_out) {
  if (Wire.requestFrom(static_cast<int>(BH1750_I2C_ADDR), 2) != 2) {
    return false;
  }

  raw_out = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  lux_out = raw_out / 1.2f;
  return true;
}

void print_wiring() {
  Serial.println();
  Serial.println("GY-302 BH1750 demo");
  Serial.println("Wiring:");
  Serial.print("  SDA -> GPIO");
  Serial.println(BH1750_SDA_PIN);
  Serial.print("  SCL -> GPIO");
  Serial.println(BH1750_SCL_PIN);
  Serial.println("  VCC -> 3V3");
  Serial.println("  GND -> GND");
  Serial.println("  ADDR -> GND");
  Serial.println("  Fixed I2C address -> 0x23");
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  print_wiring();
  Serial.println("[BH1750] starting I2C bus...");

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  Serial.println("[BH1750] I2C bus started");
  delay(200);

  if (!scanBusForTarget(BH1750_I2C_ADDR)) {
    Serial.println("[BH1750] scanner path did not find the BH1750 target address.");
    Serial.println("[BH1750] Check VCC/GND/SDA/SCL and confirm ADDR is tied to GND.");
    return;
  }

  Serial.println("[BH1750] target address confirmed by scanner path");
  sensor_ready = initialize_sensor(BH1750_I2C_ADDR);

  if (!sensor_ready) {
    Serial.println("[BH1750] address exists but BH1750 mode setup failed.");
    return;
  }

  Serial.println("[BH1750] detected at fixed address 0x23");
}

void loop() {
  if (!sensor_ready) {
    delay(1000);
    return;
  }

  unsigned long now = millis();
  if (now - last_read_ms < READ_INTERVAL_MS) {
    delay(20);
    return;
  }
  last_read_ms = now;

  float lux = 0.0f;
  uint16_t raw = 0;
  if (!read_lux(lux, raw)) {
    Serial.println("[BH1750] read failed, check wiring or power.");
    return;
  }

  Serial.print("[BH1750] raw=");
  Serial.print(raw);
  Serial.print(", lux=");
  Serial.println(lux, 1);
}
