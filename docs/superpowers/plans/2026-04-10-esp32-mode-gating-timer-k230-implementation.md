# ESP32 Mode Gating, K230 Control, and Timer Reminder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make posture/clock/timer run as mutually exclusive modes, add ESP32-to-K230 detection control, fix the OLED layout overlap, and upgrade timer completion into an acknowledged interval reminder.

**Architecture:** Keep the existing module split, but add one runtime gate in `posture_monitor.ino` that decides whether the posture pipeline is active. Drive K230 control from that gate, let `timer_fsm` own timer completion reminder state, and keep `display`/`alerts` as thin rendering and output layers.

**Tech Stack:** ESP32-S3 Arduino C++, CanMV MicroPython, UART JSON line protocol, U8g2 OLED rendering, existing alerts/timer/mode modules.

---

## File Map

- Modify: `posture_monitor/config.h`
  Purpose: add timer reminder interval constants and any display/timer tuning constants needed by the new flow.
- Modify: `posture_monitor/mode_manager.h`
  Purpose: expose a “mode switch blocked” flag for the timer done-pending state.
- Modify: `posture_monitor/mode_manager.cpp`
  Purpose: ignore rotation-based mode switching while switching is blocked.
- Modify: `posture_monitor/alerts.h`
  Purpose: expose a non-blocking LED pulse helper for timer completion reminders.
- Modify: `posture_monitor/alerts.cpp`
  Purpose: render timed indicator pulses ahead of normal posture status colors.
- Modify: `posture_monitor/timer_fsm.h`
  Purpose: extend timer state with done-pending acknowledgement helpers.
- Modify: `posture_monitor/timer_fsm.cpp`
  Purpose: implement repeated timer-done reminders, acknowledgement, and paused-state adjustment behavior.
- Modify: `posture_monitor/display.h`
  Purpose: pass timer adjust-mode information into OLED rendering.
- Modify: `posture_monitor/display.cpp`
  Purpose: remove WiFi/MQTT header text, avoid overlap, and present mode-specific titles/statuses.
- Modify: `posture_monitor/k230_parser.h`
  Purpose: declare the ESP32-to-K230 `set_detection` sender.
- Modify: `posture_monitor/k230_parser.cpp`
  Purpose: send line-based JSON control commands over `Serial1`.
- Modify: `posture_monitor/posture_monitor.ino`
  Purpose: gate the posture pipeline by mode, stop timer runtime when leaving timer mode, block mode switching during done-pending, and sync the K230 detection command.
- Modify: `k230/src/main.py`
  Purpose: receive UART commands from ESP32, toggle `detection_enabled`, and skip capture/inference/output while detection is disabled.

---

### Task 1: Define the new runtime states and outputs

**Files:**
- Modify: `posture_monitor/config.h`
- Modify: `posture_monitor/timer_fsm.h`
- Modify: `posture_monitor/alerts.h`
- Modify: `posture_monitor/display.h`
- Modify: `posture_monitor/k230_parser.h`

- [ ] **Step 1: Write the red checklist**

Confirm the current headers do not yet support the new behavior:

```text
- timer state only has IDLE/RUNNING/PAUSED/DONE
- no timer acknowledgement helper exists
- no mode-switch block helper exists
- no LED pulse helper exists
- display_setTimerStatus has no adjust-mode parameter
- no k230_setDetectionEnabled sender exists
```

- [ ] **Step 2: Run the red check**

Read the five headers and confirm the checklist is still true before editing.

Expected: RED, because the runtime interfaces do not yet expose the needed state transitions.

- [ ] **Step 3: Write the minimal header changes**

Add the missing declarations and constants, for example:

```c
enum TimerState {
    TIMER_IDLE = 0,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE_PENDING
};

bool timer_isDonePending();
void timer_ackDone();

void alerts_triggerIndicatorPulse(unsigned long durationMs, uint8_t r, uint8_t g, uint8_t b);

void display_setTimerStatus(int remainSec, int totalSec, int timerState, bool adjustMode);

bool k230_setDetectionEnabled(bool enabled);
```

- [ ] **Step 4: Re-read the headers**

Verify the new declarations are present and named consistently across callers and implementations.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Fix the timer state machine and timer-done reminder flow

**Files:**
- Modify: `posture_monitor/timer_fsm.cpp`
- Modify: `posture_monitor/alerts.cpp`

- [ ] **Step 1: Write the failing behavior checklist**

Capture the current broken behavior as the target red state:

```text
1. start from adjust mode does not exit adjust mode
2. paused timer duration changes do not update remaining time
3. timer completion only reminds once
4. timer completion does not require short-press acknowledgement
5. LED is not part of timer completion reminder output
```

- [ ] **Step 2: Run the red check**

Read the current implementation and confirm all five failures are real.

