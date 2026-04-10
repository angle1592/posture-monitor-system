# ESP32 融合姿态、OLED 与补光设计

## 1. 背景

当前 `refactored/posture_monitor` 已完成 K230 最小 UART 协议适配，K230 侧输出的姿态类别收敛为：

- `normal`
- `head_down`
- `hunchback`
- `no_person`
- `unknown`

但 ESP32 内部消费链路仍然主要围绕旧的二值语义工作：

- 提醒逻辑主要依赖 `isAbnormal`
- OLED 页面文案较模糊，信息层级不明确
- MQTT / OneNET 物模型仍以旧字段为主
- 光照传感器当前仅展示数值，未参与本地环境联动

本设计将 ESP32 侧改为基于统一融合状态驱动提醒、显示、上报和补光，避免同一系统中出现多套判定规则。

## 2. 目标

本次设计的目标是：

1. 在 ESP32 侧引入统一融合状态，作为提醒、OLED 和 MQTT 的唯一上游输入。
2. 用 `PIR + K230` 的联合结果决定最终“是否有人”和“最终姿态类型”。
3. 将 OneNET 主上报字段切换为新的 `postureType` 枚举和 `personPresent` 布尔字段。
4. 美化 OLED 姿态页面，使界面只显示重要且易理解的信息。
5. 新增基于环境光照的本地补光逻辑，使用单颗白光 LED 接在 `GPIO18`。

## 3. 非目标

以下内容不在本次范围内：

1. 不改 K230 视觉算法本体。
2. 不引入新的云端事件模型，只改属性上报。
3. 不在本次同步修改 App 展示逻辑。
4. 不把补光升级为 PWM 调光；本次仅做开关控制。
5. 不做大规模模块重构；仅做完成目标所需的必要抽象。

## 4. 统一融合状态

### 4.1 设计原则

ESP32 内部不再让提醒、显示、上报分别自行解释 `PIR`、`K230 postureType`、`timeout` 等原始信号，而是先在单一入口处得到一个最终融合结果，后续模块只消费该结果。

### 4.2 融合输入

融合逻辑的输入包括：

- PIR 是否检测到有人
- K230 当前帧 `postureType`
- K230 当前帧是否有效
- K230 链路是否超时
- BH1750 当前照度 `ambientLux`

### 4.3 最终融合规则

最终融合判定规则如下：

1. `PIR=有人` 且 `K230 postureType in {normal, head_down, hunchback}`
   - `personPresent = true`
   - `postureType = K230 postureType`

2. 其他所有情况
   - `personPresent = false`
   - `postureType = no_person`

其中“其他所有情况”包括但不限于：

- `PIR=无人` 且 `K230=no_person`
- `PIR=无人` 且 `K230` 误报为 `normal/head_down/hunchback`
- `PIR=有人` 且 `K230=unknown`
- `PIR=有人` 且 `K230=no_person`
- `K230 数据无效`
- `K230 UART 超时`

该规则意味着：

- `PIR` 是“是否有人”的硬门控。
- `unknown` 不向上游传播，统一并入 `no_person` 路径。
- `no_person` 表示“无人或当前视觉结果不可信”，是本系统的安全兜底状态。

### 4.4 内部派生字段

融合结果除 `postureType` 外，还派生以下内部语义：

- `postureKnown`
  - 当 `postureType in {normal, head_down, hunchback}` 时为 `true`
  - 当 `postureType = no_person` 时为 `false`

- `shouldAlert`
  - 当 `postureType in {head_down, hunchback}` 时为 `true`
  - 其余情况为 `false`

- `fillLightOn`
  - 由补光规则决定，见第 7 节

## 5. MQTT / OneNET 物模型设计

### 5.1 设计原则

云端主语义不再使用旧的二值 `isPosture`，而是使用融合后的枚举 `postureType`。`personPresent` 作为辅助字段保留，用于 UI 展示和问题排查。

### 5.2 新主属性

#### `postureType`

- 标识符：`postureType`
- 名称：`坐姿状态`
- 数据类型：`int`
- 访问类型：`R`
- 取值定义：
  - `0 = no_person`
  - `1 = normal`
  - `2 = head_down`
  - `3 = hunchback`
- 默认值：`0`
- 说明：该值是 `PIR + K230` 融合后的最终姿态状态，而不是单独的视觉输出。

