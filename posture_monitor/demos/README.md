# Demo Sketches

本目录提供多个互不干扰的独立 Demo，用于在主固件之外单独验证模块可用性。

> 排查记录（2026-04-11）：补光灯“不亮”的一次真实根因已确认是**控制引脚接错**。
> 实物补光控制实际改为 `GPIO13`，而不是先前排查时使用的 `GPIO18`。
> 如果你的板子也采用这版接线，请先同步核对实物连线，并修改 `config.h` 中的 `FILL_LIGHT_PIN` 以及相关 demo 使用的控制引脚，再继续测试。

当前主固件的人体存在检测已经切换到**对射式红外传感器**，GY-302 BH1750 仍为当前主链路传感器；`HC-SR501` 相关 Demo 继续保留，主要用于历史对照、单模块排障、复测与 GPIO 输入口确认。

## 目录

- `demo_hcsr501/demo_hcsr501.ino`：单独测试 HC-SR501
- `demo_bh1750/demo_bh1750.ino`：单独测试 GY-302 BH1750
- `demo_i2c_scanner/demo_i2c_scanner.ino`：扫描 GPIO1/GPIO2 上所有 I2C 地址
- `demo_k230_uart_sniffer/demo_k230_uart_sniffer.ino`：嗅探 K230 -> ESP32 的原始 UART 字节流
- `demo_k230_uart_sender/demo_k230_uart_sender.ino`：向 K230 持续发送固定 UART heartbeat 文本
- `demo_uart_loopback/demo_uart_loopback.ino`：ESP32 板内自环验证 GPIO17/GPIO18 串口收发
- `demo_gpio18_high/demo_gpio18_high.ino`：将 GPIO18 持续拉高，用于验证该引脚输出/焊点
- `demo_gpio18_toggle/demo_gpio18_toggle.ino`：让 GPIO18 在高低电平间交替，用于判断补光驱动极性
- `demo_fill_light_path/demo_fill_light_path.ino`：联调人体存在传感器 + BH1750 + `FILL_LIGHT_PIN` 补光链路

## 测试原则

- 每次只接一个模块
- Demo 接线与未来整机接线保持一致
- 不依赖 K230、MQTT、OLED 页面、云端或整机状态机
- 使用串口监视器查看输出，波特率 `115200`

## 1. HC-SR501 Demo

### 接线

- `VCC -> 5V`
- `GND -> GND`
- `OUT -> GPIO4`

### 期望现象

- 上电后先进入预热提示
- 人移动到传感器前方时，串口输出 `MOTION DETECTED`
- 一段时间无动作后，串口输出 `NO MOTION`

## 2. GY-302 BH1750 Demo

### 接线

- `VCC -> 3V3`
- `GND -> GND`
- `SDA -> GPIO1`
- `SCL -> GPIO2`
- `ADDR -> GND`
- 固定地址：`0x23`

### 期望现象

- 启动时打印固定地址 `0x23`
- 若上电初期传感器未立即响应，Demo 会在启动阶段自动重试数秒
- 正常读到 lux 数值
- 遮挡或照明变化时 lux 明显变化

## 3. 运行方式

建议用 Arduino IDE 分别打开以下 sketch 并上传：

- `demos/demo_hcsr501/demo_hcsr501.ino`
- `demos/demo_bh1750/demo_bh1750.ino`
- `demos/demo_i2c_scanner/demo_i2c_scanner.ino`
- `demos/demo_k230_uart_sniffer/demo_k230_uart_sniffer.ino`
- `demos/demo_k230_uart_sender/demo_k230_uart_sender.ino`
- `demos/demo_uart_loopback/demo_uart_loopback.ino`
- `demos/demo_gpio18_high/demo_gpio18_high.ino`
- `demos/demo_gpio18_toggle/demo_gpio18_toggle.ino`
- `demos/demo_fill_light_path/demo_fill_light_path.ino`

如果后续需要把 Demo 纳入 PlatformIO 多环境构建，再单独补充对应环境配置。

## 4. I2C Scanner Demo

当 BH1750 在固定地址 `0x23` 上没有响应时，先运行 I2C 扫描 Demo：

- 接线保持与 BH1750 Demo 一致：`SDA -> GPIO1`、`SCL -> GPIO2`
- Demo 会扫描 `0x01 ~ 0x7E`
- 若发现设备，会输出 `found device at 0x..`
- 若没有发现任何设备，优先排查供电、共地、SDA/SCL 是否接反，以及模块本身是否正常上电

## 5. K230 UART Sniffer Demo

当主固件持续提示“3秒内未收到任何串口字节”时，先运行此 Demo：

- 接线保持与整机一致：
  - `ESP32 GPIO16 <- K230 UART TX`
  - `ESP32 GPIO15 -> K230 UART RX`
  - `GND <-> GND`
- Demo 会输出：
  - 每个收到的原始字节（HEX + 可打印字符）
  - 每行文本内容
  - 每 3 秒一次心跳统计（`bytes=...` / `lines=...`）
