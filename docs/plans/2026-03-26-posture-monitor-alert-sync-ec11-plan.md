# Posture Monitor Alert/Sync/EC11 Fix Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Reduce alert cooldown to 5 seconds, add stronger EC11 key diagnostics, make local state changes publish immediately, and clean up local/cloud property synchronization semantics.

**Architecture:** Keep the existing module split (`timer_fsm`, `mode_manager`, `posture_monitor.ino`, `mqtt_handler`) and fix each issue at the layer where it originates. Treat EC11 first as an observability problem rather than assuming the state machine is wrong; treat local/cloud mismatch as a synchronization-policy bug plus property-semantics cleanup, not an MQTT transport rewrite.

**Tech Stack:** ESP32-S3 Arduino firmware, OneNET MQTT property model, EC11 rotary encoder input, K230 posture data consumer.

---

### Task 1: Reduce abnormal alert cooldown default to 5 seconds

**Files:**
- Modify: `refactored/posture_monitor/config.h`
- Review: `refactored/posture_monitor/timer_fsm.cpp`
- Review: `refactored/posture_monitor/docs/onenet-property-model.md`

**Step 1: Write the failing acceptance check**

Define the intended behavior:
- default abnormal reminder cooldown is 5000ms
- runtime config still clamps through existing bounds
- no change to the “recover to IDLE immediately when abnormal condition clears” logic

**Step 2: Run the red check**

Inspect current code and confirm it fails this requirement:
- `ALERT_COOLDOWN_MS` is currently `30000`

Expected: mismatch confirmed.

**Step 3: Write minimal implementation**

Change only the default cooldown constant in `config.h` to `5000`.

**Step 4: Run verification**

Re-read `config.h` and `timer_fsm.cpp` to confirm:
- default is 5000ms
- `timer_setCooldownMs()` clamp logic is unchanged
- alert policy behavior is still edge-triggered with cooldown

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Add stronger EC11 KEY diagnostics

**Files:**
- Modify: `refactored/posture_monitor/mode_manager.h`
- Modify: `refactored/posture_monitor/mode_manager.cpp`
- Modify: `refactored/posture_monitor/posture_monitor.ino`
- Review: `refactored/posture_monitor/docs/hardware-wiring.md`

**Step 1: Write the failing acceptance check**

Define the expected observability:
- firmware can report raw key level and debounced button state
- serial diagnostics can distinguish “pin never changed” vs “press detected but no long-click produced”

**Step 2: Run the red check**

Confirm current implementation is insufficient:
- `test ec11 watch` only logs pin level transitions
- there is no direct introspection for `_btnRawPressed`, `_btnStablePressed`, or pending click state

Expected: insufficient visibility confirmed.

**Step 3: Write minimal implementation**

Add a narrow diagnostic API from `mode_manager` exposing read-only button state, for example:
- current raw key level
- raw/stable pressed flags
- pending click event snapshot or last generated click
- current configured key pin if helpful for logs

Add/extend a serial test command in `posture_monitor.ino` to print these values continuously for a short window.

Do not change the long-press state machine yet unless diagnostics prove logic is wrong.

**Step 4: Run verification**

Verify by code inspection and serial-command path review that:
- diagnostics read the same `EC11_KEY_PIN` used by runtime input logic
- no existing mode behavior changed just by adding diagnostics
- logs clearly differentiate raw press, stable press, and long-click generation

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Publish local state changes immediately

**Files:**
- Modify: `refactored/posture_monitor/posture_monitor.ino`
- Review: `refactored/posture_monitor/mqtt_handler.h`
- Review: `refactored/posture_monitor/mqtt_handler.cpp`
- Review: `refactored/shared/protocol/constants.h`

**Step 1: Write the failing acceptance check**

Define the expected behavior:
- local EC11-driven mode changes should be pushed to cloud immediately, not only on the 10-second periodic publish
- local changes to alert mode, cooldown, timer duration, timer running state, or monitoring switch should also be eligible for immediate publish where applicable

**Step 2: Run the red check**

Confirm current implementation only periodically publishes via `handlePeriodicPublish()` and does not trigger an immediate publish after local user actions.