Expected: RED.

- [ ] **Step 3: Implement minimal timer state changes**

Update `timer_tick`, `timer_adjustDuration`, and helper functions so they behave like this:

```c
if (_timerState == TIMER_RUNNING && _timerEndMs <= now) {
    _timerRemainSec = 0;
    _timerState = TIMER_DONE_PENDING;
    _triggerTimerDoneReminder(now, true);
}

if (_timerState == TIMER_DONE_PENDING && now >= _timerNextReminderMs) {
    _triggerTimerDoneReminder(now, false);
}

if (_timerState == TIMER_PAUSED) {
    _timerRemainSec = _runtimeCfg.timerDurationSec;
}
```

Add a helper that reuses the existing alert-mode mask:

```c
static void _triggerTimerDoneReminder(unsigned long now, bool immediate) {
    if ((alerts_getAlertMode() & ALERT_MODE_LED) != 0) {
        alerts_triggerIndicatorPulse(TIMER_DONE_LED_PULSE_MS, 64, 0, 0);
    }
    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS * 2);
    if (alerts_voiceEnabled()) {
        voice_speak("时间到了，时间到了");
    }
    _timerNextReminderMs = now + TIMER_DONE_REMINDER_INTERVAL_MS;
}
```

- [ ] **Step 4: Implement acknowledgement helpers**

Make acknowledgement return the timer to a clean idle state:

```c
bool timer_isDonePending() {
    return _timerState == TIMER_DONE_PENDING;
}

void timer_ackDone() {
    if (_timerState != TIMER_DONE_PENDING) {
        return;
    }
    timer_reset();
}
```

- [ ] **Step 5: Verify by code review**

Re-read `timer_tick`, `timer_adjustDuration`, `timer_ackDone`, and the new alert pulse path.

Check that:

- paused-state adjustment updates `remainSec`
- completion transitions into `TIMER_DONE_PENDING`
- reminders repeat by interval until acknowledgement
- LED/BUZZER/VOICE all remain controlled by `alertModeMask`

- [ ] **Step 6: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Block mode switching during timer-done pending and stop timer work when leaving timer mode

**Files:**
- Modify: `posture_monitor/mode_manager.cpp`
- Modify: `posture_monitor/posture_monitor.ino`

- [ ] **Step 1: Write the failing behavior checklist**

The current system still allows these broken flows:

```text
1. rotation can still change mode while timer done reminder is pending
2. leaving timer mode does not explicitly stop timer runtime state
3. mode change side effects are split and implicit
```

- [ ] **Step 2: Run the red check**

Inspect `mode_manager.cpp` and `posture_monitor.ino` to confirm all three issues are still present.

Expected: RED.

- [ ] **Step 3: Implement the smallest mode-switch block**

In `mode_manager.cpp`, add a blocked flag and short-circuit the rotation path:

```c
if (delta != 0) {
    if (_rotationLocked) {
        _pendingDelta += delta;
    } else if (_modeSwitchBlocked) {
        _pendingDelta = 0;
    } else {
        // existing mode switching path
    }
}
```

- [ ] **Step 4: Centralize mode side effects in `posture_monitor.ino`**

Add a sync helper that runs every loop after mode/input handling:

```c
void syncModeRuntimeState() {
    int currentMode = (int)mode_getCurrent();
    mode_setSwitchBlocked(timer_isDonePending());

    if (currentMode != _runtimeAppliedMode) {
        if (currentMode != MODE_TIMER) {
            timer_setAdjustMode(false);
            timer_reset();
        }
        alerts_off();
        _runtimeAppliedMode = currentMode;
        requestImmediatePublish();
    }
}
```

- [ ] **Step 5: Verify by code review**

Confirm:

- timer-done pending blocks rotation-based mode switching
- leaving timer mode clears adjust mode and timer runtime state
- short press confirmation is still possible while blocked

- [ ] **Step 6: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Gate the posture pipeline and sync K230 detection state

**Files:**
- Modify: `posture_monitor/posture_monitor.ino`
- Modify: `posture_monitor/k230_parser.cpp`
- Modify: `k230/src/main.py`

- [ ] **Step 1: Write the failing behavior checklist**

Current behavior is still wrong in these ways:

```text
1. mode switch only changes UI, not K230 runtime behavior
2. non-posture modes still compute fillLightOn from PIR + lux
3. ESP32 cannot send a set_detection command to K230
4. K230 cannot receive or apply a stop/resume detection command
```

- [ ] **Step 2: Run the red check**

Inspect `posture_monitor.ino`, `k230_parser.cpp`, and `k230/src/main.py`.

Expected: RED.

- [ ] **Step 3: Implement ESP32 posture-pipeline gating**

Add one gate helper and use it at the top of fused posture evaluation:

