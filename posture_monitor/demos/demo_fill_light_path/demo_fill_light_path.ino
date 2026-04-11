#include <Arduino.h>
#include <Wire.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long READ_INTERVAL_MS = 500;
constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONT_HIGH_RES = 0x10;

bool bh1750_ready = false;
bool presence_ready = false;
bool presence_present = true;
int light_lux = 2000;
bool light_ready = false;
bool fill_light_on = false;
unsigned long boot_ms = 0;
unsigned long last_presence_active_ms = 0;
unsigned long last_read_ms = 0;
unsigned long last_heartbeat_ms = 0;

bool probeDevice(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool sendBh1750Command(uint8_t command) {
  Wire.beginTransmission(BH1750_I2C_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

bool initializeBh1750() {
  if (!probeDevice(BH1750_I2C_ADDR)) {
    return false;
  }

  if (!sendBh1750Command(BH1750_POWER_ON)) {
    return false;
  }

  if (!sendBh1750Command(BH1750_CONT_HIGH_RES)) {
    return false;
  }

  delay(BH1750_MEASUREMENT_DELAY_MS);
  return true;
}

bool readBh1750Lux(int& lux_out, uint16_t& raw_out) {
  if (Wire.requestFrom(static_cast<int>(BH1750_I2C_ADDR), 2) != 2) {
    return false;
  }

  raw_out = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  lux_out = static_cast<int>((raw_out + 1) / 1.2f);
  return true;
}

void printWiring() {
  Serial.println();
  Serial.println("Fill-light path demo");
  Serial.println("Wiring:");
  Serial.println("  PIR / presence sensor");
  Serial.print("    OUT -> GPIO");
  Serial.println(PERSON_SENSOR_PIN);
  Serial.println("    VCC -> sensor spec power");
  Serial.println("    GND -> GND");
  Serial.println("  GY-302 BH1750");
  Serial.print("    SDA -> GPIO");
  Serial.println(BH1750_SDA_PIN);
  Serial.print("    SCL -> GPIO");
  Serial.println(BH1750_SCL_PIN);
  Serial.println("    VCC -> 3V3");
  Serial.println("    GND -> GND");
  Serial.println("    ADDR -> GND");
  Serial.println("  Fill light");
  Serial.print("    CTRL -> GPIO");
  Serial.println(FILL_LIGHT_PIN);
  Serial.println("    Verify power and common ground separately if needed");
  Serial.println();
}

const char* levelText(int level) {
  return level == HIGH ? "HIGH" : "LOW";
}

void printConfigSummary() {
  Serial.print("[CFG] PERSON_SENSOR_TYPE=");
  Serial.println(PERSON_SENSOR_TYPE);
  Serial.print("[CFG] PERSON_SENSOR_PIN=GPIO");
  Serial.println(PERSON_SENSOR_PIN);
  Serial.print("[CFG] PERSON_SENSOR_ACTIVE_LEVEL=");
  Serial.println(levelText(PERSON_SENSOR_ACTIVE_LEVEL));
  Serial.print("[CFG] PERSON_SENSOR_WARMUP_MS=");
  Serial.println(PERSON_SENSOR_WARMUP_MS);
  Serial.print("[CFG] PERSON_SENSOR_HOLD_MS=");
  Serial.println(PERSON_SENSOR_HOLD_MS);
  Serial.print("[CFG] BH1750 addr=0x");
  Serial.println(BH1750_I2C_ADDR, HEX);
  Serial.print("[CFG] FILL_LIGHT_PIN=GPIO");
  Serial.println(FILL_LIGHT_PIN);
  Serial.print("[CFG] FILL_LIGHT_LUX_THRESHOLD=");
  Serial.println(FILL_LIGHT_LUX_THRESHOLD);
}

void updatePresence(unsigned long now, int raw_level, bool raw_active) {
  if (PERSON_SENSOR_TYPE == PERSON_SENSOR_BEAM_BREAK) {
    presence_ready = true;
    presence_present = raw_active;
    return;
  }

  if (!presence_ready) {
    if (now - boot_ms >= PERSON_SENSOR_WARMUP_MS) {
      presence_ready = true;
      Serial.println("[PRESENCE] warm-up complete");
    } else {
      presence_present = true;
      return;
    }
  }

  if (raw_active) {
    last_presence_active_ms = now;
    presence_present = true;
    return;
  }

  if (PERSON_SENSOR_HOLD_MS > 0 && now - last_presence_active_ms < PERSON_SENSOR_HOLD_MS) {
    presence_present = true;
  } else {
    presence_present = false;
  }
}

void updateFillLight() {
  fill_light_on = presence_present && light_ready && light_lux < FILL_LIGHT_LUX_THRESHOLD;
  digitalWrite(FILL_LIGHT_PIN, fill_light_on ? HIGH : LOW);
}

void printDecision(unsigned long now, int raw_level, bool raw_active, uint16_t raw_lux) {
  Serial.print("[STATE] ms=");
  Serial.print(now);
  Serial.print(" rawLevel=");
  Serial.print(levelText(raw_level));
  Serial.print(" rawActiveByConfig=");
  Serial.print(raw_active ? "true" : "false");
  Serial.print(" activeLevel=");
  Serial.print(levelText(PERSON_SENSOR_ACTIVE_LEVEL));
  Serial.print(" presenceReady=");
  Serial.print(presence_ready ? "true" : "false");
  Serial.print(" presencePresent=");
  Serial.print(presence_present ? "true" : "false");
  Serial.print(" bh1750Ready=");
  Serial.print(light_ready ? "true" : "false");
  Serial.print(" rawLux=");
  Serial.print(raw_lux);
  Serial.print(" lux=");
  Serial.print(light_lux);
  Serial.print(" threshold=");
  Serial.print(FILL_LIGHT_LUX_THRESHOLD);
  Serial.print(" fillLightOn=");
  Serial.println(fill_light_on ? "true" : "false");

  if (!presence_present) {
    Serial.println("[DECISION] fill light OFF: presence=false");
  } else if (!light_ready) {
    Serial.println("[DECISION] fill light OFF: BH1750 not ready or read failed");
  } else if (light_lux >= FILL_LIGHT_LUX_THRESHOLD) {
    Serial.print("[DECISION] fill light OFF: lux=");
    Serial.print(light_lux);
    Serial.print(" >= ");
    Serial.println(FILL_LIGHT_LUX_THRESHOLD);
  } else {
    Serial.print("[DECISION] fill light ON: presence=true, lux=");
    Serial.print(light_lux);
    Serial.print(" < ");
    Serial.println(FILL_LIGHT_LUX_THRESHOLD);
  }

  Serial.print("[GPIO] wrote GPIO");
  Serial.print(FILL_LIGHT_PIN);
  Serial.print(" -> ");
  Serial.println(fill_light_on ? "HIGH" : "LOW");

  if (PERSON_SENSOR_TYPE == PERSON_SENSOR_PIR && PERSON_SENSOR_ACTIVE_LEVEL != HIGH) {
    Serial.println("[HINT] Current config treats PIR active level as non-HIGH; confirm sensor polarity matches config.");
  }
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  boot_ms = millis();
  last_presence_active_ms = boot_ms;

  pinMode(PERSON_SENSOR_PIN, INPUT);
  pinMode(FILL_LIGHT_PIN, OUTPUT);
  digitalWrite(FILL_LIGHT_PIN, LOW);

  printWiring();
  printConfigSummary();

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  bh1750_ready = initializeBh1750();
  light_ready = bh1750_ready;

  if (bh1750_ready) {
    Serial.println("[BH1750] initialization ok");
  } else {
    Serial.println("[BH1750] initialization failed; fill light will stay OFF until sensor is readable");
  }

  if (PERSON_SENSOR_TYPE == PERSON_SENSOR_BEAM_BREAK) {
    presence_ready = true;
  }
}

void loop() {
  unsigned long now = millis();

  if (now - last_read_ms < READ_INTERVAL_MS) {
    delay(20);
    return;
  }
  last_read_ms = now;

  int raw_level = digitalRead(PERSON_SENSOR_PIN);
  bool raw_active = raw_level == PERSON_SENSOR_ACTIVE_LEVEL;
  updatePresence(now, raw_level, raw_active);

  uint16_t raw_lux = 0;
  int next_lux = light_lux;
  if (readBh1750Lux(next_lux, raw_lux)) {
    light_lux = next_lux;
    light_ready = true;
  } else {
    light_ready = false;
  }

  updateFillLight();
  printDecision(now, raw_level, raw_active, raw_lux);

  if (now - last_heartbeat_ms >= 5000UL) {
    last_heartbeat_ms = now;
    Serial.println("[HEARTBEAT] demo alive");
  }
}
