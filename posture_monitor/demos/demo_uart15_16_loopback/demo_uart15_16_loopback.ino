#include <Arduino.h>

namespace {
constexpr unsigned long USB_SERIAL_BAUD = 115200;
constexpr unsigned long UART_BAUD = 9600;
constexpr int UART_TX_PIN = 15;
constexpr int UART_RX_PIN = 16;
constexpr unsigned long SEND_INTERVAL_MS = 1000;
constexpr unsigned long SUMMARY_INTERVAL_MS = 5000;

unsigned long last_send_ms = 0;
unsigned long last_summary_ms = 0;
unsigned long tx_count = 0;
unsigned long rx_bytes = 0;
unsigned long rx_lines = 0;
char line_buf[128];
int line_pos = 0;

void printBanner() {
  Serial.println();
  Serial.println("ESP32 UART GPIO15->GPIO16 loopback demo");
  Serial.println("Purpose: verify whether GPIO15 TX and GPIO16 RX work on this board");
  Serial.print("USB Serial baud -> ");
  Serial.println(USB_SERIAL_BAUD);
  Serial.print("UART baud -> ");
  Serial.println(UART_BAUD);
  Serial.println("Required wiring:");
  Serial.println("  ESP32 GPIO15 (TX) -> ESP32 GPIO16 (RX)");
  Serial.println("  No K230 required for this test");
  Serial.println();
}

void flushLine() {
  if (line_pos <= 0) {
    return;
  }

  line_buf[line_pos] = '\0';
  ++rx_lines;
  Serial.print("[UART15_16][RX_LINE ");
  Serial.print(rx_lines);
  Serial.print("] ");
  Serial.println(line_buf);
  line_pos = 0;
}

void appendLineChar(char c) {
  if (line_pos < static_cast<int>(sizeof(line_buf)) - 1) {
    line_buf[line_pos++] = c;
    return;
  }

  Serial.println("[UART15_16][WARN] line buffer full, flushing current line");
  flushLine();
}

void sendProbe(unsigned long now) {
  if (now - last_send_ms < SEND_INTERVAL_MS) {
    return;
  }

  last_send_ms = now;
  ++tx_count;

  char payload[64];
  snprintf(payload, sizeof(payload), "UART15_16_PROBE_%lu\n", tx_count);
  size_t written = Serial1.print(payload);

  Serial.print("[UART15_16][TX] ");
  Serial.print(payload);
  Serial.print("[UART15_16][TX_STAT] bytes=");
  Serial.println(static_cast<unsigned long>(written));
}

void receiveLoopback() {
  while (Serial1.available()) {
    char c = static_cast<char>(Serial1.read());
    ++rx_bytes;

    if (c == '\n' || c == '\r') {
      flushLine();
    } else {
      appendLineChar(c);
    }
  }
}

void printSummary(unsigned long now) {
  if (now - last_summary_ms < SUMMARY_INTERVAL_MS) {
    return;
  }

  last_summary_ms = now;
  Serial.print("[UART15_16][SUMMARY] tx=");
  Serial.print(tx_count);
  Serial.print(", rxBytes=");
  Serial.print(rx_bytes);
  Serial.print(", rxLines=");
  Serial.println(rx_lines);

  if (tx_count > 0 && rx_lines == 0) {
    Serial.println("[UART15_16][WARN] no loopback line received yet; check jumper between GPIO15 and GPIO16");
  }
}
}  // namespace

void setup() {
  Serial.begin(USB_SERIAL_BAUD);
  delay(200);

  printBanner();
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("[UART15_16] ready");

  unsigned long now = millis();
  last_send_ms = now;
  last_summary_ms = now;
}

void loop() {
  unsigned long now = millis();
  sendProbe(now);
  receiveLoopback();
  printSummary(now);
  delay(10);
}
