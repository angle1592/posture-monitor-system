#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long SEND_INTERVAL_MS = 500;

unsigned long last_send_ms = 0;
unsigned long send_count = 0;
unsigned long recv_count = 0;
char line_buf[128];
int line_pos = 0;

void printBanner() {
  Serial.println();
  Serial.println("ESP32 UART loopback demo");
  Serial.print("USB Serial baud -> ");
  Serial.println(SERIAL_BAUD);
  Serial.print("Serial1 TX(GPIO) -> ");
  Serial.println(K230_UART_TX_PIN);
  Serial.print("Serial1 RX(GPIO) <- ");
  Serial.println(K230_UART_RX_PIN);
  Serial.print("UART baud -> ");
  Serial.println(K230_UART_BAUD);
  Serial.println("Loopback wiring required:");
  Serial.println("  Connect ESP32 GPIO15 directly to GPIO16");
  Serial.println();
}

void flushLine() {
  if (line_pos <= 0) {
    return;
  }

  line_buf[line_pos] = '\0';
  Serial.print("[LOOPBACK][LINE] ");
  Serial.println(line_buf);
  line_pos = 0;
}

void appendLineChar(char c) {
  if (line_pos < (int)sizeof(line_buf) - 1) {
    line_buf[line_pos++] = c;
    return;
  }

  flushLine();
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  printBanner();
  Serial1.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
  Serial.println("[LOOPBACK] ready");
  last_send_ms = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_send_ms >= SEND_INTERVAL_MS) {
    last_send_ms = now;
    ++send_count;

    char buf[64];
    snprintf(buf, sizeof(buf), "LOOPBACK_%lu\n", send_count);
    Serial1.print(buf);
    Serial.print("[LOOPBACK][TX] ");
    Serial.print(buf);
  }

  while (Serial1.available()) {
    char c = (char)Serial1.read();
    ++recv_count;

    Serial.print("[LOOPBACK][BYTE] 0x");
    if ((uint8_t)c < 0x10) {
      Serial.print('0');
    }
    Serial.println((uint8_t)c, HEX);

    if (c == '\n' || c == '\r') {
      flushLine();
    } else {
      appendLineChar(c);
    }
  }

  if (send_count && (send_count % 10 == 0) && recv_count == 0) {
    Serial.println("[LOOPBACK][WARN] no bytes received yet");
  }

  delay(10);
}
