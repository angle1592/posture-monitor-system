# App HTTP + MQTT Hybrid Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不破坏现有 OneNET HTTP 轮询能力的前提下，为 `refactored/app` 引入 MQTT 实时消息通道，使 H5/App Plus 获得更低延迟的状态同步，同时保留历史查询、属性写入和不支持平台的 HTTP 兜底。

**Architecture:** 保留 `src/utils/oneNetApi.ts` 作为 HTTP 控制面：查询属性、查询历史、查询设备在线状态、下发属性写入。新增一层“实时传输适配层”承接 MQTT 连接、订阅和消息归一化，再由 `src/utils/store.ts` 同时消费 HTTP 拉取结果与 MQTT 推送结果。平台策略按能力分层：H5/App Plus 优先启用 MQTT + 轮询降频，MP-WEIXIN 暂时继续纯 HTTP 轮询，直到有原生 Socket 版 MQTT 实现。

**Tech Stack:** uni-app、Vue 3、TypeScript、Vite、OneNET HTTP API、MQTT over WebSocket（仅 H5/App Plus 第一阶段启用）

---

## 当前状态快照（基于仓库证据）

- `src/utils/oneNetApi.ts` 当前通过 `uni.request` 访问 `https://iot-api.heclouds.com/thingmodel` 与 `https://iot-api.heclouds.com/device`，说明主链路是 HTTPS。
- `src/utils/store.ts` 当前负责 `fetchLatest()` 与多档轮询调度，页面通过提升/降低轮询档位获得“准实时”体验。
- `src/pages/control/index.vue` 当前所有控制指令都调用 `setDeviceProperty()` 走 HTTP 下发。
- `src/utils/mqtt.js` 已有 MQTT 单例封装雏形，但尚未接入 `store.ts` 或页面主流程；其 `MP-WEIXIN` 分支明确写明当前不可用。
- `package.json` 当前没有 `mqtt` 依赖，`.env.example` 也没有 MQTT 配置项，说明 MQTT 还未进入正式可运行状态。

## 推荐目标边界

### 保留 HTTP 的职责
- `queryDeviceProperty()`：首屏冷启动、MQTT 断线兜底、写入后主动回读。
- `queryPropertyHistory()`：历史曲线与报表查询。
- `queryDeviceStatus()`：与 MQTT 连接状态交叉判断在线性，避免“MQTT 连着但设备已离线”的误判。
- `setDeviceProperty()`：属性写入、写入失败重试、错误归一化。

### 交给 MQTT 的职责
- 实时姿势变化推送。
- 实时 presence / fill-light / mode / timer 等属性变化推送。
- 连接状态回调，用于前端动态切换“实时模式”和“轮询兜底模式”。

### 第一阶段不做的事
- 不做 MQTT-only 全量切换。
- 不在第一阶段支持 MP-WEIXIN 的 MQTT 直连。
- 不修改设备端协议字段定义；仍以 `src/utils/constants.ts` 中现有 `PROP_IDS` 为前端消费入口。
- 不假设具体 topic 名称；必须先从 OneNET 产品配置确认实际 broker/topic 规则后再落代码。

## 文件结构设计

### 继续保留的文件
- `src/utils/oneNetApi.ts`：HTTP 请求与错误归一化，继续作为控制面入口。
- `src/utils/store.ts`：统一状态中心，新增实时连接与降级切换逻辑。
- `src/pages/control/index.vue`：继续通过 HTTP 下发控制命令。
- `.env.example`：增加 MQTT 配置占位。
- `src/env.d.ts`：补充 MQTT 相关环境变量类型声明。

### 新增/替换的文件
- Create: `src/utils/realtime/mqtt-client.ts`
  - 负责 H5/App Plus 下的 MQTT 连接、重连、订阅、取消订阅、断连回调。
- Create: `src/utils/realtime/message-adapter.ts`
  - 负责把 MQTT payload 归一化成前端现有可消费的属性更新结构，避免 `store.ts` 直接耦合 broker 原始报文。
- Create: `src/utils/realtime/types.ts`
  - 统一定义实时消息、连接状态、属性 patch 的 TypeScript 类型。
- Optional Delete after cutover: `src/utils/mqtt.js`
  - 当 `mqtt-client.ts` 完整替代后删除；若阶段性保留，则标记为 legacy wrapper 并停止新引用。
