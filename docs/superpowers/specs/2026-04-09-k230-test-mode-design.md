# K230 测试模式设计

## 背景

当前 K230 主循环已经改为新的 5 关键点几何姿态判定，并将 UART 输出最小化为 `posture_type`。但当前用于算法效果验证时，还缺少一套专门面向 CanMV IDE 的“测试模式”：

- 需要在 IDE 中看到实时预览画面。
- 需要在 IDE 串口中逐帧看到 `posture_type`、时延与帧率。
- 需要临时关闭发往 ESP32 的 UART，避免联调链路干扰测试。

用户当前的测试方式是让真实摄像头实时采集放置在镜头前的图片，因此输入源仍然是摄像头，而不是离线图片文件。

## 目标

- 新增一个显式的 `test_mode` 配置开关。
- 当 `test_mode=True` 时，自动切换到“K230 IDE 测试模式”。
- 测试模式下强制打开 IDE 预览。
- 测试模式下强制关闭发往 ESP32 的 UART。
- 测试模式下在 IDE 串口逐帧输出简洁的调试行。

## 非目标

- 不新增离线图片输入。
- 不改动姿态分类算法本身。
- 不改动 ESP32 侧提醒策略。
- 不新增复杂的多模式脚本入口。

## 配置设计

在 `k230/src/config.py` 的 `APP_CONFIG` 中新增：

- `test_mode: True/False`

建议默认值：

- `test_mode = True`

这样做的原因：

- 当前阶段用户的主要诉求是先验证算法效果。
- 测试模式默认开启，可以避免忘记打开 IDE 预览或忘记关闭 ESP32 UART。
- 后续联调时再手动改回 `False` 即可。

## 测试模式行为

当 `test_mode=True` 时，运行时应强制采用以下行为：

- `enable_esp32_uart = False`
- `ide_preview = True`
- `runtime_verbose_logs = False`
- `metrics_enabled = False`

说明：

- `enable_esp32_uart = False`：避免向 ESP32 发送 JSON，测试期间只看 IDE。
- `ide_preview = True`：确保 CanMV IDE 可看到实时画面。
- `runtime_verbose_logs = False`：避免被其他冗长日志淹没逐帧测试输出。
- `metrics_enabled = False`：避免周期性 KPI 汇总干扰逐帧观察。

## IDE 串口输出设计

测试模式下，IDE 串口按“每帧一行”输出，格式固定为：

```text
frame=37 posture=head_down latency_ms=82 fps=8.9
```

字段定义：

- `frame`：当前帧号
- `posture`：当前帧最终姿态类别
- `latency_ms`：该帧从进入 `process_frame()` 到得到结果的推理/分析耗时
- `fps`：当前帧循环测得的帧率

以下类别都必须原样输出：

- `normal`
- `head_down`
- `hunchback`
- `unknown`
- `no_person`

例如：

```text
frame=12 posture=normal latency_ms=76 fps=9.4
frame=13 posture=head_down latency_ms=81 fps=9.0
frame=14 posture=unknown latency_ms=79 fps=9.1
frame=15 posture=no_person latency_ms=73 fps=9.7
```

## 运行流程调整

### 初始化阶段

`main.py` 初始化时：

- 读取 `test_mode`
- 若开启测试模式，打印一条明确提示，例如：

```text
[*] 当前运行于测试模式：IDE预览=开，ESP32 UART=关，逐帧日志=开
```

### 主循环阶段

主循环中：

- 继续按当前方式采集摄像头帧
- 继续调用新几何姿态算法
- 继续计算 `infer_ms` 与 `fps`
- 若 `test_mode=True`，则始终将结果输出到 IDE 串口
- 不调用 `send_to_esp32()`

## 与现有模块的关系

### `k230/src/config.py`

新增：

- `test_mode`

### `k230/src/main.py`

需要完成：

- 在初始化阶段根据 `test_mode` 派生运行开关
- 增加测试模式启动提示
- 增加逐帧 IDE 串口输出格式
- 在测试模式下跳过 ESP32 UART 发送分支

### `k230/src/camera.py`

原则上无需新增新接口。

前提是：

- `ide_preview=True` 时，当前 `Display.init(..., to_ide=True)` 链路能正确工作

若实际测试发现 IDE 仍看不到预览，再单独排查：

- `camera_preview_channel`
- `camera_preview_pixformat`
- `ide_preview_mode`

但这些属于实现验证问题，不是这次设计的主变更点。

## 验证标准

测试模式完成后，应满足：

1. CanMV IDE 中可见实时预览画面
2. IDE 串口每帧输出一行：`frame / posture / latency_ms / fps`
3. 运行时不向 ESP32 UART 发送任何 JSON
4. 切换 `test_mode=False` 后，可恢复原有联调路径
