# 硬件接线与交互说明

本文对应当前固件配置（`config.h`）：
- 主固件已启用：OLED、EC11、HC-SR501、GY-302 BH1750、语音模块、蜂鸣器
- 使用板载 WS2812 作为视觉指示（`WS2812_PIN=48`）

说明：
- `HC-SR501` 和 `GY-302 BH1750` 已按整机接线正式接入主固件
- `demos/` 中的独立 Demo 保留用于单模块排障与复测
- Demo 接线与整机接线保持一致，因此独立排障结果可直接复用于主系统

## 1. 接线总表

| 模块 | ESP32-S3 引脚 | 模块引脚 | 说明 |
|---|---:|---|---|
| K230 UART | GPIO15 | K230 GPIO4 (UART1_RX) | ESP32 TX -> K230 RX |
| K230 UART | GPIO16 | K230 GPIO3 (UART1_TX) | ESP32 RX <- K230 TX |
| OLED (I2C) | GPIO1 | SDA | I2C 数据线 |
| OLED (I2C) | GPIO2 | SCL | I2C 时钟线 |
| OLED (I2C) | 3V3 | VCC | 建议 3.3V 供电 |
| OLED (I2C) | GND | GND | 共地 |
| EC11 | GPIO9 | S1 | 编码器 A 相 |
| EC11 | GPIO10 | S2 | 编码器 B 相 |
| EC11 | GPIO21 | KEY | 按键输入 |
| EC11 | 3V3 | + | 编码器供电 |
| EC11 | GND | GND | 共地 |
| HC-SR501 | GPIO4 | OUT | PIR 数字输出 |
| HC-SR501 | 5V | VCC | 模块供电 |
| HC-SR501 | GND | GND | 共地 |
| GY-302 BH1750 | GPIO1 | SDA | 与 OLED 共用 I2C SDA |
| GY-302 BH1750 | GPIO2 | SCL | 与 OLED 共用 I2C SCL |
| GY-302 BH1750 | 3V3 | VCC | 建议 3.3V 供电 |
| GY-302 BH1750 | GND | GND | 共地 |
| GY-302 BH1750 | GND | ADDR | 固定地址 `0x23` |
| SYN-6288 | GPIO41 | RX | ESP32 TX -> 语音模块 RX |
| SYN-6288 | 3V3 或 5V | VCC | 模块支持 3.3~5V 供电 |
| SYN-6288 | GND | GND | 共地 |
| 蜂鸣器 | GPIO6 | SIG/IN | 控制脚 |
| 蜂鸣器 | 3V3 或 5V | VCC | 模块支持 3.3~5V 供电 |
| 蜂鸣器 | GND | GND | 共地 |
| 板载 WS2812 | GPIO48 | 板载 | 固件直接驱动，无需外接 |

蜂鸣器类型：有源蜂鸣器，GPIO 高电平触发。

## 2. 关键电气注意事项

- 必须共地：所有外设 GND 与 ESP32 GND 必须连接。
- 优先使用硬串口发送语音：当前实现用 `Serial2` TX-only。
- SYN-6288 供电可用 3.3V 或 5V；若模块 RX 非 3.3V 兼容，需做电平匹配。
- OLED 建议先以 3.3V 供电测试，避免 I2C 电平不兼容。
- BH1750 通过 I2C 工作，未来整机中与 OLED 共用 `GPIO1/GPIO2` 总线；独立 Demo 阶段可只接 BH1750。
- 你当前将 BH1750 的 `ADDR` 接到了 `GND`，因此本项目统一按固定地址 `0x23` 使用。
- HC-SR501 输出通常可直接接 ESP32 数字输入；模块上电后需要预热，刚启动几十秒内读数可能不稳定。
- 主固件中：PIR 预热完成前按“默认有人”处理，避免刚上电阶段误判无人。
- 主固件中：BH1750 lux 会参与 OLED 显示与 MQTT `ambientLux` 上报。
- 主固件中：PIR 会参与 `personPresent` 判定，并在无人时抑制坐姿提醒。
- 有源蜂鸣器为高电平触发，默认空闲应保持低电平。

## 3. 接线示意（逻辑）