Expected: mismatch confirmed.

**Step 3: Write minimal implementation**

Add a small helper in `posture_monitor.ino` to publish the current property snapshot immediately after local state-changing actions, reusing existing publish/report functions instead of creating a parallel payload format.

Integrate this helper into the local action paths, especially:
- EC11 mode changes
- monitoring toggle
- alert mode changes
- timer start/pause/resume
- timer duration changes if locally adjusted

Keep MQTT-disconnected behavior safe: skip send or log, but do not block loop.

**Step 4: Run verification**

Verify by code inspection that:
- the immediate publish path reuses the same property contract as periodic publishing
- local actions now request immediate sync
- periodic publishing still remains as fallback consistency

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Clean up property semantics and synchronization consistency

**Files:**
- Modify: `refactored/posture_monitor/posture_monitor.ino`
- Review: `refactored/posture_monitor/docs/onenet-property-model.md`
- Review: `refactored/shared/protocol/constants.h`

**Step 1: Write the failing acceptance check**

Define the consistency target:
- `monitoringEnabled` and legacy `isPosture` compatibility should not create contradictory semantics
- `property/get` and periodic/property-post snapshots should not disagree for the same conceptual state unless explicitly documented
- `cooldownMs` and `timerDurationSec` should map cleanly to local runtime setters

**Step 2: Run the red check**

Inspect current code and confirm the two asymmetries already found:
- legacy `isPosture` compatibility overlaps with `monitoringEnabled`
- `handlePropertyGet()` and periodic reporting can derive posture-related state from different logic paths

Expected: semantic mismatch confirmed.

**Step 3: Write minimal implementation**

Tighten semantics with the smallest diff:
- keep legacy field compatibility only as fallback input, not as an alternative live semantic source
- align property-get payload generation with the same canonical state used for periodic reports where reasonable
- avoid changing unrelated app contract fields unless necessary

**Step 4: Run verification**

Verify by code inspection that:
- canonical fields drive both get/report paths consistently
- legacy compatibility remains but does not override current semantics unexpectedly
- cloud-side cooldown/timer writes still call the existing setter functions

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: Run verification on changed firmware files

**Files:**
- Verify: `refactored/posture_monitor/config.h`
- Verify: `refactored/posture_monitor/mode_manager.h`
- Verify: `refactored/posture_monitor/mode_manager.cpp`
- Verify: `refactored/posture_monitor/posture_monitor.ino`

**Step 1: Run diagnostics**

Run LSP diagnostics on all changed C/C++ files.

Expected: no new errors introduced by this work.

**Step 2: Run compile verification**

Run the verified compile command:
```bash
"E:\software\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "C:\Users\Tanfu\Desktop\2026\software\refactored\posture_monitor"
```

Expected: compile succeeds.

**Step 3: Manual hardware validation checklist**

After flashing, verify in order:
1. `test ec11 watch` / enhanced EC11 diagnostic command shows whether raw key level changes
2. long press in `MODE_TIMER` either enters adjust mode or logs enough state to explain why not
3. abnormal reminder repeats roughly every 5 seconds during continuous bad posture
4. local mode changes appear on upper machine without waiting for the full periodic window
5. cloud-side cooldown/timer changes produce visible local runtime effect consistent with the chosen semantics

**Step 4: Record residual issues**

If EC11 raw key never changes even with stronger diagnostics, explicitly record that as a hardware-side blocker rather than masking it as firmware success.

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

## Verification Checklist

- [ ] Default alert cooldown is 5000ms
- [ ] EC11 diagnostics expose raw and debounced key state clearly
- [ ] Local mode/config changes trigger immediate publish
- [ ] Property semantics are consistent across get/report/set paths
- [ ] LSP diagnostics show no new errors on changed files
- [ ] Firmware compile succeeds
- [ ] Manual hardware validation path is documented

## Notes for Implementation

- Keep loop non-blocking
- Do not use ArduinoJson
- Keep legacy property compatibility as narrow as possible
- Treat EC11 root cause honestly: diagnostics first, logic change only with evidence
- Prefer the smallest diff that fixes synchronization behavior without redesigning MQTT flow
