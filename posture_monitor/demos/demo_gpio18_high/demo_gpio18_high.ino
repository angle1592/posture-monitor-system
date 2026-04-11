#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(FILL_LIGHT_PIN, OUTPUT);
  digitalWrite(FILL_LIGHT_PIN, HIGH);

  Serial.println();
  Serial.println("ESP32 GPIO high-level test demo");
  Serial.print("GPIO");
  Serial.print(FILL_LIGHT_PIN);
  Serial.println(" configured as OUTPUT and driven HIGH");
  Serial.println("Use LED + resistor or multimeter to verify voltage");
}

void loop() {
  delay(1000);
  Serial.print("[GPIO_HIGH] GPIO");
  Serial.print(FILL_LIGHT_PIN);
  Serial.println(" level=HIGH");
}
