#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 3000;

unsigned long last_heartbeat_ms = 0;
unsigned long total_rx_bytes = 0;
unsigned long line_count = 0;
char line_buf[256];
int line_pos = 0;

void print_banner() {
  Serial.println();
  Serial.println("ESP32 K230 UART sniffer demo");
  Serial.print("USB Serial baud -> ");
  Serial.println(SERIAL_BAUD);
  Serial.print("Serial1 RX(GPIO) <- K230 TX : ");
  Serial.println(K230_UART_RX_PIN);
  Serial.print("Serial1 TX(GPIO) -> K230 RX : ");
  Serial.println(K230_UART_TX_PIN);
  Serial.print("UART baud -> ");
  Serial.println(K230_UART_BAUD);
  Serial.println("Expected wiring:");
  Serial.println("  ESP32 GPIO16 <- K230 UART TX");
  Serial.println("  ESP32 GPIO15 -> K230 UART RX");
  Serial.println("  GND <-> GND");
  Serial.println();
}

void flush_line() {
  if (line_pos <= 0) {
    return;
  }

  line_buf[line_pos] = '\0';
  ++line_count;
  Serial.print("[UART][LINE ");
  Serial.print(line_count);
  Serial.print("] ");
  Serial.println(line_buf);
  line_pos = 0;
}

void append_line_char(char c) {
  if (line_pos < (int)sizeof(line_buf) - 1) {
    line_buf[line_pos++] = c;
    return;
  }

  flush_line();
}

void log_heartbeat(unsigned long now) {
  if (now - last_heartbeat_ms < HEARTBEAT_INTERVAL_MS) {
    return;
  }

  last_heartbeat_ms = now;
  Serial.print("[UART][HEARTBEAT] bytes=");
  Serial.print(total_rx_bytes);
  Serial.print(", lines=");
  Serial.println(line_count);
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  print_banner();
  Serial1.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
  Serial.println("[UART] sniffer ready, waiting for K230 bytes...");
  last_heartbeat_ms = millis();
}

void loop() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    ++total_rx_bytes;

    Serial.print("[UART][BYTE] 0x");
    if ((uint8_t)c < 0x10) {
      Serial.print('0');
    }
    Serial.print((uint8_t)c, HEX);
    if (c >= 32 && c <= 126) {
      Serial.print(" '");
      Serial.print(c);
      Serial.println("'");
    } else {
      Serial.println();
    }

    if (c == '\n' || c == '\r') {
      flush_line();
    } else {
      append_line_char(c);
    }
  }

  log_heartbeat(millis());
  delay(10);
}
