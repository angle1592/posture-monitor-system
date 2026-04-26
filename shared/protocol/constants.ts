/**
 * Shared protocol constants for Mobile App.
 *
 * This file is the SINGLE SOURCE OF TRUTH for protocol field names,
 * enum values, and communication configuration shared across the system.
 * Keep in sync with constants.py (K230) and constants.h (ESP32).
 */

// ============================================================
// Posture Type Enum
// ============================================================
export const POSTURE_TYPES = {
  NORMAL: 'normal',
  HEAD_DOWN: 'head_down',
  HUNCHBACK: 'hunchback',
  NO_PERSON: 'no_person',
  UNKNOWN: 'unknown',
} as const

export type PostureType = (typeof POSTURE_TYPES)[keyof typeof POSTURE_TYPES]

export const POSTURE_TYPE_CODES = {
  NO_PERSON: 0,
  NORMAL: 1,
  HEAD_DOWN: 2,
  HUNCHBACK: 3,
} as const

export type PostureTypeCode =
  (typeof POSTURE_TYPE_CODES)[keyof typeof POSTURE_TYPE_CODES]

// ============================================================
// System Modes
// ============================================================
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
  posture: '坐姿检测模式',
  clock: '时钟模式',
  timer: '定时器模式',
}

// ============================================================
// MQTT Property Identifiers (ESP32 <-> OneNET)
// ============================================================
export const PROP_IDS = {
  POSTURE_TYPE: 'postureType',
  MONITORING_ENABLED: 'monitoringEnabled',
  CURRENT_MODE: 'currentMode',
  PERSON_PRESENT: 'personPresent',
  AMBIENT_LUX: 'ambientLux',
  FILL_LIGHT_ON: 'fillLightOn',
  ALERT_MODE_MASK: 'alertModeMask',
  COOLDOWN_MS: 'cooldownMs',
  TIMER_DURATION_SEC: 'timerDurationSec',
  TIMER_RUNNING: 'timerRunning',
  CFG_VERSION: 'cfgVersion',
  SELF_TEST: 'selfTest',
  IS_POSTURE: 'isPosture',
} as const

export type PropertyIdentifier = (typeof PROP_IDS)[keyof typeof PROP_IDS]

// ============================================================
// Alert Mode Bitmask
// ============================================================
export const ALERT_MODES = {
  LED: 1 << 0,     // 0x01
  BUZZER: 1 << 1,  // 0x02
  VOICE: 1 << 2,   // 0x04
  ALL: 0x07,
} as const

// ============================================================
// Polling Intervals (ms)
// ============================================================
export const POLLING_INTERVALS = {
  BACKGROUND: 5000,
  NORMAL: 2000,
  REALTIME: 800,
} as const

// ============================================================
// Device Communication Defaults
// ============================================================
export const DEVICE_DEFAULTS = {
  ONLINE_TIMEOUT_MS: 300000,       // 5 minutes
  PUBLISH_INTERVAL_MS: 10000,
  ALERT_COOLDOWN_MS: 5000,
  TIMER_DURATION_SEC: 1500,
}