- Create: `docs/app-http-mqtt-architecture.md`
  - 记录 broker、topic、平台差异、降级策略和运维检查点。

---

### Task 1: 锁定协议边界与平台范围

**Files:**
- Modify: `docs/app-http-mqtt-architecture.md`
- Review: `src/utils/oneNetApi.ts`
- Review: `src/utils/store.ts`
- Review: `src/utils/mqtt.js`
- Review: `.env.example`

- [ ] **Step 1: 记录当前 HTTP 职责边界**

在 `docs/app-http-mqtt-architecture.md` 中明确写出以下职责：
- HTTP 负责 `queryDeviceProperty` / `queryPropertyHistory` / `queryDeviceStatus` / `setDeviceProperty`
- MQTT 只负责“实时推送”和“连接状态”，不接管历史查询与控制写入
- `store.ts` 继续是 UI 的唯一状态入口

- [ ] **Step 2: 固化第一阶段平台策略**

在文档中明确：
- H5/App Plus：启用 MQTT over WebSocket
- MP-WEIXIN：继续纯 HTTP 轮询
- 任何平台只要 MQTT 连接失败，自动回退到现有 HTTP 轮询，不影响控制和历史功能

- [ ] **Step 3: 从 OneNET 控制台补齐运行时缺失信息**

在落代码前必须补齐并写入文档：
- broker host
- broker port
- 是否使用 TLS
- clientId / username / password 的生成规则
- 设备上行 topic
- App 订阅 topic
- 是否存在属性推送与事件推送的不同 topic

验证标准：文档中出现完整的 broker/topic 清单，而不是只写“后续补充”。

- [ ] **Step 4: 定义第一阶段验收口径**

在文档中写明验收标准：
- H5/App Plus 连接 MQTT 后，姿势变化能在 1 秒内更新页面
- MQTT 断开后，页面能回退到现有轮询，不出现“卡死在线”
- 控制页下发命令仍走 HTTP，并能在 MQTT 无推送时通过主动回读确认状态
- MP-WEIXIN 功能不倒退

### Task 2: 建立类型安全的 MQTT 实时层

**Files:**
- Create: `src/utils/realtime/types.ts`
- Create: `src/utils/realtime/mqtt-client.ts`
- Create: `src/utils/realtime/message-adapter.ts`
- Modify: `src/env.d.ts`
- Modify: `.env.example`
- Modify: `package.json`

- [ ] **Step 1: 新增 MQTT 依赖并补齐环境变量**

修改 `package.json`：增加 `mqtt` 依赖。

修改 `.env.example`：增加以下配置项并保留现有 OneNET 配置：
```dotenv
VITE_MQTT_ENABLED=true
VITE_MQTT_HOST=YOUR_MQTT_HOST
VITE_MQTT_PORT=8084
VITE_MQTT_USE_SSL=true
VITE_MQTT_CLIENT_ID=YOUR_CLIENT_ID
VITE_MQTT_USERNAME=YOUR_USERNAME
VITE_MQTT_PASSWORD=YOUR_PASSWORD
VITE_MQTT_TOPIC_UP=YOUR_UP_TOPIC
VITE_MQTT_TOPIC_DOWN=YOUR_DOWN_TOPIC
```

修改 `src/env.d.ts`：为以上变量补充 `ImportMetaEnv` 类型声明，避免在 TS 中出现隐式 `any`。

- [ ] **Step 2: 用 TypeScript 重建 MQTT 客户端包装层**

在 `src/utils/realtime/types.ts` 中定义：
- `RealtimeConnectionState = 'idle' | 'connecting' | 'connected' | 'degraded' | 'unsupported'`
- `RealtimePropertyPatch`
- `RealtimeMessageEnvelope`
- `RealtimeTransportConfig`

在 `src/utils/realtime/mqtt-client.ts` 中实现：
- `initRealtimeClient(config)`
- `connectRealtime()`
- `disconnectRealtime()`
- `subscribeRealtime(topic, handler)`
- `getRealtimeState()`
- `onRealtimeStateChange(handler)`

约束：
- H5/App Plus 才真正建立 WebSocket MQTT 连接
- MP-WEIXIN 直接返回 `unsupported`
- 连接失败后状态必须进入 `degraded`，供 `store.ts` 决定是否恢复高频轮询

