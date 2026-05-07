#include <Arduino.h>
#include <Wire.h>

#include "../../config.h"

namespace {
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long SUMMARY_INTERVAL_MS = 2000;
constexpr unsigned long UART_PROBE_INTERVAL_MS = 1000;
constexpr unsigned long INPUT_SAMPLE_INTERVAL_MS = 120;
constexpr unsigned long INPUT_REPORT_INTERVAL_MS = 1000;
constexpr unsigned long OUTPUT_STEP_MS = 650;
constexpr unsigned long BH1750_READ_INTERVAL_MS = 1500;

constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONT_HIGH_RES = 0x10;

HardwareSerial& k230Serial = Serial1;
HardwareSerial& voiceSerial = Serial2;

bool bh1750_ready = false;
bool output_test_active = false;
bool watch_inputs = true;
bool k230_uart_enabled = false;
unsigned long last_summary_ms = 0;
unsigned long last_uart_probe_ms = 0;
unsigned long last_input_sample_ms = 0;
unsigned long last_input_report_ms = 0;
unsigned long last_bh1750_read_ms = 0;
unsigned long output_test_start_ms = 0;
unsigned long uart_tx_count = 0;
unsigned long uart_rx_bytes = 0;
unsigned long uart_rx_lines = 0;
unsigned long uart_empty_line_breaks = 0;
unsigned long uart_printable_bytes = 0;
unsigned long uart_control_bytes = 0;
unsigned long uart_last_rx_ms = 0;
unsigned long bh1750_read_ok = 0;
unsigned long bh1750_read_fail = 0;
char uart_line_buf[192];
int uart_line_pos = 0;

struct InputSnapshot {
  int presence;
  int ec11_a;
  int ec11_b;
  int ec11_key;
};

InputSnapshot last_inputs = {LOW, LOW, LOW, LOW};
bool have_last_inputs = false;

const char* levelText(int level) {
  return level == HIGH ? "HIGH" : "LOW";
}

void printAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
}

