#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long TOGGLE_INTERVAL_MS = 2000;

bool output_high = false;
unsigned long last_toggle_ms = 0;

void applyOutputLevel(bool high) {
  digitalWrite(FILL_LIGHT_PIN, high ? HIGH : LOW);
}

void printState(bool high) {
  Serial.print("[GPIO18_TOGGLE] GPIO");
  Serial.print(FILL_LIGHT_PIN);
  Serial.print(" -> ");
  Serial.println(high ? "HIGH" : "LOW");

  if (high) {
    Serial.println("[GPIO18_TOGGLE] If the fill light turns ON only now, hardware is likely HIGH-active.");
  } else {
    Serial.println("[GPIO18_TOGGLE] If the fill light turns ON only now, hardware is likely LOW-active.");
  }
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(FILL_LIGHT_PIN, OUTPUT);
  applyOutputLevel(output_high);

  Serial.println();
  Serial.println("ESP32 fill-light polarity test demo");
  Serial.print("Using GPIO");
  Serial.println(FILL_LIGHT_PIN);
  Serial.println("This demo alternates the control pin between LOW and HIGH every 2 seconds.");
  Serial.println("Watch the lamp and note which level actually turns it ON.");
  printState(output_high);
}

void loop() {
  unsigned long now = millis();
  if (now - last_toggle_ms < TOGGLE_INTERVAL_MS) {
    delay(20);
    return;
  }

  last_toggle_ms = now;
  output_high = !output_high;
  applyOutputLevel(output_high);
  printState(output_high);
}
