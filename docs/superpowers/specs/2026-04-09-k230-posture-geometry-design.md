# K230 姿态几何规则改造设计

## 背景

当前 `refactored/k230/src/pose_detector.py` 同时保留了 `legacy` 与 `decoupled` 两套姿态判定路径，包含风险融合、EMA 平滑、基线预热、类别 dominance 等逻辑。现有实现虽然可工作，但对当前需求存在三个问题：

- 判定逻辑偏复杂，不利于论文中直接解释。
- 输出类别包含 `tilt`，但该类别在当前侧视、单图模拟场景中不够稳定。
- UART 对 ESP32 实际发送的是粗粒度 `posture_type=bad/normal/no_person`，不能直接表达细分类别。

本次改造目标是将 K230 姿态判定收敛为一套更直观、更适合论文表达的 5 关键点几何规则算法，并同步清理旧逻辑与更新文档。

## 目标

- 仅基于 5 个关键点完成姿态判定：`nose`、`left_shoulder`、`right_shoulder`、`left_hip`、`right_hip`
- 仅保留两类异常姿态：`hunchback`、`head_down`
- 显式输出 `unknown` 与 `no_person`
- UART 最小化输出，仅发送 `posture_type`
- 保留一个可配置的稳定帧数 `stable_frame_count`，默认值为 `1`
- 删除旧的风险融合、多模式、侧倾相关与无用配置代码
- 更新 README、架构文档及相关说明，确保代码与文档一致

## 约束与假设

- 视角为侧视，可支持左侧视与右侧视。
- 方向参考使用图像坐标系，不做人体自定义坐标系推导。
- 默认相机为固定机位，画面近似摆正。
- 当前主要验证场景为单张照片，因此默认 `stable_frame_count=1`。
- 关键点任意缺失或置信度不足时，输出 `unknown`。
- 不保留个人基线校准逻辑。

## 输出状态

### 最终姿态类别

- `normal`
- `hunchback`
- `head_down`
- `unknown`
- `no_person`

说明：

- `unknown` 表示检测到人，但 5 个必需关键点不完整或关键点质量不足。
- `no_person` 表示人体存在过滤未通过，仍由人体检测阶段负责。

## 核心变量命名

为提升代码与论文表达的一致性，使用以下直观变量名：

- `shoulder_midpoint`
- `hip_midpoint`
- `torso_length`
- `torso_tilt_deg`
- `head_tilt_deg`
- `torso_excess_ratio`
- `head_excess_ratio`
- `required_keypoints_ready`
- `stable_frame_count`

不再保留旧算法中的以下命名或同类概念：

- `posture_mode`
- `risk_ema_alpha`
- `risk_enter` / `risk_exit`
- `baseline_*`
- `class_ema`
- `shoulder_roll` / `tilt`
- `head_forward_ratio`
- `posture_type_fine`

## 几何定义

### 中间点定义

- `shoulder_midpoint = midpoint(left_shoulder, right_shoulder)`
- `hip_midpoint = midpoint(left_hip, right_hip)`

### 长度定义

- `torso_length = distance(shoulder_midpoint, hip_midpoint)`

### 角度定义

- `torso_tilt_deg`
  - 使用 `shoulder_midpoint -> hip_midpoint` 连线
  - 计算其与图像竖直方向的夹角绝对值
  - 用于识别 `hunchback`

- `head_tilt_deg`
  - 使用 `nose -> shoulder_midpoint` 连线
  - 计算其与图像竖直方向的夹角绝对值
  - 用于识别 `head_down`

说明：

- 图像竖直方向即图像坐标系 `y` 轴方向。
- 左侧视和右侧视均通过绝对值处理，不依赖左右方向符号。

## 判定流程

### 第一步：人体存在过滤

若 YOLO 姿态检测结果为空，或人体存在过滤失败，则输出：

- `posture_type = no_person`

### 第二步：关键点完整性检查

若以下任一条件不满足，则输出：

- `posture_type = unknown`

检查条件：

- 5 个必需关键点全部存在
- 5 个必需关键点置信度均达到配置阈值
- `torso_length` 大于最小有效值，避免肩中点与髋中点退化重合

### 第三步：计算姿态特征

计算：

- `torso_tilt_deg`
- `head_tilt_deg`

### 第四步：与阈值比较

若：

- `torso_tilt_deg <= torso_tilt_threshold_deg`
- `head_tilt_deg <= head_tilt_threshold_deg`

则输出：

- `posture_type = normal`

若仅一个特征超阈值，则直接输出对应类别：

- `torso_tilt_deg` 超阈值 -> `hunchback`
- `head_tilt_deg` 超阈值 -> `head_down`

