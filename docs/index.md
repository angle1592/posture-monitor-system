# Product Docs Index

本页是 `refactored/` 活跃产品文档的统一入口。

## 使用规则

- 文档按领域存放，不做全量集中。
- 产品总入口放在 `refactored/docs/`。
- 子系统专属文档放在各自子目录的 `docs/` 中。
- 图表原稿与导出图继续放在工作区根目录 `diagrams/`。

## 系统级文档

- 系统架构：`refactored/docs/architecture.md`
- 产品仓库入口：`refactored/README.md`

## ESP32 / posture_monitor

- 子项目入口：`refactored/posture_monitor/README.md`
- 硬件接线：`refactored/posture_monitor/docs/hardware-wiring.md`
- OneNET 物模型：`refactored/posture_monitor/docs/onenet-property-model.md`
- 联调记录：`refactored/posture_monitor/docs/integration-debug-summary-2026-02.md`
- 独立模块 Demo：`refactored/posture_monitor/demos/README.md`

## K230

- 子项目入口：`refactored/k230/README.md`
- 引脚整理：`refactored/k230/docs/k230-pinout.md`
- 联调记录：`refactored/k230/docs/integration-debug-summary-2026-02.md`
- 工作日志：`refactored/k230/docs/worklog-2026-02-22.md`

## App

- 子项目入口：`refactored/app/README.md`
- OneNET 物模型：`refactored/app/docs/onenet-property-model.md`
- OneNET API：`refactored/app/docs/api/oneNet-api.md`

## 图表

- 硬件拓扑图原稿：`diagrams/fig3-1_hardware_topology.drawio`
- 硬件拓扑图导出：`diagrams/png/fig3-1_hardware_topology.png`

## 命名规范

对新建产品文档，优先使用以下约定：

- Markdown：小写英文 + 连字符，例如 `hardware-wiring.md`
- 图文件：与文档主题保持一致，例如 `hardware-topology.drawio`
- 索引页：目录级入口统一使用 `README.md` 或 `index.md`

## 说明

本次只规范活跃产品文档入口和少量命名；论文目录、legacy 目录和论文中已引用的图文件名暂不批量调整，以避免大范围断链。
