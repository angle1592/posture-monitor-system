# 坐姿监测系统 — 架构设计文档
## 1. 系统总览
### 1.1 系统简介
坐姿监测系统是一个基于物联网的智能坐姿检测与提醒方案，旨在帮助用户（尤其是长时间伏案工作的学生和办公人群）保持正确坐姿，预防因不良坐姿导致的颈椎病、腰椎病等健康问题。
系统通过 K230 视觉模块实时采集用户坐姿图像，利用 YOLOv8n-pose 深度学习模型进行人体姿态检测，分析坐姿是否正常（如低头、驼背、侧倾等异常姿势）。检测结果通过 UART 串口发送给 ESP32 主控模块，ESP32 将状态上报到 OneNET 云平台，同时驱动本地报警设备（LED、蜂鸣器、语音模块）进行提醒。用户可通过手机 App 远程查看坐姿状态、历史数据，并对设备进行远程控制。
**核心价值**：
- 实时监测：约 200ms 一帧的检测频率，及时发现问题坐姿
- 智能提醒：多种提醒方式（灯光、声音、语音），避免重复打扰
- 云端同步：数据上云，支持远程查看和多终端访问
- 本地交互：OLED 显示屏 + 旋转编码器，无需手机即可操作
### 1.2 系统架构图
```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              坐姿监测系统架构                                      │
└─────────────────────────────────────────────────────────────────────────────────┘
                              ┌──────────────────┐
                              │   K230 视觉模块   │
                              │  (MicroPython)   │
                              │                  │
                              │  · 摄像头采集     │
                              │  · YOLOv8n-pose  │
                              │  · 坐姿分析      │
                              └────────┬─────────┘
                                       │
                                       │ UART/JSON
                                       │ 115200 baud
                                       │ TX:GPIO3 → RX:GPIO18
                                       │ RX:GPIO4 ← TX:GPIO17
                                       ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                              ESP32-S3 主控模块 (Arduino C++)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │  K230解析   │  │  定时器FSM  │  │  报警控制   │  │  OLED显示   │              │
│  │ k230_parser │  │  timer_fsm  │  │   alerts    │  │   display   │              │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │  MQTT通信   │  │  模式管理   │  │  语音合成   │  │  传感器     │              │
│  │mqtt_handler │  │mode_manager │  │    voice    │  │   sensors   │              │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘              │
│                                                                                   │
│  外设: WS2812 LED · 蜂鸣器 · SYN-6288语音 · OLED · EC11编码器                     │
└───────────────────────────────────────┬───────────────────────────────────────────┘
                                        │
                                        │ WiFi + MQTT
                                        │ mqtts.heclouds.com:1883
                                        ▼
                              ┌──────────────────┐
                              │   OneNET 云平台   │
                              │                  │
                              │  · MQTT Broker   │
                              │  · 物模型管理    │
                              │  · 数据存储      │
                              └────────┬─────────┘
                                       │
                                       │ HTTP REST API
                                       │ iot-api.heclouds.com
                                       ▼
                              ┌──────────────────┐
                              │    手机 App      │
                              │  (UniApp/Vue3)   │
                              │                  │
                              │  · 实时状态      │
                              │  · 历史数据      │
                              │  · 远程控制      │
                              └──────────────────┘
```
### 1.3 三个子项目职责
| 子项目 | 目录 | 核心职责 | 技术栈 |
|--------|------|----------|--------|
| K230 视觉模块 | `k230/` | 摄像头采集、姿态检测、坐姿分析、UART 发送检测结果 | MicroPython, YOLOv8n-pose, nncase_runtime |
| ESP32 主控模块 | `esp32/` | 接收 K230 数据、MQTT 云端通信、本地报警、用户交互 | Arduino C++, PubSubClient, U8g2 |
| 手机 App | `app/` | 远程监控、历史数据查看、设备参数配置 | UniApp, Vue3, TypeScript |

当前 K230 视觉端默认采用 `legacy` 姿态判定路径，即基于绝对几何特征、风险评分、EMA 平滑和迟滞阈值的稳定化方案。代码中保留了基于个人基线增量的 `decoupled` 实验分支，但现阶段项目默认配置未启用该机制，因此当前交付口径不将“基线自适应校准”作为在线运行特性。