若两个特征同时超阈值，则比较超阈值比例：

- `torso_excess_ratio = (torso_tilt_deg - torso_tilt_threshold_deg) / torso_tilt_threshold_deg`
- `head_excess_ratio = (head_tilt_deg - head_tilt_threshold_deg) / head_tilt_threshold_deg`

取较大者作为最终类别：

- `torso_excess_ratio > head_excess_ratio` -> `hunchback`
- `head_excess_ratio >= torso_excess_ratio` -> `head_down`

该规则用于满足“单一姿态类别输出”的需求，并保证当两类异常同时出现时，输出最明显的主导异常。

## 稳定输出规则

新增配置项：

- `stable_frame_count = 1`

含义：

- 同一类别连续出现 `stable_frame_count` 帧后，才确认输出该稳定类别。

默认策略：

- 当前单张图片测试阶段，保持 `stable_frame_count=1`
- 后续若切换到实时视频流，可调整为 `2`

实现要求：

- 保留最小状态机支持该参数
- 但状态机仅负责类别稳定，不再保留旧的风险平滑与多类别 EMA 逻辑

## UART 协议调整

### 新协议

UART 只发送最小 JSON：

```json
{"posture_type":"head_down"}
```

允许值：

- `normal`
- `hunchback`
- `head_down`
- `unknown`
- `no_person`

### 调整原因

- `is_abnormal` 可以由 ESP32 根据 `posture_type` 自行推导
- `consecutive_abnormal` 应由 ESP32 作为提醒策略状态自行维护
- `confidence` 不属于最小必要协议，默认不通过 UART 发送
- 删除 `bad` 这一粗粒度中间态，避免 K230 已知细分类但 UART 丢失语义

## 模块改造范围

### `refactored/k230/src/pose_detector.py`

需要完成：

- 删除 `legacy` / `decoupled` 双路径分支
- 删除风险融合、EMA、dominance、baseline、side-face 等旧算法逻辑
- 保留并收敛为单一的几何规则判定入口
- 使用新的直观变量命名
- 输出新的姿态类别集合
- 仅保留服务于新算法的配置读取与状态变量

### `refactored/k230/src/config.py`

需要完成：

- 删除旧算法不再使用的姿态配置项
- 新增或保留以下直观配置项：
  - `torso_tilt_threshold_deg`
  - `head_tilt_threshold_deg`
  - `required_keypoint_confidence`
  - `stable_frame_count`
- 保持注释风格，解释各参数含义与调参方向

### `refactored/k230/src/main.py`

需要完成：

- 删除 `posture_type_fine` 相关中间表示
- `process_frame()` 直接以最终 `posture_type` 为主输出
- `send_to_esp32()` 改为仅发送 `posture_type`
- 更新注释、示例 JSON 与 IDE 输出逻辑，避免与实际发送格式不一致

## 文档更新范围

本次改造必须同步更新文档，避免代码和说明脱节。

### 需要更新的文档

- `refactored/k230/README.md`
  - 更新姿态类型说明
  - 更新算法说明为 5 点几何规则
  - 更新 UART 输出格式为仅发送 `posture_type`

- `refactored/docs/architecture.md`
  - 更新 K230 姿态分析方式
  - 去除 `tilt` 与旧多模式判定的说明
  - 说明当前姿态分类已收敛为 `hunchback` 与 `head_down`

可选补充：

- 新增 `refactored/k230/docs/posture-algorithm.md`
  - 记录变量定义、公式、判定流程与阈值说明
  - 作为论文撰写或答辩时的专用引用文档

## 兼容性与影响

- ESP32 若当前依赖 `bad` / `normal` / `no_person` 的粗粒度状态，需要同步调整为直接消费细分类 `posture_type`
- 若 App 或文档中仍以 `tilt` 为现行类别，需要同步更新文字说明
- 若现有日志或联调脚本依赖 `posture_type_fine`，需要一并清理或改写

## 非目标

以下内容不在本次改造范围内：

- 引入新的关键点类型
- 增加 `tilt`、`forward_head` 等第三类异常
- 引入人体自定义坐标系作为主判定方向
- 引入个人基线校准
- 增加 host 端自动化测试框架

## 验证建议

最小验证应覆盖：

- 单张正常坐姿图片 -> `normal`
- 单张驼背图片 -> `hunchback`
- 单张低头图片 -> `head_down`
- 关键点缺失图片 -> `unknown`
- 无人图片 -> `no_person`

同时确认：

- UART 输出只包含 `posture_type`
- README 与架构文档已同步到新口径