- [ ] **Step 3: 建立消息归一化适配器**

在 `src/utils/realtime/message-adapter.ts` 中实现两个纯函数：
- `parseRealtimePayload(raw: string): RealtimeMessageEnvelope | null`
- `toPropertyPatch(message: RealtimeMessageEnvelope): RealtimePropertyPatch | null`

约束：
- 只做“解析 + 映射”，不做 UI 状态写入
- 输出字段名必须对齐 `src/utils/constants.ts` 的 `PROP_IDS`
- 如果 payload 缺字段或格式异常，返回 `null` 并记录带前缀的日志，例如 `[Realtime] 无法解析 payload`

- [ ] **Step 4: 验证新增实时层本身可被类型检查通过**

运行：
```bash
npm run type-check
```

预期：
- `vue-tsc --noEmit` 退出码为 `0`
- 不因新增环境变量、MQTT 类型或平台分支报错

### Task 3: 在 store 中接入“推送优先，轮询兜底”的混合策略

**Files:**
- Modify: `src/utils/store.ts`
- Review: `src/utils/constants.ts`
- Review: `src/utils/realtime/types.ts`
- Review: `src/utils/realtime/mqtt-client.ts`
- Review: `src/utils/realtime/message-adapter.ts`

- [ ] **Step 1: 为 store 增加实时连接状态字段**

在 `AppState` 中新增：
- `realtimeState`
- `realtimeAvailable`
- `lastRealtimeMessageTime`

新增计算规则：
- MQTT 已连接且最近消息新鲜时，视为“实时通道可用”
- MQTT 不可用时，不改变现有 HTTP 轮询逻辑

- [ ] **Step 2: 把 MQTT patch 合并到现有状态更新流程**

在 `store.ts` 新增一个内部方法，例如：
- `applyPropertyPatch(patch: RealtimePropertyPatch)`

该方法复用 `fetchLatest()` 里现有属性归一化规则，至少覆盖：
- `postureType`
- `monitoringEnabled`
- `currentMode`
- `personPresent`
- `fillLightOn`
- `timerRunning`
- `timerDurationSec`

要求：
- 不复制两份业务映射逻辑；能抽公共逻辑就抽到共享函数
- MQTT 与 HTTP 最终都走同一套状态归一化路径

- [ ] **Step 3: 重写轮询策略切换条件**

在现有 `applyPollingPolicy()` 基础上增加规则：
- `connected`：从 `realtime` 档降到 `normal` 或更低的保底轮询
- `degraded` / `unsupported`：恢复现有 `realtime` / `normal` / `background` 档逻辑
- 长时间未收到 MQTT 消息：主动视为降级，重新启用更高频 HTTP 拉取

要求：
- 不删除当前 `startPolling()` / `stopPolling()` / `setPollingProfile()` 公开接口
- 页面层不需要感知复杂状态机，只继续调用现有 store API

- [ ] **Step 4: 在初始化与前后台切换时管理 MQTT 生命周期**

在 `init()`、页面可见性处理或等效入口中补充：
- 应用启动时初始化实时层配置
- 页面进入前台时尝试连接 MQTT
- 页面退到后台时可按平台策略保持连接或断开；第一阶段优先选择“保守不断开，只降频 HTTP”

验证标准：
- 现有页面不需要大改即可继续工作
- MQTT 可用时状态更新更快，不可用时仍与当前行为一致

### Task 4: 保持 HTTP 写入链路稳定，并增加 MQTT 缺席时的写后确认

**Files:**
- Modify: `src/utils/oneNetApi.ts`
- Modify: `src/pages/control/index.vue`
- Modify: `src/utils/store.ts`

- [ ] **Step 1: 保留 `setDeviceProperty()` 为唯一写入入口**

要求：
- 控制页继续只调用 `setDeviceProperty()`
- 不在控制页直接调用 MQTT `publish()` 发送控制命令
- 这样可以继续复用当前的错误处理、非法 identifier 过滤和 HTTP 返回语义

- [ ] **Step 2: 在写入成功后增加“短闭环确认”**

当控制页调用 `setDeviceProperty()` 成功后：
- 若实时通道可用，优先等待 MQTT 推送刷新 UI
- 若实时通道不可用，调用一次 `store.fetchLatest()` 做写后确认

