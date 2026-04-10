# ESP32 Fusion Posture Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the ESP32 firmware to a single PIR-gated fused posture state, report the new `postureType/personPresent/fillLightOn` properties to OneNET, polish the OLED posture page, and drive a GPIO18 fill light from ambient lux.

**Architecture:** Keep `k230_parser` focused on UART parsing, but stop letting downstream modules consume raw K230 semantics directly. Add one fused posture evaluation path in `posture_monitor.ino`, then route MQTT payload construction, alert policy, WS2812 state, OLED rendering, and fill-light control through that fused result. Update `shared/protocol` and `posture_monitor/config.h` together so the cloud contract stays aligned across C/C++, TypeScript, Python, and schema definitions.

**Tech Stack:** ESP32-S3 Arduino C++, OneNET MQTT, U8g2 OLED rendering, BH1750 ambient light sensor, HC-SR501 PIR sensor, shared protocol constants in C/TypeScript/Python/JSON schema.

---

## File Map

- Modify: `refactored/shared/protocol/constants.h`
  Purpose: define the new MQTT property IDs and fused posture enum codes for C/C++ consumers.
- Modify: `refactored/shared/protocol/constants.ts`
  Purpose: define the same posture enum codes and property IDs for the App.
- Modify: `refactored/shared/protocol/constants.py`
  Purpose: keep Python-side protocol constants synchronized.
- Modify: `refactored/shared/protocol/schemas.json`
  Purpose: update the documented UART/MQTT contract to the new posture and fill-light properties.
- Modify: `refactored/posture_monitor/config.h`
  Purpose: sync local property IDs and add fill-light GPIO / lux threshold / enable macros.
- Modify: `refactored/posture_monitor/posture_monitor.ino`
  Purpose: introduce `FusedPostureState`, evaluate PIR-gated posture, and publish the new property payloads.
- Modify: `refactored/posture_monitor/timer_fsm.h`
  Purpose: change the alert-policy interface so it consumes fused booleans instead of raw `K230Data`.
- Modify: `refactored/posture_monitor/timer_fsm.cpp`
  Purpose: trigger reminders only from fused abnormal posture.
- Modify: `refactored/posture_monitor/alerts.h`
  Purpose: narrow the indicator API to `mqttConnected/personPresent/shouldAlert` semantics.
- Modify: `refactored/posture_monitor/alerts.cpp`
  Purpose: map WS2812 state from fused presence + fused posture alert state.
- Modify: `refactored/posture_monitor/display.h`
  Purpose: expose a clearer posture-page view model setter.
- Modify: `refactored/posture_monitor/display.cpp`
  Purpose: redesign the posture page to show WiFi/MQTT, a large posture label, PIR, lux, and fill-light state.
- Modify: `refactored/posture_monitor/sensors.h`
  Purpose: declare fill-light control accessors.
- Modify: `refactored/posture_monitor/sensors.cpp`
  Purpose: own GPIO18 fill-light output state and expose `on/off/query` helpers.

---

### Task 1: Lock the new protocol contract

**Files:**
- Modify: `refactored/shared/protocol/constants.h`
- Modify: `refactored/shared/protocol/constants.ts`
- Modify: `refactored/shared/protocol/constants.py`
- Modify: `refactored/shared/protocol/schemas.json`
- Modify: `refactored/posture_monitor/config.h`

- [ ] **Step 1: Write the failing contract check**

Define the target protocol surface before editing code. The shared contract should include a fused posture enum and the new MQTT property IDs:

```c
#define PROP_ID_POSTURE_TYPE      "postureType"
#define PROP_ID_PERSON_PRESENT    "personPresent"
#define PROP_ID_FILL_LIGHT_ON     "fillLightOn"

#define POSTURE_CODE_NO_PERSON    0
#define POSTURE_CODE_NORMAL       1
#define POSTURE_CODE_HEAD_DOWN    2
#define POSTURE_CODE_HUNCHBACK    3
```

Schema target for MQTT properties:

```json
"postureType": {
  "type": "integer",
  "enum": [0, 1, 2, 3],
  "description": "0=no_person, 1=normal, 2=head_down, 3=hunchback"
},
"personPresent": { "type": "boolean" },
"fillLightOn": { "type": "boolean" }
```

