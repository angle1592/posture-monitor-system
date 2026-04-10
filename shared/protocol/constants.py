"""
Shared protocol constants for K230 Vision module.

This file is the SINGLE SOURCE OF TRUTH for protocol field names,
enum values, and UART configuration shared across the system.
Keep in sync with constants.h (ESP32) and constants.ts (App).
"""

# === UART Configuration (K230 -> ESP32) ===
UART_BAUDRATE = 115200
UART_TX_PIN = 3
UART_RX_PIN = 4

# === K230 UART JSON Field Names ===
# Active contract: the refactored K230 sends the minimal posture frame.
FIELD_POSTURE_TYPE = "posture_type"

# === Posture Type Enum Values ===
POSTURE_NORMAL = "normal"
POSTURE_HEAD_DOWN = "head_down"
POSTURE_HUNCHBACK = "hunchback"
POSTURE_NO_PERSON = "no_person"
POSTURE_UNKNOWN = "unknown"

ALL_POSTURE_TYPES = (
    POSTURE_NORMAL,
    POSTURE_HEAD_DOWN,
    POSTURE_HUNCHBACK,
    POSTURE_NO_PERSON,
    POSTURE_UNKNOWN,
)

# === Fused Posture Type Codes (ESP32 -> OneNET) ===
POSTURE_CODE_NO_PERSON = 0
POSTURE_CODE_NORMAL = 1
POSTURE_CODE_HEAD_DOWN = 2
POSTURE_CODE_HUNCHBACK = 3

# === System Modes ===
MODE_POSTURE = 0
MODE_CLOCK = 1
MODE_TIMER = 2

PROP_ID_POSTURE_TYPE = "postureType"
PROP_ID_MONITORING_ENABLED = "monitoringEnabled"
PROP_ID_CURRENT_MODE = "currentMode"
PROP_ID_PERSON_PRESENT = "personPresent"
PROP_ID_AMBIENT_LUX = "ambientLux"
PROP_ID_FILL_LIGHT_ON = "fillLightOn"
PROP_ID_ALERT_MODE_MASK = "alertModeMask"
PROP_ID_COOLDOWN_MS = "cooldownMs"
PROP_ID_TIMER_DURATION_SEC = "timerDurationSec"
PROP_ID_TIMER_RUNNING = "timerRunning"
PROP_ID_CFG_VERSION = "cfgVersion"
PROP_ID_SELF_TEST = "selfTest"

# Legacy property kept temporarily for old consumers during migration.
PROP_ID_IS_POSTURE = "isPosture"

# === Alert Mode Bitmask ===
ALERT_MODE_LED = 1 << 0  # 0x01
ALERT_MODE_BUZZER = 1 << 1  # 0x02
ALERT_MODE_VOICE = 1 << 2  # 0x04
ALERT_MODE_ALL = 0x07
