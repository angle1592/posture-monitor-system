/*
 * 模块职责：集中维护移动端与设备端共享的协议常量。
 *
 * 与其他模块关系：
 * - store.ts：消费 POLLING_INTERVALS / DEVICE_DEFAULTS / PROP_IDS 进行轮询、在线判定与属性映射。
 * - oneNetApi.ts：按 PROP_IDS 组织设备属性读写字段。
 * - 页面层：通过 SYSTEM_MODE_LABELS 将模式值转成中文展示文本。
 *
 * 映射来源：该文件与 ESP32/K230 协议字段保持一致，协议变更时需同步更新。
 */

// 姿势类型枚举：对应视觉检测侧输出的姿势类别编码。
export const POSTURE_TYPES = {
  NORMAL: 'normal',
  HEAD_DOWN: 'head_down',
  HUNCHBACK: 'hunchback',
  TILT: 'tilt',
  NO_PERSON: 'no_person',
  UNKNOWN: 'unknown',
  BAD: 'bad',
} as const

export type PostureType = (typeof POSTURE_TYPES)[keyof typeof POSTURE_TYPES]

// 系统模式枚举：与设备固件 currentMode 数值约定一一对应。
export const SYSTEM_MODES = {
  POSTURE: 0,
  CLOCK: 1,
  TIMER: 2,
} as const

export type SystemModeValue = (typeof SYSTEM_MODES)[keyof typeof SYSTEM_MODES]

export const SYSTEM_MODE_NAMES: Record<SystemModeValue, string> = {
  [SYSTEM_MODES.POSTURE]: 'posture',
  [SYSTEM_MODES.CLOCK]: 'clock',
  [SYSTEM_MODES.TIMER]: 'timer',
}

export const SYSTEM_MODE_LABELS: Record<string, string> = {
  // UI 展示文案映射：页面统一从该对象取模式中文名，避免多处硬编码。
  posture: '坐姿检测模式',
  clock: '时钟模式',
  timer: '定时器模式',
}

// 属性标识映射：ESP32/K230 <-> OneNET 物模型 identifier 统一入口。
export const PROP_IDS = {
  IS_POSTURE: 'isPosture',
  MONITORING_ENABLED: 'monitoringEnabled',
  CURRENT_MODE: 'currentMode',
  ALERT_MODE_MASK: 'alertModeMask',
  COOLDOWN_MS: 'cooldownMs',
  TIMER_DURATION_SEC: 'timerDurationSec',
  TIMER_RUNNING: 'timerRunning',
  CFG_VERSION: 'cfgVersion',
  SELF_TEST: 'selfTest',
} as const

export type PropertyIdentifier = (typeof PROP_IDS)[keyof typeof PROP_IDS]

// 提醒方式位掩码：与设备端 alertModeMask 按位协议保持一致。
export const ALERT_MODES = {
  LED: 1 << 0,     // 0x01
  BUZZER: 1 << 1,  // 0x02
  VOICE: 1 << 2,   // 0x04
  ALL: 0x07,
} as const

// 轮询间隔配置（毫秒）：前台/后台/实时三档，供 store 轮询策略动态切换。
export const POLLING_INTERVALS = {
  BACKGROUND: 5000,
  NORMAL: 2000,
  REALTIME: 800,
} as const

// 设备通信默认值：在线超时、上报节奏、冷却时间、默认计时时长等基础参数。
export const DEVICE_DEFAULTS = {
  ONLINE_TIMEOUT_MS: 300000,       // 在线超时阈值（5分钟）
  PUBLISH_INTERVAL_MS: 10000,
  ALERT_COOLDOWN_MS: 30000,
  TIMER_DURATION_SEC: 1500,
}