Manual red check: current files still center `isPosture`, and `schemas.json` still documents `tilt`, `bad`, and the old UART frame shape.

- [ ] **Step 2: Run the red check**

Inspect these files and confirm all of the following are still true before editing:

- `refactored/shared/protocol/constants.h` still defines `POSTURE_TILT` and `POSTURE_BAD`
- `refactored/shared/protocol/constants.ts` still defines `TILT` and `BAD`
- `refactored/shared/protocol/constants.py` still defines `POSTURE_TILT` and `POSTURE_BAD`
- `refactored/shared/protocol/schemas.json` still documents the old UART frame fields and omits `fillLightOn`
- `refactored/posture_monitor/config.h` still defines `PROP_ID_IS_POSTURE` and does not define `PROP_ID_POSTURE_TYPE` / `PROP_ID_FILL_LIGHT_ON`

Expected: FAIL, because the protocol has not been updated yet.

- [ ] **Step 3: Write minimal implementation**

Update the shared constants first. In `constants.h`, replace the old posture strings with a fused MQTT enum section and keep the existing text posture constants only where UART parsing still needs them:

```c
#define POSTURE_NORMAL            "normal"
#define POSTURE_HEAD_DOWN         "head_down"
#define POSTURE_HUNCHBACK         "hunchback"
#define POSTURE_NO_PERSON         "no_person"
#define POSTURE_UNKNOWN           "unknown"

#define POSTURE_CODE_NO_PERSON    0
#define POSTURE_CODE_NORMAL       1
#define POSTURE_CODE_HEAD_DOWN    2
#define POSTURE_CODE_HUNCHBACK    3

#define PROP_ID_POSTURE_TYPE      "postureType"
#define PROP_ID_PERSON_PRESENT    "personPresent"
#define PROP_ID_AMBIENT_LUX       "ambientLux"
#define PROP_ID_FILL_LIGHT_ON     "fillLightOn"
```

Mirror the same meaning in `constants.ts`:

```ts
export const POSTURE_TYPE_CODES = {
  NO_PERSON: 0,
  NORMAL: 1,
  HEAD_DOWN: 2,
  HUNCHBACK: 3,
} as const

export type PostureTypeCode =
  (typeof POSTURE_TYPE_CODES)[keyof typeof POSTURE_TYPE_CODES]

export const PROP_IDS = {
  POSTURE_TYPE: 'postureType',
  PERSON_PRESENT: 'personPresent',
  AMBIENT_LUX: 'ambientLux',
  FILL_LIGHT_ON: 'fillLightOn',
  MONITORING_ENABLED: 'monitoringEnabled',
  CURRENT_MODE: 'currentMode',
  ALERT_MODE_MASK: 'alertModeMask',
  COOLDOWN_MS: 'cooldownMs',
  TIMER_DURATION_SEC: 'timerDurationSec',
  TIMER_RUNNING: 'timerRunning',
  CFG_VERSION: 'cfgVersion',
  SELF_TEST: 'selfTest',
} as const
```

Mirror the same meaning in `constants.py`:

```python
POSTURE_CODE_NO_PERSON = 0
POSTURE_CODE_NORMAL = 1
POSTURE_CODE_HEAD_DOWN = 2
POSTURE_CODE_HUNCHBACK = 3

PROP_ID_POSTURE_TYPE = "postureType"
PROP_ID_PERSON_PRESENT = "personPresent"
PROP_ID_AMBIENT_LUX = "ambientLux"
PROP_ID_FILL_LIGHT_ON = "fillLightOn"
```

Update `schemas.json` so the MQTT side documents the new fields, and trim `PostureType` to the active labels:

```json
"PostureType": {
  "type": "string",
  "enum": ["normal", "head_down", "hunchback", "no_person", "unknown"],
  "description": "Posture classification labels shared between K230 and ESP32"
},
"MqttPropertyPost": {
  "type": "object",
  "properties": {
    "postureType": { "type": "integer", "enum": [0, 1, 2, 3] },
    "personPresent": { "type": "boolean" },
    "ambientLux": { "type": "integer" },
    "fillLightOn": { "type": "boolean" },
    "monitoringEnabled": { "type": "boolean" },
    "currentMode": { "$ref": "#/definitions/SystemMode" },
    "alertModeMask": { "$ref": "#/definitions/AlertModeMask" },
    "cooldownMs": { "type": "integer" },
    "timerDurationSec": { "type": "integer" },
    "timerRunning": { "type": "boolean" },
    "cfgVersion": { "type": "integer" },
    "selfTest": { "type": "boolean" }
  }
}
```

