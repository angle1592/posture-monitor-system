# Posture Monitor System

智能坐姿监测系统 — 基于 K230 视觉模块 + ESP32 + 移动端 App 的实时坐姿检测与提醒方案。

## 系统架构

```
┌──────────────┐    UART/JSON     ┌──────────────┐   MQTT/OneNET   ┌──────────────┐
│  K230 Vision │ ──────────────→  │    ESP32-S3   │ ←────────────→  │  OneNET Cloud │
│  YOLOv8n-pose│   115200 baud    │  Gateway+UI   │                │               │
│  (MicroPython)│                 │  (Arduino C++) │                └───────┬───────┘
└──────────────┘                  └──────────────┘                        │
                                                                    HTTP / MQTT
                                                                         │
                                                                  ┌──────▼───────┐
                                                                  │  Mobile App   │
                                                                  │ UniApp/Vue3/TS│
                                                                  └──────────────┘
```

## 项目结构

```text
refactored/
├── docs/                    # 产品总文档入口与系统级文档
├── shared/                  # 共享协议层 (三端统一常量)
│   └── protocol/
├── posture_monitor/         # ESP32-S3 固件 (Arduino)
├── k230/                    # K230 视觉模块 (MicroPython)
├── app/                     # 移动端 App (UniApp + Vue3 + TypeScript)
└── scripts/                 # 工具脚本
```

## 子项目说明

### K230 Vision (`k230/`)
- **功能**: YOLOv8n-pose 人体姿态检测，实时分析坐姿
- **输出**: 通过 UART 向 ESP32 发送 JSON 帧 (posture_type, confidence, is_abnormal)
- **技术栈**: MicroPython (CanMV), nncase_runtime, K230 NPU

### ESP32-S3 Gateway (`esp32/`)
- **功能**: 系统网关 — 接收 K230 数据、驱动本地告警、同步云端状态
- **外设**: SSD1306 OLED, WS2812 LED, SYN-6288 语音模块, EC11 编码器, 蜂鸣器
- **通信**: UART (K230), WiFi/MQTT (OneNET), I2C (OLED)
- **技术栈**: Arduino Framework, PubSubClient, U8g2

### Mobile App (`app/`)
- **功能**: 用户界面 — 坐姿概览、实时监控、历史趋势、设备控制
- **通信**: 通过 OneNET REST API 与 ESP32 间接通信
- **技术栈**: UniApp, Vue 3, TypeScript, Vite

## 共享协议

三端通过 `shared/protocol/` 共享统一的协议常量：
- **姿势类型**: `normal`, `head_down`, `hunchback`, `tilt`, `no_person`
- **系统模式**: `0=posture`, `1=clock`, `2=timer`
- **告警掩码**: `bit0=LED`, `bit1=Buzzer`, `bit2=Voice`

修改协议时，更新 `shared/protocol/schemas.json` 后同步更新三种语言的常量文件。

## 文档入口

- 产品文档统一索引：`refactored/docs/index.md`
- 系统架构文档：`refactored/docs/architecture.md`
- ESP32 接线文档：`refactored/posture_monitor/docs/hardware-wiring.md`
- K230 引脚文档：`refactored/k230/docs/k230-pinout.md`

产品文档按领域存放：系统级文档放在 `refactored/docs/`，子系统专属文档放在各自子目录的 `docs/`，图表原稿与导出图放在工作区根目录 `diagrams/`。

## 开发指南

### K230 部署
```bash
# 通过 CanMV IDE 上传 k230/src/*.py 和 k230/models/*.kmodel 到 K230 开发板
```

### ESP32 编译
```bash
# 使用 Arduino IDE 或 PlatformIO 编译 esp32/ 目录下的固件
```

### App 开发
```bash
cd app
npm install
npm run dev:h5          # H5 开发
npm run dev:mp-weixin   # 微信小程序开发
```