该规则至少应用到以下交互：
- `switchMode()`
- `toggleReminder()`
- `onTimerSwitch()`
- `onIntervalChange()`
- `runHardwareSelfTest()`

- [ ] **Step 3: 明确在线判定的双通道语义**

在线判定继续按“HTTP 设备详情 + 属性时间戳兜底”为准；MQTT 连接状态只作为“实时通道可用性”信号，不能单独替代设备在线判定。

这样可以避免出现：
- WebSocket 还连着，但设备已经停止上报
- Broker 可达，但设备实体离线

### Task 5: 文档与兼容策略收口

**Files:**
- Modify: `README.md`
- Modify: `docs/app-http-mqtt-architecture.md`
- Optional Modify: `src/utils/mqtt.js`

- [ ] **Step 1: 在 README 中写清通信分层**

补充说明：
- 当前控制/历史仍走 OneNET HTTP API
- 实时更新可选走 MQTT
- 微信小程序第一阶段仍使用 HTTP 轮询

- [ ] **Step 2: 标记旧 `src/utils/mqtt.js` 的处理策略**

二选一，且必须在文档里写清：
- 方案 A：迁移完成后删除 `src/utils/mqtt.js`
- 方案 B：临时保留，但文件头部注释明确标记为 legacy，不再新增引用

推荐使用方案 A，避免 JS/TS 双实现长期并存。

- [ ] **Step 3: 把运维排查点写进文档**

在 `docs/app-http-mqtt-architecture.md` 增加排查清单：
- broker 不通时看什么日志
- topic 配错时页面会出现什么症状
- 连接成功但无消息时如何区分“无推送”与“设备离线”
- 如何强制回退到 HTTP-only 模式（例如 `VITE_MQTT_ENABLED=false`）

### Task 6: 验证与发布顺序

**Files:**
- Review: `package.json`
- Review: `src/utils/store.ts`
- Review: `src/utils/realtime/*`
- Review: `src/pages/control/index.vue`

- [ ] **Step 1: 执行静态校验**

运行：
```bash
npm run type-check
npm run test
npm run lint
```

预期：
- 类型检查通过
- 现有测试不回归
- 新增 TS 文件和环境变量声明不引入 lint 错误

- [ ] **Step 2: 执行 H5 手工验证**

运行：
```bash
npm run dev:h5
```

在浏览器中验证：
- 首屏加载仍能显示状态
- MQTT 建连成功后，姿势变化能自动刷新
- 断开 broker 或填错凭据后，页面仍能靠 HTTP 轮询更新
- 控制页操作后，UI 能通过 MQTT 推送或 `fetchLatest()` 闭环更新

- [ ] **Step 3: 执行 MP-WEIXIN 不退化验证**

运行：
```bash
npm run build:mp-weixin
```

验证：
- 构建通过
- 没有因为 H5/App Plus 的 MQTT 依赖导致小程序编译失败
- 小程序端仍保留当前 HTTP 轮询行为

- [ ] **Step 4: 按阶段上线**

推荐发布顺序：
1. 先合入文档、配置、实时层骨架，不接主流程
2. 再接入 `store.ts` 的实时消费与降级逻辑
3. 最后补控制页“写后确认”闭环

这样即使中途回滚，也能保住现有 HTTP 主链路。

---

## 风险清单

- **平台风险：** `MP-WEIXIN` 当前明确不支持现有 MQTT 实现，不能把它算进第一阶段交付。
- **协议风险：** 仓库内没有 broker/topic 真值，必须先补齐 OneNET 控制台配置，否则会把错误推到运行期。
- **状态一致性风险：** 如果 MQTT 与 HTTP 同时更新状态但不共用归一化逻辑，会出现 UI 闪烁或字段覆盖。
- **依赖风险：** 当前 `package.json` 没有 `mqtt` 依赖，引入后要验证多端构建兼容性。
- **误判风险：** MQTT 连接成功不等于设备在线，在线性判断不能只看 socket 状态。

## 推荐实施顺序（一句话版）

先文档化 OneNET MQTT 运行参数，再补类型安全的 MQTT 实时层，然后把 `store.ts` 改成“推送优先、轮询兜底”，最后只在控制页增加写后确认，不动现有 HTTP 写入主链路。