```text
ESP32-S3
  GPIO15 ----------------------> K230 GPIO4 (UART1_RX)
  GPIO16 <---------------------- K230 GPIO3 (UART1_TX)

  GPIO1  ----------------------> OLED SDA
  GPIO2  ----------------------> OLED SCL
  3V3    ----------------------> OLED VCC
  GND    ----------------------> OLED GND

  GPIO9  ----------------------> EC11 S1
  GPIO10 ----------------------> EC11 S2
  GPIO21 ----------------------> EC11 KEY
  3V3    ----------------------> EC11 +
  GND    ----------------------> EC11 GND

  GPIO4  <---------------------- HC-SR501 OUT
  5V     ----------------------> HC-SR501 VCC
  GND    ----------------------> HC-SR501 GND

  GPIO1  <---------------------> BH1750 SDA
  GPIO2  <---------------------> BH1750 SCL
  3V3    ----------------------> BH1750 VCC
  GND    ----------------------> BH1750 GND

  GPIO41 ----------------------> SYN-6288 RX
  VCC    ----------------------> SYN-6288 VCC
  GND    ----------------------> SYN-6288 GND

  GPIO6  ----------------------> Buzzer SIG
  VCC    ----------------------> Buzzer VCC
  GND    ----------------------> Buzzer GND

  GPIO48 -> On-board WS2812 (no external wiring)
```

## 4. 单模块 Demo 接线建议

为避免互相干扰，建议先分别测试：

### 4.1 HC-SR501 Demo

- 只接 `HC-SR501 VCC/GND/OUT`
- `OUT -> GPIO4`
- 暂时不接 BH1750
- 运行：`demos/demo_hcsr501/demo_hcsr501.ino`

### 4.2 GY-302 BH1750 Demo

- 只接 `BH1750 VCC/GND/SDA/SCL`
- `SDA -> GPIO1`，`SCL -> GPIO2`
- `ADDR -> GND`，固定使用地址 `0x23`
- 暂时不接 HC-SR501
- 运行：`demos/demo_bh1750/demo_bh1750.ino`

### 4.3 与未来整机接线的关系

- HC-SR501 Demo 使用的 `GPIO4` 就是未来整机接法
- BH1750 Demo 使用的 `GPIO1/GPIO2` 就是未来整机接法
- 因此 Demo 通过后，不需要因为整机接入再改动引脚规划

## 5. 定时器模式如何操作（解决“模式切换冲突”）

你提到的冲突是对的：EC11 默认左右旋转用于切模式。当前方案通过“编辑锁”解决：

- 全局默认：
  - 旋转：切模式（Posture/Clock/Timer）
  - 按键短按：模式内动作
  - 按键长按：模式内高级动作
- 在 `MODE_TIMER` 下：
  1. **长按**进入“定时器调参模式”（编辑锁打开）
  2. 调参模式中，**旋转不再切模式**，改为增减时长（每步 1 分钟）
  3. **短按**开始/暂停/继续定时器
  4. **再次长按**退出调参模式（编辑锁关闭，旋转恢复切模式）

### 定时器交互状态机

```text
[MODE_TIMER + IDLE]
  long press -> [ADJUST ON]
  short press -> RUNNING

[ADJUST ON]
  rotate -> duration +/- 1min
  short press -> start/pause/resume
  long press -> [ADJUST OFF]

[RUNNING]
  short press -> PAUSED
  timeout -> DONE (beep/voice)

[PAUSED]
  short press -> RUNNING
```

## 6. 与云端联动的定时器字段

- `timerDurationSec`：定时器时长（秒）
- `timerRunning`：是否运行

本地 EC11 和云端都可修改；建议云端下发后立即回读 `property/get` 确认生效值。

## 7. OLED 无显示排查

- 在 Arduino IDE 的 Library Manager 安装 `U8g2` 库（`ENABLE_OLED=1` 时必需）
- 若未安装，编译会直接报缺少 `U8g2lib.h`
- 确认接线：`SDA=GPIO1`、`SCL=GPIO2`、`VCC=3V3`、`GND=GND`
- 确认模块地址常见为 `0x3C`（少数是 `0x3D`，如不显示可尝试改 `OLED_I2C_ADDR`）
- 确认共地，且 OLED 供电稳定（建议先 3.3V）

## 8. EC11 排障经验（本项目实测）

问题现象：

- `test ec11 watch`、`test ec11 state`、`test ec11 scan`、`test ec11 scanext` 无任何变化日志
- `test pin scanall` 也仅有 start/end，没有中间引脚变化

最终根因：

- ESP32 对应引脚（本例为 `GPIO21`）焊接/接触不良，导致 `KEY` 信号未真正进入芯片

如何快速定位（无万用表）：

1. 运行 `test pin scanall`
2. 用杜邦线一端接 `GND`，另一端直接触碰目标 GPIO 焊盘（如 `GPIO21`）
3. 若日志出现 `GPIO21=0` 再回 `GPIO21=1`，说明固件与检测链路正常
4. 若只有触碰焊盘才有变化，而模块按键无变化，优先判断为焊点/接触问题

修复建议：

- 优先补焊或重做该 GPIO 焊点，保证稳定接触
- 若短期不便补焊，可临时改用接触可靠的其他 GPIO 作为 `EC11_KEY_PIN`