#### `personPresent`

- 标识符：`personPresent`
- 名称：`是否有人`
- 数据类型：`bool`
- 访问类型：`R`
- 取值定义：
  - `true = 融合结果判定当前有人`
  - `false = 融合结果判定当前无人或无有效姿态`

#### `ambientLux`

- 标识符：`ambientLux`
- 名称：`环境光照`
- 数据类型：`int`
- 访问类型：`R`
- 单位：`lx`

#### `fillLightOn`

- 标识符：`fillLightOn`
- 名称：`补光灯状态`
- 数据类型：`bool`
- 访问类型：`R`
- 取值定义：
  - `true = 补光灯开启`
  - `false = 补光灯关闭`

### 5.3 旧字段处理

旧的 `isPosture` 不再作为新的主物模型字段使用。本轮设计默认按“不兼容旧物模型主语义”实施：

- 云端和设备以 `postureType` 为主
- `personPresent` 用于补充“有人/无人”语义
- 不再用 `isPosture` 代替 `no_person`

若项目后续需要兼容旧前端，可在独立任务中添加兼容层，但不属于本次设计范围。

### 5.4 共享协议同步要求

本次改动需要同步更新以下共享协议文件：

- `refactored/shared/protocol/constants.h`
- `refactored/shared/protocol/constants.ts`
- `refactored/shared/protocol/constants.py`
- `refactored/shared/protocol/schemas.json`

同步内容包括：

1. MQTT 属性标识符新增 `postureType` 和 `fillLightOn`
2. 姿态枚举删除旧的 `tilt`、`bad` 在主姿态链路中的定义
3. 增加姿态编码值常量：`0/1/2/3`
4. 更新 schema，表达新的 MQTT 属性结构

## 6. OLED 设计

### 6.1 设计目标

当前 OLED 页面存在以下问题：

- `PIR:Y`、`Lux:OK`、`State:ok` 等文案过于抽象
- 主次信息没有明显区分
- 页面参数较多，但真正重要的信息不突出

新设计要求 OLED 在姿态模式中做到：

1. 一眼看出当前姿态结果
2. 一眼看出联网状态
3. 一眼看出 PIR 和光照环境
4. 一眼看出补光灯是否工作

### 6.2 新页面布局

姿态主页面采用四层结构：

1. 顶栏：联网状态
   - `WiFi: OK/--`
   - `MQTT: OK/--`

2. 分隔线
   - 顶栏与主状态之间加细横线，拉开层次

3. 主状态区
   - 居中大字显示：
     - `无人`
     - `正常`
     - `低头`
     - `驼背`

4. 辅助状态区
   - `PIR: 有人/无人`
   - `光照: xxx lx`
   - `补光: 开/关`

### 6.3 视觉语言

由于当前 OLED 为单色屏，不能依赖颜色表达层级，因此使用以下视觉手段：

- 顶栏使用小字体
- 主状态使用大字体并居中
- 使用分隔线区分信息区块
- 中文状态词控制在 2~4 字，保持界面整齐
- 底部辅助信息统一格式，避免缩写混杂

### 6.4 示例界面

正常：

```text
+----------------------+
| WiFi:OK    MQTT:OK   |
|----------------------|
|                      |
|         正常         |
|                      |
| PIR: 有人            |
| 光照: 126 lx         |
| 补光: 关             |
+----------------------+
```

低头：

```text
+----------------------+
| WiFi:OK    MQTT:OK   |
|----------------------|
|                      |
|         低头         |
|                      |
| PIR: 有人            |
| 光照: 44 lx          |
| 补光: 开             |
+----------------------+
```

无人：

```text
+----------------------+
| WiFi:OK    MQTT:OK   |
|----------------------|
|                      |
|         无人         |
|                      |
| PIR: 无人            |
| 光照: 18 lx          |
| 补光: 关             |
+----------------------+
```

## 7. 补光设计

### 7.1 目标

将 BH1750 的环境光照值真正接入控制链路。当环境过暗且 PIR 认为有人时，自动打开本地补光 LED。

### 7.2 控制规则

补光规则如下：

- 当 `PIR=有人` 且 `ambientLux < FILL_LIGHT_LUX_THRESHOLD` 时，`fillLightOn = true`
- 其他所有情况，`fillLightOn = false`