Sync the local firmware macros in `config.h`:

```c
#define ENABLE_FILL_LIGHT         1
#define FILL_LIGHT_PIN            18
#define FILL_LIGHT_LUX_THRESHOLD  80

#define PROP_ID_POSTURE_TYPE      "postureType"
#define PROP_ID_PERSON_PRESENT    "personPresent"
#define PROP_ID_AMBIENT_LUX       "ambientLux"
#define PROP_ID_FILL_LIGHT_ON     "fillLightOn"
```

Delete `PROP_ID_IS_POSTURE` from the active property list in `config.h`.

- [ ] **Step 4: Run verification**

Re-read the five edited files and confirm:

- `postureType` and `fillLightOn` exist in all shared constants files
- posture code values are exactly `0/1/2/3` in every language
- `schemas.json` no longer advertises `tilt` or `bad` in the active posture enum
- `config.h` defines `GPIO18` and a concrete lux threshold for the fill light

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Introduce the fused posture model in `posture_monitor.ino`

**Files:**
- Modify: `refactored/posture_monitor/posture_monitor.ino`

- [ ] **Step 1: Write the failing acceptance check**

Define the target fused state API before editing the main sketch:

```c
typedef enum {
    FUSED_POSTURE_NO_PERSON = 0,
    FUSED_POSTURE_NORMAL = 1,
    FUSED_POSTURE_HEAD_DOWN = 2,
    FUSED_POSTURE_HUNCHBACK = 3,
} FusedPostureType;

typedef struct {
    FusedPostureType postureType;
    bool personPresent;
    bool postureKnown;
    bool shouldAlert;
    bool fillLightOn;
    bool pirPresent;
    bool pirReady;
    int ambientLux;
    bool k230TimedOut;
    bool k230DataValid;
} FusedPostureState;
```

Target helper behavior:

```c
FusedPostureState state = evaluateFusedPostureState(k230_getData(), millis());
// PIR=有人 + K230=normal -> NORMAL
// PIR=有人 + K230=head_down -> HEAD_DOWN
// PIR=有人 + K230=hunchback -> HUNCHBACK
// otherwise -> NO_PERSON
```

Manual red check: current `posture_monitor.ino` still computes `noPerson` inline and has no fused posture struct or helper.

- [ ] **Step 2: Run the red check**

Inspect `posture_monitor.ino` and confirm all of the following are still true:

- there is no `FusedPostureType` enum
- there is no `FusedPostureState` struct
- `noPerson` is still computed ad hoc in multiple places
- `handlePeriodicPublish()`, `alerts_update()`, and `display_update()` are still fed from raw `K230Data`

Expected: FAIL, because the main sketch has not centralized the posture decision yet.

- [ ] **Step 3: Write minimal implementation**

Add the new enum/struct near the existing `PublishDecision` types:

```c
enum FusedPostureType {
    FUSED_POSTURE_NO_PERSON = 0,
    FUSED_POSTURE_NORMAL = 1,
    FUSED_POSTURE_HEAD_DOWN = 2,
    FUSED_POSTURE_HUNCHBACK = 3,
};

typedef struct {
    FusedPostureType postureType;
    bool personPresent;
    bool postureKnown;
    bool shouldAlert;
    bool fillLightOn;
    bool pirPresent;
    bool pirReady;
    int ambientLux;
    bool k230TimedOut;
    bool k230DataValid;
} FusedPostureState;
```

Add prototypes:

```c
FusedPostureState evaluateFusedPostureState(const K230Data* data, unsigned long now);
const char* fusedPostureLabel(FusedPostureType postureType);
int fusedPostureCode(FusedPostureType postureType);
```

Implement the fused evaluation in one place:

```c
FusedPostureState evaluateFusedPostureState(const K230Data* data, unsigned long now) {
    FusedPostureState state = {
        FUSED_POSTURE_NO_PERSON,
        false,
        false,
        false,
        false,
        sensors_isPersonPresent(),
        sensors_isPirReady(),
        sensors_getLightLevel(),
        k230_isTimeout(K230_TIMEOUT_MS),
        data != NULL ? data->valid : false,
    };

    bool pirAllowsPerson = state.pirPresent;
    const char* posture = (data != NULL) ? data->postureType : NULL;
    bool k230AcceptsPosture = posture != NULL && (
        strcmp(posture, "normal") == 0 ||
        strcmp(posture, "head_down") == 0 ||
        strcmp(posture, "hunchback") == 0
    );

    if (pirAllowsPerson && !state.k230TimedOut && state.k230DataValid && k230AcceptsPosture) {
        state.personPresent = true;
        state.postureKnown = true;
        if (strcmp(posture, "normal") == 0) {
            state.postureType = FUSED_POSTURE_NORMAL;
        } else if (strcmp(posture, "head_down") == 0) {
            state.postureType = FUSED_POSTURE_HEAD_DOWN;
            state.shouldAlert = true;
        } else {
            state.postureType = FUSED_POSTURE_HUNCHBACK;
            state.shouldAlert = true;
        }
    }

    state.fillLightOn = state.pirPresent && state.ambientLux < FILL_LIGHT_LUX_THRESHOLD;
    return state;
}
```

Use `fusedPostureLabel()` for the OLED-facing Chinese strings:

```c
const char* fusedPostureLabel(FusedPostureType postureType) {
    switch (postureType) {
        case FUSED_POSTURE_NORMAL: return "正常";
        case FUSED_POSTURE_HEAD_DOWN: return "低头";
        case FUSED_POSTURE_HUNCHBACK: return "驼背";
        default: return "无人";
    }
}
```

- [ ] **Step 4: Run verification**

Re-read `posture_monitor.ino` and confirm:

- the fused rule is expressed once in `evaluateFusedPostureState()`
- `PIR=有人 + K230 in {normal, head_down, hunchback}` is the only path that yields a posture other than `NO_PERSON`
- `fillLightOn` is derived from `pirPresent` and `ambientLux`
- the label function returns `无人/正常/低头/驼背`

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Rewire MQTT payloads and property replies to the fused state

**Files:**
- Modify: `refactored/posture_monitor/posture_monitor.ino`

- [ ] **Step 1: Write the failing acceptance check**

Define the target payload shape before editing the existing publish helpers:

```json
{
  "postureType": { "value": 2 },
  "personPresent": { "value": true },
  "ambientLux": { "value": 44 },
  "fillLightOn": { "value": true },
  "monitoringEnabled": { "value": true },
  "currentMode": { "value": 0 },
  "timerRunning": { "value": false }
}
```

Manual red check: `buildPropertyPayloads()` and `handlePropertyGet()` still serialize `isPosture` and do not serialize `postureType` or `fillLightOn`.

- [ ] **Step 2: Run the red check**

Inspect `handlePropertyGet()`, `evaluatePeriodicPublishDecision()`, and `buildPropertyPayloads()` and confirm all of the following are still true:

- `PublishDecision` still stores `isPostureOk`
- `handlePropertyGet()` still prints `PROP_ID_IS_POSTURE`
- `buildPropertyPayloads()` still builds `isPosture` and omits `fillLightOn`
- logs still say `isPosture=true/false`

Expected: FAIL, because the MQTT layer still uses the old boolean contract.

- [ ] **Step 3: Write minimal implementation**

Redefine `PublishDecision` so it carries the fused posture result instead of a boolean:

```c
typedef struct {
    FusedPostureState fused;
    PublishDecisionReason reason;
    unsigned long dataAgeMs;
} PublishDecision;
```

Update `evaluatePeriodicPublishDecision()` to start from the fused state and only preserve the existing startup/stale reasoning for logging:

```c
PublishDecision evaluatePeriodicPublishDecision(const K230Data* data, unsigned long now) {
    PublishDecision decision = { evaluateFusedPostureState(data, now), DECISION_LIVE_FRAME, 0 };

    if (!monitoringEnabled) {
        decision.reason = DECISION_MONITOR_OFF;
        decision.fused.postureType = FUSED_POSTURE_NO_PERSON;
        decision.fused.personPresent = false;
        decision.fused.postureKnown = false;
        decision.fused.shouldAlert = false;
        return decision;
    }

    if (k230_getFrameCount() == 0) {
        decision.reason = DECISION_STARTUP_WAIT;
        decision.fused.postureType = FUSED_POSTURE_NO_PERSON;
        decision.fused.personPresent = false;
        decision.fused.postureKnown = false;
        decision.fused.shouldAlert = false;
        return decision;
    }

    if (!data->valid || k230_isTimeout(K230_TIMEOUT_MS)) {
        decision.reason = DECISION_FALLBACK_TRUE;
        decision.fused.postureType = FUSED_POSTURE_NO_PERSON;
        decision.fused.personPresent = false;
        decision.fused.postureKnown = false;
        decision.fused.shouldAlert = false;
        return decision;
    }

    return decision;
}
```