### 1.3.1 感知层组成
本项目中的感知层并不等同于单一的 K230 视觉端，而是由视觉感知、人体存在感知和环境感知共同组成，为后续控制、提醒与云端上报提供原始状态输入。

| 感知模块 | 主要实现位置 | 作用 |
|----------|--------------|------|
| 视觉感知模块 | `refactored/k230/` | 通过摄像头采集用户图像，结合 YOLOv8n-pose 完成人体关键点检测、姿态识别与坐姿分类，输出 `normal`、`head_down`、`hunchback`、`tilt`、`no_person` 等结果。 |
| 人体存在感知模块 | `refactored/posture_monitor/sensors.h` / `sensors.cpp` | 基于 HC-SR501 PIR 红外传感器检测用户是否在位，用于辅助区分“无人”与“异常坐姿”场景，并在无人时抑制不必要的提醒。 |
| 环境光感知模块 | `refactored/posture_monitor/sensors.h` / `sensors.cpp` | 基于 GY-302 BH1750 光照传感器采集环境照度（lux），为设备状态显示、属性上报和后续环境联动功能提供输入。 |

其中，K230 视觉模块是感知层中的核心识别单元，但不应将整个感知层简单等同于 `k230/`；ESP32 侧的 PIR 与 BH1750 也属于感知层的重要组成部分。

