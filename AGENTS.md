# AGENTS.md - Posture Monitor System

智能坐姿监测系统：K230 视觉模块 + ESP32 主控 + 手机 App。

## 当前任务

**新增 FastAPI + MySQL 后端服务**，将 App 数据源从 OneNET 切换到自建后端。

详细实施计划见：`.hermes/plans/2026-05-05_023000-add-fastapi-backend.md`

## 项目结构

```
posture-monitor-system/
├── shared/protocol/       # 三端共享协议常量
├── k230/                  # K230 视觉模块 (MicroPython + YOLOv8n-pose)
├── posture_monitor/       # ESP32-S3 固件 (Arduino C++)
├── app/                   # 手机 App (UniApp + Vue3 + TypeScript)
├── server/                # FastAPI 后端
└── .hermes/plans/         # Hermes 实施计划
```

## 架构

```
K230 -> UART -> ESP32 -> MQTT -> OneNET (设备通信保留)
                                 |
                           FastAPI 轮询 OneNET API -> MySQL
                                 |
                           App 调用 FastAPI 接口
```

## 关键约束

1. **K230 和 ESP32 代码零改动**，只加中间层 + 改 App 数据源。
2. 后端部署在阿里云服务器上。
3. 分阶段开发，每步有里程碑验证。
4. 详见 plan 文件中的 6 个里程碑 (M1-M6)。

## 子项目 AGENTS.md

- App 端规范：`app/AGENTS.md`
- ESP32 端规范：`posture_monitor/AGENTS.md`
- K230 端规范：`k230/AGENTS.md`

## OneNET 凭据

OneNET 凭据只允许通过部署环境或本地未提交的 `.env` 文件配置：

- `VITE_ONENET_PRODUCT_ID` / 后端 `ONENET_PRODUCT_ID`
- `VITE_ONENET_DEVICE_NAME` / 后端 `ONENET_DEVICE_NAME`
- `VITE_ONENET_TOKEN` / 后端 `ONENET_TOKEN`

如需确认取值，优先从 OneNET 控制台、服务器现有环境配置或本地私有 `.env` 获取。不要把 token 写入源码、文档、测试快照或提交历史。
