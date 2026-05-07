#include <Arduino.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long PRINT_INTERVAL_MS = 1000;

unsigned long last_print_ms = 0;

void printBanner() {
  Serial.println();
  Serial.println("蜂鸣器高电平测试");
  Serial.print("GPIO");
  Serial.print(BUZZER_PIN);
  Serial.println(" 持续输出 HIGH，用于检查蜂鸣器供电/焊点。");
  Serial.println("上电后万用表测 GPIO6 对 GND 电压，正常接近 3.3V。");
  Serial.println("可听：正常蜂鸣器会持续鸣叫。");
  Serial.println("无万用表时，可轻触 GPIO6 和 GND 判断电平稳定性。");
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  printBanner();
  last_print_ms = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_print_ms >= PRINT_INTERVAL_MS) {
    last_print_ms = now;
    int level = digitalRead(BUZZER_PIN);
    Serial.print("[蜂鸣器] GPIO");
    Serial.print(BUZZER_PIN);
    Serial.print(" 输出电平=");
    Serial.println(level == HIGH ? "HIGH (3.3V)" : "LOW (0V)");
  }
  delay(20);
}