### 1.4 系统运行模式
系统支持三种运行模式，通过 EC11 旋转编码器切换：
| 模式 | 编号 | 功能描述 |
|------|------|----------|
| 坐姿检测模式 | `MODE_POSTURE (0)` | 默认模式，实时监测坐姿并报警提醒 |
| 时钟模式 | `MODE_CLOCK (1)` | 显示当前时间，不进行坐姿检测 |
| 定时器模式 | `MODE_TIMER (2)` | 番茄钟功能，可设置倒计时提醒休息 |
## 2. 硬件与通信架构
### 2.1 硬件节点与职责映射
| 节点 | 主要硬件 | 主要职责 | 与其他节点关系 |
|------|----------|----------|----------------|
| K230 视觉模块 | 摄像头 + K230 NPU | 采集画面、关键点推理、姿态判定、输出 UART JSON | 作为上游数据源，向 ESP32 单向输出 |
| ESP32-S3 主控 | MCU + WiFi + 外设总线 | 串口接收、状态机管理、报警控制、MQTT 上云、OLED/EC11 交互 | 连接本地硬件与云端，是系统中枢 |
| OneNET 云平台 | MQTT Broker + 物模型服务 + 历史存储 | 接收属性上报、下发属性命令、提供历史查询接口 | 为 App 提供统一后端入口 |
| 手机 App | UniApp 前端 | 展示实时状态、历史统计、控制设备参数 | 不直连设备，只通过 OneNET API 间接控制 |
### 2.2 物理连接与引脚对照
| 通道 | K230 侧 | ESP32 侧 | 说明 |
|------|---------|----------|------|
| UART TX | `uart_tx_pin=3` | `K230_UART_RX_PIN=18` | K230 发数据到 ESP32 |
| UART RX | `uart_rx_pin=4` | `K230_UART_TX_PIN=17` | ESP32 可回发控制/调试 |
| UART 波特率 | `uart_baudrate=115200` | `K230_UART_BAUD=115200` | 两端必须严格一致 |
| 外设 | ESP32 引脚 | 作用 |
|------|-----------|------|
| WS2812 | `GPIO48` | 状态灯（联网/异常/提醒） |
| 蜂鸣器 | `GPIO6` | 脉冲提示与告警 |
| 语音模块 TX | `GPIO41` | 向 SYN-6288 发送播报数据 |
| OLED SDA/SCL | `GPIO1/GPIO2` | I2C 显示 |
| EC11 S1/S2/KEY | `GPIO9/GPIO10/GPIO11` | 模式切换与按键输入 |
### 2.3 通信链路分层
```
K230 摄像头帧
   |
   | 视觉推理 + 姿态判定
   v
K230 JSON 帧（UART, 115200）
   |
   | frame_id / posture_type / is_abnormal / confidence / ...
   v
ESP32 串口解析层（k230_parser）
   |
   | 状态机 + 策略判定 + 报警执行
   v
ESP32 MQTT 层（mqtt_handler）
   |
   | topic: $sys/DX0LtPPJQ9/main/thing/property/post
   v
OneNET 物模型
   |
   | HTTP 查询 / 控制
   v
手机 App（store + oneNetApi）
```
### 2.4 UART 负载格式（K230 -> ESP32）
K230 在主循环里按检测间隔（`detection_interval=120ms`）发送 JSON 行文本，核心字段如下：
| 字段 | 类型 | 语义 |
|------|------|------|
| `frame_id` | integer | 帧递增序号 |
| `posture_type` | string | 粗粒度分类（`normal`/`bad`/`no_person`） |
| `posture_type_fine` | string | 细粒度分类（`head_down`/`hunchback`/`tilt`/`unknown`） |
| `is_abnormal` | bool | 当前帧是否异常 |
| `consecutive_abnormal` | integer | 连续异常帧累计 |
| `confidence` | number | 识别置信度 |
| `timestamp` | integer | 发送时刻（ms） |
### 2.5 MQTT 主题与方向
| 方向 | 主题 | 用途 |
|------|------|------|
| ESP32 发布 | `$sys/DX0LtPPJQ9/main/thing/property/post` | 属性上报 |
| ESP32 发布 | `$sys/DX0LtPPJQ9/main/thing/property/set_reply` | 对 `property/set` 的回复 |
| ESP32 发布 | `$sys/DX0LtPPJQ9/main/thing/property/get_reply` | 对 `property/get` 的回复 |
| ESP32 发布 | `$sys/DX0LtPPJQ9/main/thing/event/post` | 事件上报 |
| ESP32 订阅 | `$sys/DX0LtPPJQ9/main/thing/property/post/reply` | 属性上报回执 |
| ESP32 订阅 | `$sys/DX0LtPPJQ9/main/thing/property/set` | 云端下发属性 |
| ESP32 订阅 | `$sys/DX0LtPPJQ9/main/thing/property/get` | 云端查询属性 |
| ESP32 订阅 | `$sys/DX0LtPPJQ9/main/thing/event/post/reply` | 事件回执 |
### 2.6 网络时序与容错参数
| 参数 | 值 | 作用 |
|------|----|------|
| `PUBLISH_INTERVAL_MS` | `10000` | MQTT 周期上报间隔 |
| `K230_TIMEOUT_MS` | `5000` | 5 秒无新帧视为视觉链路超时 |
| `K230_STARTUP_GRACE_MS` | `15000` | 启动阶段首帧等待窗口 |
| `K230_STALE_HOLD_MS` | `15000` | 超时后短期沿用上一帧判定 |
| `ALERT_CHECK_INTERVAL_MS` | `200` | 报警策略刷新间隔 |
| `DISPLAY_UPDATE_INTERVAL_MS` | `500` | OLED 刷新间隔 |
### 2.7 云端 API 与 App 访问路径
App 不直接操作 MQTT，而是走 OneNET HTTP：
- 属性查询基地址：`https://iot-api.heclouds.com/thingmodel`
- 设备状态基地址：`https://iot-api.heclouds.com/device`
- 查询当前属性：`/query-device-property`
- 下发属性设置：`/set-device-property`
- 查询属性历史：`/query-device-property-history`
- 查询设备在线：`/detail`
---
## 3. ESP32 固件架构
### 3.1 模块拆分与主入口
ESP32 主程序位于 `esp32/posture_monitor.ino`，其职责不是“做所有事情”，而是“调度各模块”：
| 模块 | 文件 | 职责 |
|------|------|------|
| 配置层 | `config.h` | 引脚、主题、阈值、开关、网络参数 |
| UART 解析层 | `k230_parser.*` | 接收并解析 K230 JSON 帧，维护最新姿态状态 |
| MQTT 层 | `mqtt_handler.*` | WiFi/MQTT 建连、订阅、上报、回包 |
| 状态机层 | `timer_fsm.*` | 定时器状态 + 告警冷却策略 |
| 模式输入层 | `mode_manager.*` | EC11 旋钮短按/长按/旋转事件 |
| 提醒执行层 | `alerts.*` + `voice.*` | LED/蜂鸣器/语音输出 |
| 显示层 | `display.*` | OLED 画面与状态显示 |
### 3.2 setup() 启动顺序
```
[串口日志 initSerial]
        -> [K230 UART 初始化]
        -> [SNTP 配置]
        -> [外设 init: sensors/mode/display/voice/alerts]
        -> [timer_applyConfig + timer_init]
        -> [WiFi 连接]
        -> [MQTT init + connect + subscribe]
        -> [OLED 启动画面]
        -> [系统就绪]
```
这样排序的原因：先保证底层通信和执行通道可用，再开放云端控制与 UI，避免“云端下发已到但本地执行链路没准备好”。
### 3.3 loop() 非阻塞调度
主循环基于 `millis()`，每轮做六类任务：
1. `mqtt_loop()`：维持 MQTT 心跳与重连。
2. `k230_read()`：拉取串口帧，更新姿态缓存。
3. `mode_update()` + `handleModeAndTimerInput()`：处理 EC11 输入与模式切换。
4. `timer_tick(now)`：推进定时器状态机。
5. 按间隔调度 `sensors_update` / `handlePeriodicPublish` / `timer_alertPolicyTick` / `display_update`。
6. `delay(LOOP_YIELD_DELAY_MS)` 让出 CPU（当前为 `10ms`）。
### 3.4 周期上报决策树（核心）
ESP32 并不是每次都“直接上报实时帧结果”，而是走 `evaluatePeriodicPublishDecision()`：
```
            +-------------------------------+
            | monitoringEnabled == false ? |
            +-------------------------------+
                    |yes
                    v
         [DECISION_MONITOR_OFF] -> isPosture=true
                    |
                   no
                    v
            +----------------------+
            | frame_count == 0 ?   |
            +----------------------+
                    |yes
                    v
         [DECISION_STARTUP_WAIT] -> isPosture=true
                    |
                   no
                    v
       +----------------------------------------+
       | data invalid OR timeout(>5000ms) ?     |
       +----------------------------------------+
              |yes                    |no
              v                       v
   +-------------------------+     [DECISION_LIVE_FRAME]
   | data_age <= 15000ms ?   |          -> isPosture = !isAbnormal
   +-------------------------+
      |yes           |no
      v              v
[DECISION_STALE_HOLD]   [DECISION_FALLBACK_TRUE]
  -> 沿用最后状态          -> isPosture=true
```
该决策树体现三个工程目标：
- 启动期稳态：视觉首帧未到时不误报异常。
- 抖动抑制：短时串口抖动时沿用最近状态。
- 安全默认：长时间失联时回退到 `true`，避免连环误报警。
### 3.5 MQTT 下行命令处理
#### `property/set` 处理流程
```
[收到 property/set]
   -> 提取 msgId
   -> 逐项解析 params
      - monitoringEnabled / isPosture(legacy)
      - currentMode
      - alertModeMask
      - cooldownMs / remindIntervalMs(legacy)
      - timerDurationSec
      - timerRunning
      - selfTest
   -> timer_applyConfig()
   -> 发布 set_reply(msgId)
```
关键点：
- 兼容新旧字段：优先 `monitoringEnabled`，旧字段 `isPosture` 仅作兜底。
- 配置落地后统一 `timer_applyConfig()`，避免模块配置不一致。
- 每次处理结束都按事务 `msgId` 回 `set_reply`。
#### `property/get` 处理流程
```
[收到 property/get]
   -> 提取 msgId
   -> 读取当前状态快照
   -> 组装 params JSON
   -> 发布 get_reply(msgId, params)
```
返回字段包含：
- `isPosture`
- `monitoringEnabled`
- `currentMode`
- `alertModeMask`
- `cooldownMs`
- `timerDurationSec`
- `timerRunning`
- `cfgVersion`
### 3.6 双状态机设计
ESP32 使用两个独立状态机：
1. 计时器流程状态机（TimerState）
2. 告警节流状态机（AlertPolicyState）
分离原因：计时器是“业务流程”，告警冷却是“策略流程”，混在一起会让条件爆炸。
#### TimerState 转移图
```
[TIMER_IDLE] --短按start--> [TIMER_RUNNING]
[TIMER_RUNNING] --短按pause--> [TIMER_PAUSED]
[TIMER_PAUSED] --短按resume--> [TIMER_RUNNING]
[TIMER_RUNNING] --倒计时归零--> [TIMER_DONE]
[TIMER_DONE] --短按start--> [TIMER_RUNNING]
[任意状态] --reset--> [TIMER_IDLE]
```
#### AlertPolicyState 转移图
```
[ALERT_POLICY_IDLE]
   --(monitoringEnabled && k230Ok && !noPerson && dataValid && isAbnormal)--> [ALERT_POLICY_COOLDOWN]
   （进入时触发一次提醒，设置 cooldownUntil）
[ALERT_POLICY_COOLDOWN]
   --(now < cooldownUntil && 仍异常)--> [ALERT_POLICY_COOLDOWN]
   --(条件不成立：恢复正常/无人/超时/监控关闭)--> [ALERT_POLICY_IDLE]
   --(cooldown结束且仍异常)--> [ALERT_POLICY_COOLDOWN]（再触发一次提醒并重置冷却）
```
### 3.7 运行时配置对象 RuntimeConfig
`timer_fsm` 维护统一配置体：
| 字段 | 初始值来源 | 说明 |
|------|------------|------|
| `alertModeMask` | `ALERT_MODE_LED|BUZZER|VOICE` | 控制提醒输出通道 |
| `cooldownMs` | `ALERT_COOLDOWN_MS=30000` | 告警冷却窗口 |
| `timerDurationSec` | `TIMER_DEFAULT_DURATION_SEC=1500` | 番茄钟时长 |
| `cfgVersion` | 初始 `1` | 每次配置更新自增 |
### 3.8 告警输出位掩码
| bit 位 | 掩码值 | 输出通道 |
|--------|--------|----------|
| bit0 | `0x01` | LED |
| bit1 | `0x02` | 蜂鸣器 |
| bit2 | `0x04` | 语音 |
| ALL | `0x07` | 全部启用 |
### 3.9 关键配置速查（来自 config.h）
| 分类 | 配置项 | 值 |
|------|--------|----|
| WiFi | `WIFI_SSID` | `Mi` |
| MQTT | `MQTT_HOST` | `mqtts.heclouds.com` |
| MQTT | `MQTT_PORT` | `1883` |
| OneNET | `ONENET_PRODUCT_ID` | `DX0LtPPJQ9` |
| OneNET | `ONENET_DEVICE_NAME` | `main` |
| 周期 | `PUBLISH_INTERVAL_MS` | `10000` |
| 超时 | `K230_TIMEOUT_MS` | `5000` |
| 冷却 | `ALERT_COOLDOWN_MS` | `30000` |
| 定时器 | `TIMER_DEFAULT_DURATION_SEC` | `1500` |
| 定时器上限 | `TIMER_MAX_DURATION_SEC` | `7200` |
| 异常阈值 | `ABNORMAL_THRESHOLD_FRAMES` | `15` |
---
## 4. K230 视觉模块架构
### 4.1 角色定位
K230 是系统的“感知端”，输出的是“结构化姿态信息”，而不是原始图像。它关注两件事：
1. 人在不在（`no_person` 过滤）
2. 姿势好不好（`normal` / `bad` + 细粒度标签）
### 4.2 主循环数据流
```
[摄像头采样]
    -> [YOLOv8n-pose 推理]
    -> [人体存在过滤 PERSON_FILTER]
    -> [姿态判定 POSTURE_THRESHOLDS]
    -> [连续异常计数 consecutive_abnormal]
    -> [UART JSON 输出到 ESP32]
```
### 4.3 核心配置（config.py）
| 配置 | 值 | 含义 |
|------|----|------|
| `model_path` | `/sdcard/examples/kmodel/yolov8n-pose.kmodel` | 模型路径 |
| `input_width/input_height` | `320/320` | 模型输入尺寸 |
| `camera_framesize` | `QVGA` | 采集分辨率 |
| `camera_pixformat` | `RGBP888` | AI 输入像素格式 |
| `detection_interval` | `120` ms | 检测循环间隔 |
| `abnormal_threshold` | `2` | 连续异常阈值（K230 侧） |
| `uart_id` | `1` | 串口编号 |
| `uart_baudrate` | `115200` | 串口波特率 |
| `uart_tx_pin/uart_rx_pin` | `3/4` | 串口引脚 |
| `no_person_reset_frames` | `5` | 连续无人后重置异常状态 |
### 4.4 姿态标签映射策略
在 `main.py` 中，细粒度姿态会映射到粗粒度输出：
- `normal` -> `posture_type=normal`, `is_abnormal=false`
- `head_down/hunchback/tilt/unknown` -> `posture_type=bad`, `is_abnormal=true`
- 未检测到人 -> `posture_type=no_person`, `is_abnormal=false`
这样做的原因：
- ESP32 和云端策略只需要稳定的二分类（正常/异常）即可驱动提醒。
- App 若要解释异常类型，可读取 `posture_type_fine`。
### 4.5 性能与稳态策略
K230 代码内置了多层稳态设计：
- 滑动窗口统计识别时延（平均值 + P95）。
- `no_person_reset_frames` 避免用户离开后回到画面立即触发误报。
- 可配置 `gc_collect_interval_frames` 限制内存碎片累积。
- 检测器未初始化时提供 mock 输出，保证联调链路不中断。
### 4.6 K230 输出契约（与 shared/protocol 一致）
`schemas.json` 中 `K230UartFrame` 要求字段：
- `frame_id`
- `posture_type`
- `is_abnormal`
- `confidence`
可选字段：
- `posture_type_fine`
- `consecutive_abnormal`
- `timestamp`
- `person_debug`
- `posture_debug`
---
## 5. 手机 App 架构
### 5.1 App 的系统定位
App 是“可视化与控制端”，不是实时控制器。它通过 OneNET HTTP 接口拉取状态、下发属性，依赖云端转发到设备。
### 5.2 三层结构
| 层次 | 文件 | 职责 |
|------|------|------|
| 协议常量层 | `app/src/utils/constants.ts` | 维护属性 ID、模式值、轮询配置 |
| 网络访问层 | `app/src/utils/oneNetApi.ts` | 封装 OneNET 请求、错误处理、token 管理 |
| 状态管理层 | `app/src/utils/store.ts` | 聚合实时状态、统计逻辑、轮询策略、页面可见性策略 |
### 5.3 Store 状态模型
`store.ts` 维护四类状态：
1. 连接态：`isOnline`, `lastCheckTime`
2. 姿态态：`isPosture`, `lastPostureTime`
3. 今日统计：`todayAbnormalCount`, `todayGoodMinutes`, `todayTotalMinutes`
4. 控制态：`monitoringEnabled`, `currentMode`
### 5.4 轮询策略（三档）
| 档位 | 常量 | 间隔 |
|------|------|------|
| 后台档 | `POLLING_INTERVALS.BACKGROUND` | `5000ms` |
| 常规档 | `POLLING_INTERVALS.NORMAL` | `2000ms` |
| 实时档 | `POLLING_INTERVALS.REALTIME` | `800ms` |
策略规则：
- 页面不可见时强制降到后台档。
- 监控/控制页面可切到实时档。
- 间隔不变时不重建 timer，避免轮询抖动。
### 5.5 在线判定逻辑（双通道兜底）
App 不只看一个接口：
1. `queryDeviceStatus()`：读取 `/device/detail` 中 `status` 与 `last_time`
2. `inferOnlineFromProperties()`：检查属性流最近时间戳
在线阈值：`ONLINE_TIMEOUT_MS=300000`（5 分钟）
优势：当详情接口延迟更新时，属性流仍可判断设备“真实在线”。
### 5.6 控制路径
```
[用户在 App 切换开关/模式]
   -> setDeviceProperty(params)
   -> OneNET thingmodel/set-device-property
   -> 云端转 MQTT property/set
   -> ESP32 handlePropertySet
   -> ESP32 set_reply
   -> App 下一轮轮询读取到新状态
```
### 5.7 本地持久化策略
| 键名 | 内容 | 目的 |
|------|------|------|
| `postureStats` | 当天异常次数 + 使用分钟数 | 跨页面快速回显 |
| `controlSettings` | 最近模式 + 监控开关 | 启动后保持用户习惯 |
| `oneNetToken` | 运行时 token | 支持设置页临时更换凭据 |
---
## 6. 共享协议层和开发指南
### 6.1 共享协议层的意义
`shared/protocol` 是三端协同的“契约中心”：
- `schemas.json`：定义 JSON 结构与字段语义。
- `constants.h`：ESP32 侧常量。
- `constants.py`：K230 侧常量。
- `constants.ts`：App 侧常量。
协议层目标是避免“同一字段三端不同名”导致的隐性故障。
### 6.2 统一枚举与字段
#### 姿态枚举
`normal`, `head_down`, `hunchback`, `tilt`, `no_person`, `unknown`, `bad`
#### 模式枚举
- `0 = posture`
- `1 = clock`
- `2 = timer`
#### 属性标识（OneNET identifier）
- `isPosture`
- `monitoringEnabled`
- `currentMode`
- `alertModeMask`
- `cooldownMs`
- `timerDurationSec`
- `timerRunning`
- `cfgVersion`
- `selfTest`
### 6.3 配置一致性检查清单
每次改协议或改参数时，至少检查以下同步点：
1. `shared/protocol/schemas.json` 字段名是否变化。
2. `esp32/config.h` 的 `PROP_ID_*` 是否同步。
3. `app/src/utils/constants.ts` 的 `PROP_IDS` 是否同步。
4. K230 输出字段是否仍满足 `K230UartFrame.required`。
5. OneNET 物模型 identifier 是否与代码一致。
### 6.4 端到端调试顺序（推荐）
```
步骤1：单测 K230
  看 UART 是否持续输出 JSON 且 frame_id 递增
步骤2：联调 ESP32 串口
  看 ESP32 日志是否持续收到 posture_type 与 confidence
步骤3：联调 MQTT
  看 property/post 是否每 10 秒上报，set/get 是否有 reply
步骤4：联调 App
  看在线状态、姿态状态、模式切换是否与设备一致
```
### 6.5 常见问题与定位
| 现象 | 优先排查 |
|------|----------|
| App 一直离线 | `device/detail` 的 `status` 与 `last_time`；token 是否有效 |
| ESP32 无姿态数据 | UART 引脚是否 `3/4 <-> 18/17`，波特率是否 `115200` |
| 频繁误报警 | K230 阈值 + ESP32 `cooldownMs` + `ABNORMAL_THRESHOLD_FRAMES` |
| 云端设置不生效 | identifier 拼写、`property/set` 是否命中、`set_reply` 是否返回 |
### 6.6 版本演进建议
后续要新增功能（例如久坐统计、学习时长分段、多设备支持）时，建议按以下顺序：
1. 先改 `schemas.json` 和常量文件。
2. 再改 ESP32 解析与上报。
3. 再改 App 展示和控制。
4. 最后补文档中的字段表、状态机图、调试清单。
### 6.7 本文档阅读路径建议
给首次接触该系统的同学，推荐阅读顺序：
1. 第 1 章：先建立全局图景。
2. 第 2 章：搞清硬件和通信通道。
3. 第 3 章：理解中枢控制（ESP32）。
4. 第 4 章：理解视觉端判定逻辑（K230）。
5. 第 5 章：理解用户端与云端交互（App）。
6. 第 6 章：最后掌握协议协同与调试方法。
## 7. 开发指南
本项目建议按“协议先行、三端同步、端到端验证”的顺序开发。
1. 先修改 `shared/protocol/schemas.json` 与三端常量文件。
2. 再实现 `esp32/` 的解析、状态机与 MQTT 上报。
3. 然后实现 `k230/` 的检测输出与字段对齐。
4. 最后实现 `app/` 的展示、控制与轮询策略。
每次改动后，至少完成 UART 联调、MQTT 回执检查、App 在线状态核对三项验收。