void printByteHex(uint8_t value) {
  Serial.print("0x");
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

const char* i2cResultText(uint8_t code) {
  switch (code) {
    case 0:
      return "ACK，设备有响应";
    case 1:
      return "数据过长";
    case 2:
      return "地址无应答，常见于未上电/断线/地址不对";
    case 3:
      return "数据无应答，常见于总线接触不良";
    case 4:
      return "其他总线错误，优先怀疑 SDA/SCL 短路或虚焊";
    default:
      return "未知返回码";
  }
}

uint8_t probeI2c(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission();
}

bool sendBh1750Command(uint8_t command) {
  Wire.beginTransmission(BH1750_I2C_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

bool initBh1750() {
  if (probeI2c(BH1750_I2C_ADDR) != 0) {
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

bool readBh1750(float& lux_out, uint16_t& raw_out) {
  if (Wire.requestFrom(static_cast<int>(BH1750_I2C_ADDR), 2) != 2) {
    return false;
  }
  raw_out = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  lux_out = raw_out / 1.2f;
  return true;
}

void printBanner() {
  Serial.println();
  Serial.println("洞洞板硬件巡检 Demo");
  Serial.println("用途：迁移到洞洞板后，逐项定位虚焊、断线或接触不良。");
  Serial.println("串口监视器：115200 波特率，换行符 LF 或 CRLF 均可。");
  Serial.println();
  Serial.println("当前接线配置来自 config.h：");
  Serial.print("  I2C SDA GPIO");
  Serial.print(BH1750_SDA_PIN);
  Serial.print("，SCL GPIO");
  Serial.println(BH1750_SCL_PIN);
  Serial.print("  BH1750 预期地址 ");
  printAddress(BH1750_I2C_ADDR);
  Serial.print("，OLED 预期地址 ");
  printAddress(OLED_I2C_ADDR);
  Serial.println();
  Serial.print("  人体存在传感器 OUT GPIO");
  Serial.print(PERSON_SENSOR_PIN);
  Serial.print("，有效电平 ");
  Serial.println(levelText(PERSON_SENSOR_ACTIVE_LEVEL));
  Serial.print("  补光灯控制 GPIO");
  Serial.println(FILL_LIGHT_PIN);
  Serial.print("  蜂鸣器 GPIO");
  Serial.println(BUZZER_PIN);
  Serial.print("  WS2812 GPIO");
  Serial.println(WS2812_PIN);
  Serial.print("  EC11 A GPIO");
  Serial.print(EC11_S1_PIN);
  Serial.print("，B GPIO");
  Serial.print(EC11_S2_PIN);
  Serial.print("，KEY GPIO");
  Serial.println(EC11_KEY_PIN);
  Serial.print("  K230 UART TX GPIO");
  Serial.print(K230_UART_TX_PIN);
  Serial.print("，RX GPIO");
  Serial.print(K230_UART_RX_PIN);
  Serial.print("，波特率 ");
  Serial.println(K230_UART_BAUD);
  Serial.print("  SYN6288 TX GPIO");
  Serial.print(VOICE_TX_PIN);
  Serial.print("，RX GPIO");
  Serial.print(VOICE_RX_PIN);
  Serial.print("，波特率 ");
  Serial.println(VOICE_BAUD);
  Serial.println();
  Serial.println("可输入命令：help, i2c, bh1750, inputs, outputs, uart on, uart off, uart, voice, all, watch on, watch off");
  Serial.println("提示：一次轻晃一根线或一个模块，观察日志是否出现电平、I2C 或 UART 断续变化；变化点通常就是可疑焊点。");
  Serial.println("说明：K230 UART 默认关闭；K230 未接时 GPIO16 会悬空，可能把噪声误判成串口字节。接上 K230 后输入 uart on 再测。");
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("命令说明：");
  Serial.println("  help      - 显示这份帮助");
  Serial.println("  i2c       - 扫描 GPIO1/GPIO2 I2C 总线，并标出 BH1750/OLED 地址");
  Serial.println("  bh1750    - 初始化 BH1750 并读取一次光照值");
  Serial.println("  inputs    - 打印一次人体存在传感器和 EC11 的原始电平");
  Serial.println("  outputs   - 运行补光灯、蜂鸣器、WS2812 可见/可听测试");
  Serial.println("  uart on   - 启用 K230 UART 测试（接上 K230 后再用）");
  Serial.println("  uart off  - 关闭 K230 UART 测试，避免 GPIO16 悬空噪声误导");
  Serial.println("  uart      - 发送一次 K230 UART 探测行，并打印接收计数");
  Serial.println("  voice     - 发送一次 SYN6288 固定欢迎语音包");
  Serial.println("  all       - 依次执行 i2c、bh1750、inputs、outputs、uart、voice");
  Serial.println("  watch on  - 持续打印输入电平变化");
  Serial.println("  watch off - 停止持续打印输入电平变化");
  Serial.println();
}

void scanI2c() {
  int found_count = 0;
  bool found_bh1750 = false;
  bool found_oled = false;

  Serial.println();
  Serial.println("[I2C] 正在扫描地址 0x01-0x7E...");
  for (uint8_t address = 1; address < 127; ++address) {
    uint8_t result = probeI2c(address);
    if (result == 0) {
      ++found_count;
      Serial.print("[I2C][正常] 发现设备 ");
      printAddress(address);
      if (address == BH1750_I2C_ADDR) {
        found_bh1750 = true;
        Serial.print(" BH1750");
      }
      if (address == OLED_I2C_ADDR) {
        found_oled = true;
        Serial.print(" OLED");
      }
      Serial.println();
    } else if (result == 4) {
      Serial.print("[I2C][警告] 总线错误，地址 ");
      printAddress(address);
      Serial.println("，result=4");
    }
  }

  Serial.print("[I2C] 共发现设备数量：");
  Serial.println(found_count);
  Serial.print("[I2C] BH1750 ");
  printAddress(BH1750_I2C_ADDR);
  Serial.println(found_bh1750 ? " -> 通过" : " -> 失败/无 ACK");
  Serial.print("[I2C] OLED ");
  printAddress(OLED_I2C_ADDR);
  Serial.println(found_oled ? " -> 通过" : " -> 失败/无 ACK");
  if (found_count == 0) {
    Serial.println("[I2C][提示] 没扫到任何设备：优先检查 3V3、GND、SDA/SCL 连续性、SDA/SCL 是否接反，以及共享 I2C 总线焊点。");
  }
}

void checkBh1750() {
  Serial.println();
  Serial.println("[BH1750] 正在检查光照传感器...");
  uint8_t probe_result = probeI2c(BH1750_I2C_ADDR);
  Serial.print("[BH1750] 地址 ");
  printAddress(BH1750_I2C_ADDR);
  Serial.print(" 探测返回码=");
  Serial.print(probe_result);
  Serial.print("（");
  Serial.print(i2cResultText(probe_result));
  Serial.println("）");

  bh1750_ready = initBh1750();
  if (!bh1750_ready) {
    Serial.println("[BH1750][失败] 初始化失败。请检查 VCC/GND/SDA/SCL/ADDR，以及 I2C 焊点。");
    Serial.println("[BH1750][判断] 如果 OLED 也扫不到，多半是 GPIO1/GPIO2 总线或供电共地问题；如果只有 BH1750 扫不到，优先查 BH1750 模块和 ADDR 焊点。");
    return;
  }

  float lux = 0.0f;
  uint16_t raw = 0;
  if (!readBh1750(lux, raw)) {
    ++bh1750_read_fail;
    Serial.println("[BH1750][失败] 初始化后读取失败。轻晃 SDA/SCL/VCC/GND，观察是否会间歇恢复。");
    return;
  }

  ++bh1750_read_ok;
  Serial.print("[BH1750][通过] 原始值 raw=");
  Serial.print(raw);
  Serial.print("，照度 lux=");
  Serial.println(lux, 1);
  Serial.println("[BH1750][人工判断] 遮挡/照亮传感器，lux 应该明显变化。");
}

InputSnapshot readInputs() {
  InputSnapshot s;
  s.presence = digitalRead(PERSON_SENSOR_PIN);
  s.ec11_a = digitalRead(EC11_S1_PIN);
  s.ec11_b = digitalRead(EC11_S2_PIN);
  s.ec11_key = digitalRead(EC11_KEY_PIN);
  return s;
}

void printInputs(const InputSnapshot& s, const char* label) {
  Serial.print("[输入]");
  Serial.print(label);
  Serial.print(" 人体存在(GPIO");
  Serial.print(PERSON_SENSOR_PIN);
  Serial.print(")=");
  Serial.print(levelText(s.presence));
  Serial.print(" 有效=");
  Serial.print(s.presence == PERSON_SENSOR_ACTIVE_LEVEL ? "是" : "否");
  Serial.print(" EC11_A(GPIO");
  Serial.print(EC11_S1_PIN);
  Serial.print(")=");
  Serial.print(levelText(s.ec11_a));
  Serial.print(" EC11_B(GPIO");
  Serial.print(EC11_S2_PIN);
  Serial.print(")=");
  Serial.print(levelText(s.ec11_b));
  Serial.print(" EC11_KEY(GPIO");
  Serial.print(EC11_KEY_PIN);
  Serial.print(")=");
  Serial.println(levelText(s.ec11_key));
}

void checkInputs() {
  InputSnapshot s = readInputs();
  printInputs(s, "[单次]");
  Serial.println("[输入][人工判断] 遮挡/放开人体存在传感器、旋转 EC11、按下 KEY，对应电平必须变化。");
}

void setWs2812(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(WS2812_PIN, r, g, b);
}

void startOutputTest() {
  output_test_active = true;
  output_test_start_ms = millis();
  Serial.println();
  Serial.println("[输出] 测试开始。请观察补光灯、蜂鸣器和 WS2812 颜色，持续约 6 秒。");
}

void stopOutputs() {
  digitalWrite(FILL_LIGHT_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  setWs2812(0, 0, 0);
}

void updateOutputTest() {
  if (!output_test_active) {
    return;
  }

  unsigned long elapsed = millis() - output_test_start_ms;
  unsigned long step = elapsed / OUTPUT_STEP_MS;

  switch (step) {
    case 0:
      digitalWrite(FILL_LIGHT_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      setWs2812(48, 0, 0);
      break;
    case 1:
      digitalWrite(FILL_LIGHT_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      setWs2812(0, 48, 0);
      break;
    case 2:
      digitalWrite(FILL_LIGHT_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      setWs2812(0, 0, 48);
      break;
    case 3:
      digitalWrite(FILL_LIGHT_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      setWs2812(48, 48, 0);
      break;
    case 4:
      digitalWrite(FILL_LIGHT_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      setWs2812(0, 48, 48);
      break;
    case 5:
      digitalWrite(FILL_LIGHT_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      setWs2812(48, 0, 48);
      break;
    default:
      stopOutputs();
      output_test_active = false;
      Serial.println("[输出] 测试结束。如果日志显示已切换，但实物无反应，请检查对应输出脚焊点、负载供电、极性和共地。");
      break;
  }
}

void resetUartCounters() {
  uart_tx_count = 0;
  uart_rx_bytes = 0;
  uart_rx_lines = 0;
  uart_empty_line_breaks = 0;
  uart_printable_bytes = 0;
  uart_control_bytes = 0;
  uart_last_rx_ms = 0;
  uart_line_pos = 0;
}

void enableK230Uart() {
  if (k230_uart_enabled) {
    Serial.println("[UART] K230 UART 测试已经开启。");
    return;
  }

  resetUartCounters();
  k230Serial.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
  k230_uart_enabled = true;
  last_uart_probe_ms = millis();
  Serial.print("[UART] 已开启 K230 UART 测试：TX GPIO");
  Serial.print(K230_UART_TX_PIN);
  Serial.print("，RX GPIO");
  Serial.print(K230_UART_RX_PIN);
  Serial.print("，波特率 ");
  Serial.println(K230_UART_BAUD);
  Serial.println("[UART][提示] 请确认 K230 TX -> ESP32 GPIO16、K230 RX <- ESP32 GPIO15、GND 共地。未接 K230 时请保持 uart off。");
}

void disableK230Uart() {
  if (!k230_uart_enabled) {
    Serial.println("[UART] K230 UART 测试已经关闭。");
    return;
  }

  k230Serial.end();
  k230_uart_enabled = false;
  pinMode(K230_UART_RX_PIN, INPUT_PULLUP);
  pinMode(K230_UART_TX_PIN, INPUT);
  Serial.println("[UART] 已关闭 K230 UART 测试。GPIO16 改为上拉输入，避免未接线时悬空噪声刷屏。");
}

void sendK230Probe() {
  if (!k230_uart_enabled) {
    Serial.println("[UART][跳过] K230 UART 测试未开启。接上 K230 后输入 uart on；未接 K230 时这一路日志没有诊断意义。");
    return;
  }

  ++uart_tx_count;
  char payload[64];
  snprintf(payload, sizeof(payload), "ESP32_PERFBOARD_PROBE_%lu\n", uart_tx_count);
  k230Serial.print(payload);
  Serial.print("[UART][发送] ");
  Serial.print(payload);
}

void printEscapedUartLine() {
  Serial.print("内容=\"");
  for (int i = 0; i < uart_line_pos; ++i) {
    uint8_t value = static_cast<uint8_t>(uart_line_buf[i]);
    if (value >= 32 && value <= 126) {
      Serial.print(static_cast<char>(value));
    } else {
      Serial.print("\\x");
      if (value < 0x10) {
        Serial.print('0');
      }
      Serial.print(value, HEX);
    }
  }
  Serial.print("\" 十六进制=[");
  for (int i = 0; i < uart_line_pos; ++i) {
    if (i > 0) {
      Serial.print(' ');
    }
    printByteHex(static_cast<uint8_t>(uart_line_buf[i]));
  }
  Serial.print(']');
}

void flushUartLine() {
  if (uart_line_pos <= 0) {
    ++uart_empty_line_breaks;
    return;
  }
  uart_line_buf[uart_line_pos] = '\0';
  ++uart_rx_lines;
  Serial.print("[UART][接收行 ");
  Serial.print(uart_rx_lines);
  Serial.print("] 长度=");
  Serial.print(uart_line_pos);
  Serial.print(' ');
  printEscapedUartLine();
  Serial.println();
  uart_line_pos = 0;
}

void appendUartChar(char c) {
  if (uart_line_pos < static_cast<int>(sizeof(uart_line_buf)) - 1) {
    uart_line_buf[uart_line_pos++] = c;
    return;
  }
  flushUartLine();
}

void readK230Uart() {
  if (!k230_uart_enabled) {
    return;
  }

  while (k230Serial.available()) {
    char c = static_cast<char>(k230Serial.read());
    ++uart_rx_bytes;
    uart_last_rx_ms = millis();
    uint8_t value = static_cast<uint8_t>(c);
    if (value >= 32 && value <= 126) {
      ++uart_printable_bytes;
    } else {
      ++uart_control_bytes;
    }
    if (c == '\n' || c == '\r') {
      flushUartLine();
    } else {
      appendUartChar(c);
    }
  }
}

void printUartStatus() {
  if (!k230_uart_enabled) {
    Serial.println("[UART][状态] K230 UART 测试关闭。K230 未接时这是正确状态；接上 K230 后输入 uart on 再测。");
    return;
  }

  Serial.print("[UART][状态] 已发送 tx=");
  Serial.print(uart_tx_count);
  Serial.print("，收到字节 rxBytes=");
  Serial.print(uart_rx_bytes);
  Serial.print("，收到行 rxLines=");
  Serial.println(uart_rx_lines);
  Serial.print("[UART][状态] 可打印字节=");
  Serial.print(uart_printable_bytes);
  Serial.print("，控制/不可见字节=");
  Serial.print(uart_control_bytes);
  Serial.print("，空换行=");
  Serial.print(uart_empty_line_breaks);
  if (uart_last_rx_ms > 0) {
    Serial.print("，距上次收到字节ms=");
    Serial.println(millis() - uart_last_rx_ms);
  } else {
    Serial.println("，尚未收到任何字节");
  }
  if (uart_rx_bytes == 0) {
    Serial.println("[UART][提示] 目前没有收到字节。请检查 K230 TX -> ESP32 GPIO16、共地；若只测 ESP32 引脚，请运行 demo_uart15_16_loopback。");
  } else if (uart_rx_lines == 0 || uart_printable_bytes == 0) {
    Serial.println("[UART][提示] 物理 RX 已收到电平变化，但没有形成可读文本；优先检查波特率、K230 是否真在发 JSON、TX/RX 是否接反或信号线虚焊干扰。");
  }
}

void sendVoiceWelcomePacket() {
  static const uint8_t packet_welcome[] = {
      0xFD, 0x00, 0x13, 0x01, 0x00,
      0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
      0xBB, 0xB6, 0xD3, 0xAD, 0xCA, 0xB9, 0xD3, 0xC3, 0xF6};

  voiceSerial.write(packet_welcome, sizeof(packet_welcome));
  Serial.print("[语音] 已发送 SYN6288 欢迎语音包，字节数=");
  Serial.println(sizeof(packet_welcome));
  Serial.println("[语音][人工判断] 如果没有声音，请检查 SYN6288 VCC/GND、喇叭、ESP32 GPIO41 -> SYN6288 RX，以及共地。");
}

void autoBh1750Read(unsigned long now) {
  if (now - last_bh1750_read_ms < BH1750_READ_INTERVAL_MS) {
    return;
  }
  last_bh1750_read_ms = now;

  if (!bh1750_ready) {
    bh1750_ready = initBh1750();
    if (!bh1750_ready) {
    ++bh1750_read_fail;
    if (bh1750_read_fail == 1 || bh1750_read_fail % 20 == 0) {
      Serial.print("[BH1750][自动][提示] 已连续/累计失败 ");
      Serial.print(bh1750_read_fail);
      Serial.println(" 次。建议输入 i2c 看 0x23/0x3C 是否存在，再轻晃 SDA/SCL/VCC/GND 观察是否恢复。");
    }
    return;
  }
  }

  float lux = 0.0f;
  uint16_t raw = 0;
  if (readBh1750(lux, raw)) {
    ++bh1750_read_ok;
    Serial.print("[BH1750][自动] 原始值 raw=");
    Serial.print(raw);
    Serial.print("，照度 lux=");
    Serial.println(lux, 1);
  } else {
    ++bh1750_read_fail;
    bh1750_ready = false;
    Serial.println("[BH1750][自动][失败] 读取失败");
    if (bh1750_read_fail % 20 == 0) {
      Serial.println("[BH1750][自动][提示] 初始化曾通过但读取失败，常见于 SDA/SCL 接触不良或供电不稳。");
    }
  }
}

void watchInputChanges(unsigned long now) {
  if (!watch_inputs || now - last_input_sample_ms < INPUT_SAMPLE_INTERVAL_MS) {
    return;
  }
  last_input_sample_ms = now;

  InputSnapshot current = readInputs();
  bool changed = !have_last_inputs || current.presence != last_inputs.presence || current.ec11_a != last_inputs.ec11_a || current.ec11_b != last_inputs.ec11_b || current.ec11_key != last_inputs.ec11_key;
  if (changed) {
    printInputs(current, "[变化]");
    last_inputs = current;
    have_last_inputs = true;
    last_input_report_ms = now;
    return;
  }

  if (now - last_input_report_ms >= INPUT_REPORT_INTERVAL_MS) {
    printInputs(current, "[监视]");
    last_inputs = current;
    have_last_inputs = true;
    last_input_report_ms = now;
  }
}

void periodicUartProbe(unsigned long now) {
  if (!k230_uart_enabled) {
    return;
  }

  if (now - last_uart_probe_ms < UART_PROBE_INTERVAL_MS) {
    return;
  }
  last_uart_probe_ms = now;
  sendK230Probe();
}

void printSummary(unsigned long now) {
  if (now - last_summary_ms < SUMMARY_INTERVAL_MS) {
    return;
  }
  last_summary_ms = now;

  Serial.print("[摘要] 运行时间ms=");
  Serial.print(now);
  Serial.print(" BH1750成功=");
  Serial.print(bh1750_read_ok);
  Serial.print(" BH1750失败=");
  Serial.print(bh1750_read_fail);
  Serial.print(" K230_UART=");
  Serial.print(k230_uart_enabled ? "开启" : "关闭");
  Serial.print(" UART发送=");
  Serial.print(uart_tx_count);
  Serial.print(" UART收字节=");
  Serial.print(uart_rx_bytes);
  Serial.print(" UART收行=");
  Serial.print(uart_rx_lines);
  Serial.print(" UART空换行=");
  Serial.print(uart_empty_line_breaks);
  Serial.print(" UART可见/不可见=");
  Serial.print(uart_printable_bytes);
  Serial.print('/');
  Serial.print(uart_control_bytes);
  Serial.print(" 输出测试=");
  Serial.println(output_test_active ? "运行中" : "空闲");

  if (bh1750_read_ok == 0 && bh1750_read_fail > 0) {
    Serial.println("[摘要][判断] BH1750 仍未成功读取：当前重点排查 I2C 的 3V3/GND/SDA/SCL/ADDR。");
  }
  if (k230_uart_enabled && uart_rx_bytes > 0 && uart_rx_lines > 0 && uart_printable_bytes == 0) {
    Serial.println("[摘要][判断] UART 有接收但内容不可见：可能是乱码、波特率不一致、K230 输出不是文本，或信号线接触不良。");
  }
}

void runAllChecks() {
  scanI2c();
  checkBh1750();
  checkInputs();
  startOutputTest();
  if (k230_uart_enabled) {
    sendK230Probe();
  } else {
    Serial.println("[UART][跳过] all 命令未测试 K230 UART；如已接上 K230，请先输入 uart on。");
  }
  printUartStatus();
  sendVoiceWelcomePacket();
}

void handleCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command.length() == 0) {
    return;
  }

  if (command == "help") {
    printHelp();
  } else if (command == "i2c") {
    scanI2c();
  } else if (command == "bh1750") {
    checkBh1750();
  } else if (command == "inputs") {
    checkInputs();
  } else if (command == "outputs") {
    startOutputTest();
  } else if (command == "uart on") {
    enableK230Uart();
  } else if (command == "uart off") {
    disableK230Uart();
  } else if (command == "uart") {
    sendK230Probe();
    printUartStatus();
  } else if (command == "voice") {
    sendVoiceWelcomePacket();
  } else if (command == "all") {
    runAllChecks();
  } else if (command == "watch on") {
    watch_inputs = true;
    Serial.println("[监视] 已开启输入电平持续监视");
  } else if (command == "watch off") {
    watch_inputs = false;
    Serial.println("[监视] 已关闭输入电平持续监视");
  } else {
    Serial.print("[命令][警告] 未知命令：");
    Serial.println(command);
    printHelp();
  }
}

void readSerialCommands() {
  if (!Serial.available()) {
    return;
  }
  String command = Serial.readStringUntil('\n');
  handleCommand(command);
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  pinMode(PERSON_SENSOR_PIN, INPUT);
  pinMode(EC11_S1_PIN, INPUT_PULLUP);
  pinMode(EC11_S2_PIN, INPUT_PULLUP);
  pinMode(EC11_KEY_PIN, INPUT_PULLUP);
  pinMode(FILL_LIGHT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  stopOutputs();

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  pinMode(K230_UART_RX_PIN, INPUT_PULLUP);
  pinMode(K230_UART_TX_PIN, INPUT);
  voiceSerial.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);

  printBanner();
  scanI2c();
  bh1750_ready = initBh1750();
  Serial.println(bh1750_ready ? "[BH1750] 启动初始化通过" : "[BH1750] 启动初始化失败");
  checkInputs();

  unsigned long now = millis();
  last_summary_ms = now;
  last_uart_probe_ms = now;
  last_input_sample_ms = now;
  last_input_report_ms = now;
  last_bh1750_read_ms = now;
}

void loop() {
  unsigned long now = millis();
  readSerialCommands();
  readK230Uart();
  periodicUartProbe(now);
  autoBh1750Read(now);
  watchInputChanges(now);
  updateOutputTest();
  printSummary(now);
  delay(10);
}
