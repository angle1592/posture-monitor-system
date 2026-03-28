#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long WARMUP_MS = 30000;
constexpr unsigned long POLL_INTERVAL_MS = 100;
constexpr unsigned long HEARTBEAT_MS = 5000;
constexpr uint8_t DEMO_PIR_PIN = 4;

bool last_motion_state = false;
unsigned long boot_ms = 0;
unsigned long last_poll_ms = 0;
unsigned long last_heartbeat_ms = 0;

bool read_motion() {
  return digitalRead(DEMO_PIR_PIN) == HIGH;
}

void print_wiring() {
  Serial.println();
  Serial.println("HC-SR501 demo");
  Serial.println("Wiring:");
  Serial.println("  VCC -> 5V");
  Serial.println("  GND -> GND");
  Serial.print("  OUT -> GPIO");
  Serial.println(DEMO_PIR_PIN);
  Serial.print("  Note: config PIR_PIN is still GPIO");
  Serial.println(PIR_PIN);
  Serial.println();
}

void print_state(bool motion) {
  Serial.print("[PIR] ");
  Serial.println(motion ? "MOTION DETECTED" : "NO MOTION");
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(DEMO_PIR_PIN, INPUT_PULLDOWN);
  boot_ms = millis();

  print_wiring();
  Serial.println("Warm-up started. HC-SR501 may need tens of seconds to stabilize.");

  last_motion_state = read_motion();
  print_state(last_motion_state);
}

void loop() {
  unsigned long now = millis();

  if (now - boot_ms < WARMUP_MS) {
    if (now - last_heartbeat_ms >= HEARTBEAT_MS) {
      last_heartbeat_ms = now;
      Serial.print("[PIR] warming up... remaining ms: ");
      Serial.println(WARMUP_MS - (now - boot_ms));
    }
    delay(20);
    return;
  }

  if (now - last_poll_ms < POLL_INTERVAL_MS) {
    delay(10);
    return;
  }
  last_poll_ms = now;

  bool motion = read_motion();
  if (motion != last_motion_state) {
    last_motion_state = motion;
    print_state(motion);
  }

  if (now - last_heartbeat_ms >= HEARTBEAT_MS) {
    last_heartbeat_ms = now;
    Serial.print("[PIR] heartbeat, current state: ");
    Serial.println(motion ? "HIGH" : "LOW");
  }
}