Update `buildPropertyPayloads()` to serialize the new properties:

```c
snprintf(
    statusParams,
    statusLen,
    "{"
    "\"%s\":{\"value\":%d},"
    "\"%s\":{\"value\":%s},"
    "\"%s\":{\"value\":%d},"
    "\"%s\":{\"value\":%s},"
    "\"%s\":{\"value\":%s},"
    "\"%s\":{\"value\":%u}"
    "}",
    PROP_ID_POSTURE_TYPE, fusedPostureCode(decision.fused.postureType),
    PROP_ID_PERSON_PRESENT, decision.fused.personPresent ? "true" : "false",
    PROP_ID_AMBIENT_LUX, decision.fused.ambientLux,
    PROP_ID_FILL_LIGHT_ON, decision.fused.fillLightOn ? "true" : "false",
    PROP_ID_MONITORING_ENABLED, monitoringEnabled ? "true" : "false",
    PROP_ID_CURRENT_MODE, (unsigned int)mode_getCurrent()
);
```

Use the same fields in `handlePropertyGet()`.

Update log strings so they mention `postureType` and `personPresent`, for example:

```c
LOGI("[定时上报][%s] postureType=%d personPresent=%s fillLightOn=%s",
     publishDecisionReasonText(decision.reason),
     fusedPostureCode(decision.fused.postureType),
     decision.fused.personPresent ? "true" : "false",
     decision.fused.fillLightOn ? "true" : "false");
```

- [ ] **Step 4: Run verification**

Run the firmware build:

`"E:\software\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "C:\Users\Tanfu\Desktop\2026\software\refactored\posture_monitor"`

Expected: compile succeeds.

Then re-read the payload helpers and confirm:

- `PROP_ID_POSTURE_TYPE` is serialized in both periodic publish and property/get reply
- `PROP_ID_FILL_LIGHT_ON` is serialized in both periodic publish and property/get reply
- `PROP_ID_IS_POSTURE` is no longer serialized from the main path

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Route reminders, indicator lights, OLED, and fill light through the fused state

**Files:**
- Modify: `refactored/posture_monitor/timer_fsm.h`
- Modify: `refactored/posture_monitor/timer_fsm.cpp`
- Modify: `refactored/posture_monitor/alerts.h`
- Modify: `refactored/posture_monitor/alerts.cpp`
- Modify: `refactored/posture_monitor/display.h`
- Modify: `refactored/posture_monitor/display.cpp`
- Modify: `refactored/posture_monitor/sensors.h`
- Modify: `refactored/posture_monitor/sensors.cpp`
- Modify: `refactored/posture_monitor/posture_monitor.ino`

- [ ] **Step 1: Write the failing acceptance check**

Define the target control interfaces before editing the modules:

```c
void timer_alertPolicyTick(unsigned long now, bool monitoringEnabled, bool shouldAlert,
                           bool testForcePosture, bool testForceAbnormal);

void alerts_update(bool mqttConnected, bool personPresent, bool shouldAlert);

void display_setPostureState(const char* postureLabel, bool fillLightOn);

void sensors_setFillLight(bool on);
bool sensors_isFillLightOn();
```

Manual red check: the current code still passes raw `K230Data` into `timer_alertPolicyTick()`, still passes `k230Valid/isAbnormal/noPerson` into `alerts_update()`, the display page still renders `Posture:%s` and `State:ok`, and there is no fill-light GPIO helper in `sensors.cpp`.

- [ ] **Step 2: Run the red check**

Inspect the listed files and confirm:

- `timer_alertPolicyTick()` still takes `const K230Data* data`
- `alerts_update()` still takes `k230Valid/isAbnormal/noPerson`
- `display_update()` still prints `State:%s Lx:%d`
- `sensors.cpp` has no `_fillLightOn` state or GPIO18 writes