```c
static bool isPosturePipelineActive() {
    return mode_getCurrent() == MODE_POSTURE && monitoringEnabled;
}

if (!isPosturePipelineActive()) {
    state.personPresent = false;
    state.postureKnown = false;
    state.shouldAlert = false;
    state.fillLightOn = false;
    return state;
}
```

- [ ] **Step 4: Implement ESP32-to-K230 command sending**

Use the existing `Serial1` link for line-based JSON:

```c
bool k230_setDetectionEnabled(bool enabled) {
    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"cmd\":\"set_detection\",\"enabled\":%s}\n",
             enabled ? "true" : "false");
    return Serial1.print(payload) > 0;
}
```

Then sync the desired state from `posture_monitor.ino` only when it changes.

- [ ] **Step 5: Implement K230 command reception and soft stop**

In `k230/src/main.py`, add a UART line reader and detection gate:

```python
self.detection_enabled = True
self._uart_rx_line = bytearray()

def poll_uart_commands(self):
    while self.uart and self.uart.any():
        ch = self.uart.read(1)
        if ch == b"\n":
            self._apply_uart_command(bytes(self._uart_rx_line))
            self._uart_rx_line = bytearray()
        elif ch not in (b"\r", None):
            self._uart_rx_line.extend(ch)
```

When `detection_enabled` is `False`, skip capture/inference/output and sleep briefly before the next poll.

- [ ] **Step 6: Verify by narrow checks**

Re-read the three changed files and confirm:

- non-posture modes force `fillLightOn=false`
- ESP32 only sends `set_detection` when the desired state changes
- K230 main loop keeps polling UART while detection is disabled
- K230 stops sending posture JSON while disabled

- [ ] **Step 7: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: Re-layout OLED pages and fix timer input semantics

**Files:**
- Modify: `posture_monitor/display.cpp`
- Modify: `posture_monitor/posture_monitor.ino`

- [ ] **Step 1: Write the failing behavior checklist**

```text
1. clock page overlaps the header line
2. timer page overlaps the header line
3. WiFi/MQTT occupies the most valuable header space
4. short-press start from timer adjust mode does not auto-exit adjust mode
```

- [ ] **Step 2: Run the red check**

Inspect `display.cpp` and the timer input handler in `posture_monitor.ino`.

Expected: RED.

- [ ] **Step 3: Re-layout the three OLED pages**

Use mode-specific titles and lower the big-number baselines:

```c
_u8g2.setFont(u8g2_font_6x10_tf);
_u8g2.drawUTF8(0, 10, "时钟");
_u8g2.drawHLine(0, 14, 128);
_u8g2.setFont(u8g2_font_logisoso24_tn);
_u8g2.drawStr(0, 46, hhmmss);
```

Apply the same pattern to timer mode and replace the posture page header with a title-oriented layout instead of WiFi/MQTT text.

- [ ] **Step 4: Fix short-press timer semantics**

Make timer-mode short press leave adjust mode before starting:

```c
if ((tState == TIMER_IDLE) && timer_isAdjustMode()) {
    timer_setAdjustMode(false);
    mode_setRotationLocked(false);
}
timer_start();
```

Handle timer completion acknowledgement before all other timer-mode clicks:

```c
if (timer_isDonePending()) {
    if (click == MODE_CLICK_SHORT) {
        timer_ackDone();
        display_showMessage("Timer cleared", NULL);
    }
    return;
}
```

- [ ] **Step 5: Verify by code review**

Check that:

- no page still draws WiFi/MQTT in the top row
- clock/timer big fonts sit below the decorative line
- start-from-adjust exits adjust mode automatically
- timer done pending only responds to short-press acknowledgement

- [ ] **Step 6: Commit**

Do not commit unless the user explicitly asks.

---

### Task 6: Run available verification

**Files:**
- No source edits required

- [ ] **Step 1: Syntax-check K230 Python changes**

Run:

```bash
py -3 -m py_compile "C:\Users\Tanfu\Desktop\2026\software\refactored\.worktrees\k230-posture-geometry\k230\src\main.py"
```

Expected: no output, exit code 0.

- [ ] **Step 2: Compile the ESP32 firmware in the active worktree**

Run:

```bash
"E:\software\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "C:\Users\Tanfu\Desktop\2026\software\refactored\.worktrees\k230-posture-geometry\posture_monitor"
```

Expected: compile succeeds.

- [ ] **Step 3: Record manual verification still required**

Document the board-level checks that cannot be proven locally:

- mode switch sends stop/resume correctly to K230
- clock/timer layout no longer overlaps on the physical OLED
- paused timer can be adjusted and resumed with the new remaining time
- timer-done interval reminder repeats until short-press confirmation

- [ ] **Step 4: Commit**

Do not commit unless the user explicitly asks.
