#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long SEND_INTERVAL_MS = 500;

unsigned long last_send_ms = 0;
unsigned long send_count = 0;

void printBanner() {
  Serial.println();
  Serial.println("ESP32 K230 UART sender demo");
  Serial.print("USB Serial baud -> ");
  Serial.println(SERIAL_BAUD);
  Serial.print("Serial1 TX(GPIO) -> K230 RX : ");
  Serial.println(K230_UART_TX_PIN);
  Serial.print("Serial1 RX(GPIO) <- K230 TX : ");
  Serial.println(K230_UART_RX_PIN);
  Serial.print("UART baud -> ");
  Serial.println(K230_UART_BAUD);
  Serial.println("Expected wiring:");
  Serial.println("  ESP32 GPIO15 -> K230 UART RX");
  Serial.println("  ESP32 GPIO16 <- K230 UART TX");
  Serial.println("  GND <-> GND");
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  printBanner();
  Serial1.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
  Serial.println("[UART] sender ready, sending heartbeat to K230...");
  last_send_ms = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_send_ms >= SEND_INTERVAL_MS) {
    last_send_ms = now;
    ++send_count;

    char buf[64];
    snprintf(buf, sizeof(buf), "ESP32_UART_OK_%lu\n", send_count);
    size_t written = Serial1.print(buf);

    Serial.print("[UART][TX] ");
    Serial.print(buf);
    Serial.print("[UART][STAT] bytes=");
    Serial.println((unsigned long)written);
  }

  delay(10);
}
