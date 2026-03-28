# Demo Sketches

本目录提供两个互不干扰的独立 Demo，用于在主固件之外单独验证模块可用性。

当前主固件已经接入 HC-SR501 与 GY-302 BH1750；这些 Demo 继续保留，主要用于单模块排障、复测与接线确认。

## 目录

- `demo_hcsr501/demo_hcsr501.ino`：单独测试 HC-SR501
- `demo_bh1750/demo_bh1750.ino`：单独测试 GY-302 BH1750
- `demo_i2c_scanner/demo_i2c_scanner.ino`：扫描 GPIO1/GPIO2 上所有 I2C 地址
- `demo_k230_uart_sniffer/demo_k230_uart_sniffer.ino`：嗅探 K230 -> ESP32 的原始 UART 字节流
- `demo_k230_uart_sender/demo_k230_uart_sender.ino`：向 K230 持续发送固定 UART heartbeat 文本
- `demo_uart_loopback/demo_uart_loopback.ino`：ESP32 板内自环验证 GPIO17/GPIO18 串口收发
- `demo_gpio18_high/demo_gpio18_high.ino`：将 GPIO18 持续拉高，用于验证该引脚输出/焊点

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

- Demo 会把 `GPIO18` 配置为输出并持续拉高
- 可用以下方式验证：
  - 万用表测 `GPIO18` 对 `GND` 电压
  - LED 串联限流电阻后接到 `GPIO18`
- 串口会每秒打印一次：
  - `[GPIO18] level=HIGH`