Expected: FAIL, because the downstream modules still consume pre-fusion semantics.

- [ ] **Step 3: Write minimal implementation**

Update the timer FSM interface and implementation:

```c
void timer_alertPolicyTick(unsigned long now, bool monitoringEnabled, bool shouldAlert,
                           bool testForcePosture, bool testForceAbnormal) {
    bool abnormalNow = monitoringEnabled && shouldAlert;
    if (testForcePosture) {
        abnormalNow = testForceAbnormal;
    }
    if (!abnormalNow) {
        _alertPolicyState = ALERT_POLICY_IDLE;
        return;
    }
    if (now < _alertCooldownUntilMs) {
        _alertPolicyState = ALERT_POLICY_COOLDOWN;
        return;
    }
    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS);
    if (alerts_voiceEnabled()) {
        voice_speak("请调整坐姿");
    }
    _alertCooldownUntilMs = now + _runtimeCfg.cooldownMs;
    _alertPolicyState = ALERT_POLICY_COOLDOWN;
}
```

Narrow the alerts API:

```c
void alerts_update(bool mqttConnected, bool personPresent, bool shouldAlert) {
    if (!mqttConnected) {
        _alertsSetIndicatorColor(64, 0, 0);
        return;
    }
    if (!personPresent) {
        _alertsSetIndicatorColor(0, 0, 0);
        return;
    }
    if (shouldAlert) {
        unsigned long now = millis();
        if (now - _alertLastBlink >= 300) {
            _alertLastBlink = now;
            _alertBlinkState = !_alertBlinkState;
            if (_alertBlinkState) {
                _alertsSetIndicatorColor(64, 0, 0);
            } else {
                _alertsSetIndicatorColor(0, 0, 0);
            }
        }
        return;
    }
    _alertsSetIndicatorColor(0, 32, 0);
}
```

Add fill-light control to `sensors.h/.cpp`:

```c
void sensors_setFillLight(bool on);
bool sensors_isFillLightOn();
```

```c
static bool _fillLightOn = false;

void sensors_init() {
#if ENABLE_PIR
    LOGI("PIR 传感器初始化: GPIO%d", PIR_PIN);
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    _pirBootMs = millis();
    _pirReady = false;
    _pirPresent = true;
#else
    LOGI("PIR 未启用（ENABLE_PIR=0），默认: 有人");
#endif

#if ENABLE_LIGHT_SENSOR
    LOGI("BH1750 初始化: SDA=GPIO%d SCL=GPIO%d addr=0x%02X", BH1750_SDA_PIN, BH1750_SCL_PIN, BH1750_I2C_ADDR);
    Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
    _lightReady = initBh1750();
    if (!_lightReady) {
        LOGW("BH1750 初始化失败，维持默认 lux=%d", _lightLevel);
    }
#else
    LOGI("光敏传感器未启用（ENABLE_LIGHT_SENSOR=0），默认: 2000");
#endif

#if ENABLE_FILL_LIGHT
    LOGI("补光 LED 初始化: GPIO%d", FILL_LIGHT_PIN);
    pinMode(FILL_LIGHT_PIN, OUTPUT);
    digitalWrite(FILL_LIGHT_PIN, LOW);
    _fillLightOn = false;
#endif
}

void sensors_setFillLight(bool on) {
#if ENABLE_FILL_LIGHT
    _fillLightOn = on;
    digitalWrite(FILL_LIGHT_PIN, on ? HIGH : LOW);
#else
    _fillLightOn = false;
    (void)on;
#endif
}

bool sensors_isFillLightOn() {
    return _fillLightOn;
}
```

Update the display interface to cache the clear posture page data:

```c
void display_setPostureState(const char* postureLabel, bool fillLightOn);
```

Implement the page layout in `display.cpp` around the existing posture branch:

```c
snprintf(line, sizeof(line), "WiFi:%s MQTT:%s", _displayWifiConnected ? "OK" : "--", _displayMqttConnected ? "OK" : "--");
_u8g2.drawStr(0, 10, line);
_u8g2.drawHLine(0, 14, 128);

_u8g2.setFont(u8g2_font_wqy14_t_gb2312);
_u8g2.drawUTF8(36, 40, postureLabel);

_u8g2.setFont(u8g2_font_6x10_tf);
snprintf(line, sizeof(line), "PIR: %s", _displayPersonPresent ? "有人" : "无人");
_u8g2.drawUTF8(0, 52, line);
snprintf(line, sizeof(line), "光照: %d lx", _displayLightLux);
_u8g2.drawUTF8(0, 60, line);
snprintf(line, sizeof(line), "补光: %s", _displayFillLightOn ? "开" : "关");
_u8g2.drawUTF8(68, 60, line);
```

Finally, update the main loop to drive everything from the fused state:

```c
const K230Data* data = k230_getData();
FusedPostureState fused = evaluateFusedPostureState(data, now);

sensors_setFillLight(fused.fillLightOn);
timer_alertPolicyTick(now, monitoringEnabled, fused.shouldAlert,
                      testForcePostureEnabled, testForceAbnormal);
alerts_update(mqttOk, fused.personPresent, fused.shouldAlert);

display_setConnectivity(WiFi.status() == WL_CONNECTED, mqttOk);
display_setSensorStatus(fused.pirReady, fused.pirPresent,
                        sensors_isLightSensorReady(), fused.ambientLux);
display_setPostureState(fusedPostureLabel(fused.postureType), fused.fillLightOn);
display_update(mode_getCurrent());
```

To support the last call, simplify `display_update` to `void display_update(int mode);` and let it render posture from cached state.

- [ ] **Step 4: Run verification**

Run the firmware build again:

`"E:\software\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "C:\Users\Tanfu\Desktop\2026\software\refactored\posture_monitor"`

Expected: compile succeeds.

Then verify the implementation reads cleanly:

- `timer_alertPolicyTick()` no longer depends on `K230Data`
- `alerts_update()` no longer mentions `k230Valid` or `noPerson`
- `display_update()` posture page no longer prints `State:ok` or `Lux:OK`
- `GPIO18` is driven only through `sensors_setFillLight()`

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: End-to-end verification and protocol sanity check

**Files:**
- Review: `refactored/posture_monitor/posture_monitor.ino`
- Review: `refactored/posture_monitor/display.cpp`
- Review: `refactored/posture_monitor/sensors.cpp`
- Review: `refactored/shared/protocol/constants.h`
- Review: `refactored/shared/protocol/schemas.json`

- [ ] **Step 1: Write the failing verification checklist**

Before the final build, define the exact manual checks the implementation must satisfy:

```text
1. PIR=有人 + K230=normal      -> postureType=1, personPresent=true, fillLightOn depends on lux
2. PIR=有人 + K230=head_down   -> postureType=2, shouldAlert=true
3. PIR=有人 + K230=hunchback   -> postureType=3, shouldAlert=true
4. PIR=有人 + K230=unknown     -> postureType=0, personPresent=false
5. PIR=无人 + 任意K230         -> postureType=0, personPresent=false
6. PIR=有人 + lux below threshold -> fillLightOn=true
7. PIR=无人 or lux recovered      -> fillLightOn=false
```

- [ ] **Step 2: Run the red check**

Compare the edited code against the checklist and look for any mismatch, especially:

- `unknown` accidentally propagated instead of collapsing to `no_person`
- fill-light control tied to K230 posture instead of PIR + lux
- OLED posture page still depending on raw `postureType` strings from K230

Expected: if any of these mismatches remain, fix them before claiming completion.

- [ ] **Step 3: Run the full build verification**

Run:

`"E:\software\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "C:\Users\Tanfu\Desktop\2026\software\refactored\posture_monitor"`

Expected: compile succeeds with no missing symbol errors from the changed interfaces.

- [ ] **Step 4: Run the hardware validation checklist**

After flashing, validate with the serial monitor and real sensors:

1. Watch serial logs while toggling PIR and confirm `postureType=0` when PIR is idle.
2. Present a normal seated posture and confirm OLED shows `正常`.
3. Present a head-down posture and confirm OLED shows `低头`, WS2812 blinks red, and reminder cooldown still works.
4. Cover the BH1750 or dim the environment and confirm the GPIO18 LED turns on only while PIR says `有人`.
5. Query OneNET property/get and confirm `postureType/personPresent/ambientLux/fillLightOn` all appear.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.