- 判定标准：
  - 若 `bytes` 一直为 `0`，说明 ESP32 物理上没收到任何 K230 串口数据
  - 若能看到连续字节/JSON 行，说明物理 UART 已通，后续再回到主固件解析链路排查

## 6. K230 UART Sender Demo

当需要验证 `ESP32 -> K230` 方向时，运行此 Demo：

- 接线保持与整机一致：
  - `ESP32 GPIO15 -> K230 UART RX`
  - `ESP32 GPIO16 <- K230 UART TX`
  - `GND <-> GND`
- Demo 会每 500ms 发送一行：
  - `ESP32_UART_OK_<n>`
- 建议与 `refactored/k230/src/uart_diag.py` 搭配使用：
  - K230 IDE 应看到 `[diag][RX][LINE] ESP32_UART_OK_<n>`
- 判定标准：
  - 若 K230 IDE 完全收不到回显，说明 `ESP32 -> K230` 物理链路不通

## 7. ESP32 UART Loopback Demo

当怀疑 `GPIO18` 接收路径有问题时，运行此 Demo：

- 先断开 K230
- 用一根杜邦线直接连接：
  - `ESP32 GPIO15 -> ESP32 GPIO16`
- Demo 会每 500ms 发送一行：
  - `LOOPBACK_<n>`
- 若自环正常，串口监视器会同时看到：
  - `[LOOPBACK][TX] LOOPBACK_<n>`
  - `[LOOPBACK][LINE] LOOPBACK_<n>`
- 若只有 TX 没有 RX，说明应优先怀疑 `GPIO18` 接收路径或板级接触问题

## 8. GPIO18 High-Level Demo

当需要验证 `GPIO18` 本身是否能稳定输出高电平时，运行此 Demo：

- **注意**：如果你的实物补光控制线已经改到 `GPIO13`，此 Demo 需要先同步改成 `GPIO13` 再测，否则测试结论无效

- Demo 会把 `GPIO18` 配置为输出并持续拉高
- 可用以下方式验证：
  - 万用表测 `GPIO18` 对 `GND` 电压
  - LED 串联限流电阻后接到 `GPIO18`
- 串口会每秒打印一次：
  - `[GPIO_HIGH] GPIO18 level=HIGH`

## 9. GPIO18 Toggle Demo

当怀疑补光驱动级的有效电平和软件假设不一致时，运行此 Demo：

- **注意**：如果你的实物补光控制线已经改到 `GPIO13`，此 Demo 也需要先把控制引脚改成 `GPIO13`

- Demo 会每 2 秒把 `GPIO18` 在 `LOW` 与 `HIGH` 之间切换一次
- 串口会打印当前写入的电平，并提示你观察“灯是在 HIGH 亮还是 LOW 亮”
- 判定标准：
  - 如果只有 `HIGH` 时亮，硬件更像 **高电平有效**
  - 如果只有 `LOW` 时亮，硬件更像 **低电平有效**
  - 如果高低都不亮，继续排查接线、供电、共地、驱动级

## 10. Fill-Light Path Demo

当主固件里已经满足“有人 + 低照度”但补光灯仍不亮时，优先运行此 Demo：

- 当前仓库配置中，补光控制已经切到 `GPIO13`；如果你的实物不是这版接线，先检查 `config.h` 中的 `FILL_LIGHT_PIN` 与实物是否一致

- 接线保持与整机一致：
  - 人体存在传感器 `OUT -> GPIO4`
  - BH1750 `SDA -> GPIO1`、`SCL -> GPIO2`、`ADDR -> GND`
  - 补光控制 `CTRL -> GPIO13`（或当前 `FILL_LIGHT_PIN`）
- Demo 不依赖 K230、MQTT、OLED 或主状态机，只验证三段链路：
  - 人体存在输入
  - 光照输入
  - `FILL_LIGHT_PIN` 输出
- 串口会持续打印：
  - `rawLevel` / `rawActiveByConfig`
  - `presenceReady` / `presencePresent`
  - `bh1750Ready` / `rawLux` / `lux`
  - `fillLightOn`
  - 最终决策原因与 GPIO18 写入电平

### 判定标准

- 若日志显示 `fill light ON` 且当前补光控制脚输出有效电平，但实物补光灯不亮：
  - 优先排查当前 `FILL_LIGHT_PIN` 接线、驱动级、供电、共地、极性
- 若日志显示 `presence=false`：
  - 优先排查对射式红外接线、发射/接收是否对准、光路是否被遮挡、输出极性是否与配置一致
- 若日志显示 `BH1750 not ready` 或 lux 一直异常偏大：
  - 优先排查 BH1750 供电、I2C 接线、地址与模块状态
- 若日志长期显示 `presence=true` 且原始电平不变化：
  - 优先检查对射式红外 `VCC/GND/DO` 是否接错、是否共地、是否一直遮挡光路，以及 `PERSON_SENSOR_ACTIVE_LEVEL` 是否配置正确
