/**
 * Shared protocol constants for ESP32 firmware.
 *
 * This file is the SINGLE SOURCE OF TRUTH for protocol field names,
 * enum values, and communication configuration shared across the system.
 * Keep in sync with constants.py (K230) and constants.ts (App).
 */

#ifndef SHARED_PROTOCOL_CONSTANTS_H
#define SHARED_PROTOCOL_CONSTANTS_H

// ============================================================
// UART Configuration (K230 -> ESP32)
// ============================================================
#define PROTO_UART_BAUDRATE       9600
#define PROTO_K230_TX_PIN         15
#define PROTO_K230_RX_PIN         16

// ============================================================
// K230 UART JSON Field Names
// ============================================================
// Active contract: the refactored K230 sends the minimal posture frame.
#define FIELD_POSTURE_TYPE        "posture_type"

// ============================================================
// Posture Type Enum Values
// ============================================================
#define POSTURE_NORMAL            "normal"
#define POSTURE_HEAD_DOWN         "head_down"
#define POSTURE_HUNCHBACK         "hunchback"
#define POSTURE_NO_PERSON         "no_person"
#define POSTURE_UNKNOWN           "unknown"

// ============================================================
// Fused Posture Type Codes (ESP32 -> OneNET)
// ============================================================
#define POSTURE_CODE_NO_PERSON    0
#define POSTURE_CODE_NORMAL       1
#define POSTURE_CODE_HEAD_DOWN    2
#define POSTURE_CODE_HUNCHBACK    3

// ============================================================
// MQTT Property Identifiers (ESP32 <-> OneNET)
// ============================================================
#define PROP_ID_POSTURE_TYPE      "postureType"
#define PROP_ID_MONITORING_ENABLED "monitoringEnabled"
#define PROP_ID_CURRENT_MODE      "currentMode"
#define PROP_ID_PERSON_PRESENT    "personPresent"
#define PROP_ID_AMBIENT_LUX       "ambientLux"
#define PROP_ID_FILL_LIGHT_ON     "fillLightOn"
#define PROP_ID_ALERT_MODE_MASK   "alertModeMask"
#define PROP_ID_COOLDOWN_MS       "cooldownMs"
#define PROP_ID_TIMER_DURATION_SEC "timerDurationSec"
#define PROP_ID_TIMER_RUNNING     "timerRunning"
#define PROP_ID_CFG_VERSION       "cfgVersion"
#define PROP_ID_SELF_TEST         "selfTest"

// Legacy property kept temporarily for old consumers during migration.
#define PROP_ID_IS_POSTURE        "isPosture"

// ============================================================
// System Modes
// ============================================================
#define MODE_POSTURE              0
#define MODE_CLOCK                1
#define MODE_TIMER                2

// ============================================================
// Alert Mode Bitmask
// ============================================================
#define ALERT_MODE_LED            (1 << 0)  // 0x01
#define ALERT_MODE_BUZZER         (1 << 1)  // 0x02
#define ALERT_MODE_VOICE          (1 << 2)  // 0x04
#define ALERT_MODE_ALL            0x07

// ============================================================
// Timer States
// ============================================================
#define TIMER_IDLE                0
#define TIMER_RUNNING             1
#define TIMER_PAUSED              2
#define TIMER_DONE                3

// ============================================================
// Default Timing Constants
// ============================================================
#define DEFAULT_PUBLISH_INTERVAL_MS     10000
#define DEFAULT_K230_TIMEOUT_MS         5000
#define DEFAULT_ABNORMAL_THRESHOLD      15
#define DEFAULT_ALERT_COOLDOWN_MS       5000
#define DEFAULT_TIMER_DURATION_SEC      1500

#endif // SHARED_PROTOCOL_CONSTANTS_H
