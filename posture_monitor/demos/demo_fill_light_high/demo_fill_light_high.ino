#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long PRINT_INTERVAL_MS = 1000;

unsigned long last_print_ms = 0;

void printBanner() {
  Serial.println();
  Serial.println("补光灯高电平测试");
  Serial.print("GPIO");
  Serial.print(FILL_LIGHT_PIN);
  Serial.println(" 持续输出 HIGH，用于检查补光灯焊点/驱动/供电。");
  Serial.println("上电后补光灯应常亮（如果灯和驱动级正常）。");
  Serial.println("如果灯不亮：先万用表测 GPIO13 对 GND 电压。");
  Serial.println("  - 没电压（接近 0V）：焊点/引脚可能虚焊。");
  Serial.println("  - 有 3.3V 但灯不亮：驱动管/MOS 或灯本身/供电共地问题。");
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(FILL_LIGHT_PIN, OUTPUT);
  digitalWrite(FILL_LIGHT_PIN, HIGH);

  printBanner();
  last_print_ms = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_print_ms >= PRINT_INTERVAL_MS) {
    last_print_ms = now;
    int level = digitalRead(FILL_LIGHT_PIN);
    Serial.print("[补光灯] GPIO");
    Serial.print(FILL_LIGHT_PIN);
    Serial.print(" 输出电平=");
    Serial.println(level == HIGH ? "HIGH (3.3V)" : "LOW (0V)");
  }
  delay(20);
}
