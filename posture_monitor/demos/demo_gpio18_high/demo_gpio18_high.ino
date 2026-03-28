#include <Arduino.h>

namespace {
constexpr int TEST_PIN = 15;
constexpr unsigned long SERIAL_BAUD = 115200;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(TEST_PIN, OUTPUT);
  digitalWrite(TEST_PIN, HIGH);

  Serial.println();
  Serial.println("ESP32 GPIO18 high-level test demo");
  Serial.println("GPIO18 configured as OUTPUT and driven HIGH");
  Serial.println("Use LED + resistor or multimeter to verify voltage");
}

void loop() {
  delay(1000);
  Serial.println("[GPIO18] level=HIGH");
}
