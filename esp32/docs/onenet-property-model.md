# OneNET 物模型字段规范（本地/云端统一）

本文定义当前固件使用的物模型标识符、类型、范围与示例 JSON。
对应代码常量在 `config.h`（`PROP_ID_*`）。

## 1. 字段清单

| 标识符 | 类型 | 范围/枚举 | 说明 |
|---|---|---|---|
| `isPosture` | bool | `true/false` | 当前坐姿是否正常 |
| `monitoringEnabled` | bool | `true/false` | 坐姿检测开关 |
| `currentMode` | enum(int) | `0/1/2` | 设备模式：0=坐姿检测, 1=时钟, 2=定时器 |
| `alertModeMask` | int | `0..7` | 报警方式位掩码：1=LED, 2=BUZZER, 4=VOICE |
| `cooldownMs` | int | `1000..600000` | 在异常持续情况下的提醒冷却时间 |
| `timerDurationSec` | int | `60..7200` | 定时器时长（秒） |
| `timerRunning` | bool | `true/false` | 定时器运行状态 |
| `cfgVersion` | int | `>=1` | 本地配置版本号（递增） |
| `selfTest` | int | `1..100000` | 自检触发命令（下发后执行语音/灯光/蜂鸣器测试） |

## 2. `alertModeMask` 位定义

```text
bit0 (1): LED
bit1 (2): BUZZER
bit2 (4): VOICE
```

示例：
- `1` -> 仅 LED
- `3` -> LED + BUZZER
- `7` -> LED + BUZZER + VOICE
- `0` -> 全关（不建议）

## 3. OneNET `property/set` 示例

### 3.1 仅改报警方式

```json
{
  "id": "1001",
  "version": "1.0",
  "params": {
    "alertModeMask": { "value": 7 }
  }
}
```

### 3.2 一次下发多字段

```json
{
  "id": "1002",
  "version": "1.0",
  "params": {
    "isPosture": { "value": true },
    "monitoringEnabled": { "value": true },
    "currentMode": { "value": 0 },
    "cooldownMs": { "value": 30000 },
    "timerDurationSec": { "value": 1500 },
    "timerRunning": { "value": false }
  }
}
```

## 4. OneNET `property/get` 返回示例

设备会返回当前生效值（effective config）：

```json
{
  "id": "2001",
  "code": 200,
  "msg": "success",
  "data": {
    "isPosture": { "value": true },
    "monitoringEnabled": { "value": true },
    "currentMode": { "value": 0 },
    "alertModeMask": { "value": 7 },
    "cooldownMs": { "value": 30000 },
    "timerDurationSec": { "value": 1500 },
    "timerRunning": { "value": false },
    "cfgVersion": { "value": 5 }
  }
}
```

## 5. 参数校验规则（固件侧）

- `cooldownMs` 越界自动夹紧到 `1000..600000`
- `timerDurationSec` 越界自动夹紧到 `60..7200`
- `alertModeMask` 仅取低 3 位（`value & 0x07`）

## 6. 兼容说明（旧字段）

- 固件仍兼容读取旧字段 `remindIntervalMs`。
- 若云端下发 `remindIntervalMs`，设备会映射到 `cooldownMs`。
- 建议云端模型统一只保留 `cooldownMs`，避免重复语义。

## 7. 本地与云端协同建议

- 本地（EC11）和云端都能修改 `timerDurationSec` / `timerRunning`。
- 云端改参后，建议立即发起 `property/get` 做闭环确认。
- 如需强一致，可在平台侧以 `cfgVersion` 做操作顺序判定（后写覆盖先写）。
- App 控制页“硬件自检”按钮通过下发 `selfTest` 触发一次语音/灯光/蜂鸣器自检。