该规则有意不依赖 K230 姿态分类结果，以避免视觉波动导致补光频繁闪烁。

### 7.3 硬件与引脚

本轮使用单颗白光 LED，分配：

- `GPIO18` -> 补光 LED 控制引脚

接线要求：

```text
GPIO18 -> 限流电阻 -> LED 长脚(阳极)
LED 短脚(阴极) -> GND
```

注意事项：

- 不允许 GPIO 直接裸接 LED，必须串限流电阻
- 推荐电阻值：`220Ω ~ 1kΩ`
- 建议优先使用 `330Ω` 或 `470Ω`

### 7.4 新增配置项

建议在 `refactored/posture_monitor/config.h` 中新增：

- `ENABLE_FILL_LIGHT`
- `FILL_LIGHT_PIN`
- `FILL_LIGHT_LUX_THRESHOLD`

其中：

- `FILL_LIGHT_PIN = 18`
- `FILL_LIGHT_LUX_THRESHOLD` 在实现阶段给出默认值，并保留后续调参空间

## 8. 模块边界与文件影响

### 8.1 ESP32 固件侧

建议修改的核心文件：

- `refactored/posture_monitor/posture_monitor.ino`
  - 融合状态计算
  - MQTT 属性构建
  - 定时上报决策切换到融合结果

- `refactored/posture_monitor/timer_fsm.cpp`
  - 提醒策略改为消费融合后的 `shouldAlert`

- `refactored/posture_monitor/display.cpp`
  - OLED 姿态页面重排与文案更新

- `refactored/posture_monitor/display.h`
  - 若显示接口需要新增融合态字段，则同步调整

- `refactored/posture_monitor/config.h`
  - 新增物模型属性名常量
  - 新增补光引脚/阈值/开关配置

- `refactored/posture_monitor/sensors.h`
- `refactored/posture_monitor/sensors.cpp`
  - 若补光控制归入环境模块，则在此扩展控制接口

### 8.2 共享协议侧

- `refactored/shared/protocol/constants.h`
- `refactored/shared/protocol/constants.ts`
- `refactored/shared/protocol/constants.py`
- `refactored/shared/protocol/schemas.json`

## 9. 风险与处理

### 9.1 PIR 门控过强

使用 `PIR` 作为硬门控后，如果 PIR 漏检，系统会把姿态统一降级为 `no_person`。这是本设计明确接受的折中：

- 优点：不会在无人时误报异常
- 代价：PIR 漏检时可能丢失有效姿态帧

本轮优先保证“无人不误报”的安全性，而不是极限识别覆盖率。

### 9.2 OLED 文案截断

OLED 可用宽度有限，中文与英文混排时必须控制字符串长度。实现阶段需要使用固定短文案，避免运行时拼接过长字符串导致换行混乱。

### 9.3 补光阈值需要现场调参

不同环境下 BH1750 的 lux 阈值可能差异较大。实现时应保留阈值常量，并在文档中说明调参方法。

## 10. 验证要求

实现后至少应验证以下场景：

1. `PIR=有人 + K230=normal` -> 上报 `postureType=1`，OLED 显示“正常”，不提醒
2. `PIR=有人 + K230=head_down` -> 上报 `postureType=2`，OLED 显示“低头”，触发提醒
3. `PIR=有人 + K230=hunchback` -> 上报 `postureType=3`，OLED 显示“驼背”，触发提醒
4. `PIR=有人 + K230=unknown` -> 上报 `postureType=0`，OLED 显示“无人”，不提醒
5. `PIR=无人 + 任意 K230` -> 上报 `postureType=0`，OLED 显示“无人”，不提醒
6. `PIR=有人 + 低光照` -> 补光灯开启
7. `PIR=无人` 或 `光照恢复` -> 补光灯关闭
8. OneNET 物模型能正确接收：`postureType`、`personPresent`、`ambientLux`、`fillLightOn`

## 11. 实施建议

建议先完成以下顺序：

1. 先补共享协议常量与 schema
2. 再补 ESP32 融合状态与 MQTT 上报
3. 再改提醒逻辑
4. 再改 OLED 页面
5. 最后接入补光 LED 控制与现场调试

这样可以先把“语义”和“上报”稳定下来，再进入硬件联动和界面打磨。
