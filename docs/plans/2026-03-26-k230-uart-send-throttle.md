# K230 UART Send Throttle Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Reduce K230->ESP32 UART overflow by keeping continuous recognition, throttling UART sends, and removing `person_debug` / `posture_debug` from the default UART payload.

**Architecture:** Keep the K230 recognition loop unchanged so posture analysis freshness and `consecutive_abnormal` semantics remain intact. Add a send-rate gate in `refactored/k230/src/main.py` so the loop always computes the latest result but only emits a compact UART payload at a fixed interval, with an immediate send path reserved for future state-change logic if needed.

**Tech Stack:** MicroPython (CanMV/K230), ESP32-S3 Arduino UART consumer, lightweight string-based JSON parsing on ESP32.

---

### Task 1: Define the throttled UART contract in config

**Files:**
- Modify: `refactored/k230/src/config.py`
- Review: `refactored/posture_monitor/k230_parser.cpp`
- Review: `refactored/posture_monitor/config.h`

**Step 1: Write the failing test / acceptance check**

No verified host-side Python test framework exists in `refactored/k230`. Use a manual contract check instead: the UART payload must contain only fields consumed by ESP32 parser (`posture_type`, `is_abnormal`, `consecutive_abnormal`, `confidence`) plus the already harmless metadata kept for observability if explicitly chosen.

**Step 2: Run the red check**

Inspect current code and confirm it fails the intended contract:
- `refactored/k230/src/main.py` currently sends `person_debug` and `posture_debug`
- `refactored/posture_monitor/k230_parser.cpp` only parses four core fields

Expected: mismatch confirmed.

**Step 3: Write minimal implementation**

Add explicit K230 config keys for send throttling, for example:
- `uart_send_interval_ms`
- optional compact-payload toggle only if needed; otherwise hardcode compact UART payload for now

Do not change `detection_interval` in this task.

**Step 4: Run verification**

Re-read `config.py` and ensure:
- keys are documented in Chinese comments
- no unrelated thresholds changed
- UART baud/pins remain unchanged

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Add UART send throttling in K230 main loop

**Files:**
- Modify: `refactored/k230/src/main.py`

**Step 1: Write the failing test / acceptance check**

Define the expected behavior:
- recognition still runs every loop
- UART send happens at most once per configured interval
- the sent frame is the latest computed posture result

Manual red check: current `run()` sends every processed frame via `self.send_to_esp32(output_payload)`.

**Step 2: Run the red check**

Read `main.py:819-833` and confirm there is no send timestamp gate.

Expected: fail because every frame is sent.

**Step 3: Write minimal implementation**

In `SittingPostureMonitor`:
- add state such as `self.last_uart_send_ms`
- add a helper like `_should_send_uart(now_ms)` or equivalent inline logic
- in `run()`, continue building `output_payload` every frame, but only call `send_to_esp32()` when the interval has elapsed
- initialize the gate so the first frame can send promptly after startup

Keep the loop non-blocking and do not add long sleeps beyond existing `detection_interval` handling.

**Step 4: Run verification**

Verify by code inspection and runtime-safe syntax checks that:
- recognition path is unchanged
- send gate depends on current monotonic ms clock
- no new blocking behavior was introduced

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Trim default UART payload to core fields only

**Files:**
- Modify: `refactored/k230/src/main.py`
- Review: `refactored/posture_monitor/k230_parser.cpp`

**Step 1: Write the failing test / acceptance check**

Expected compact UART payload fields:
- required: `posture_type`, `is_abnormal`, `consecutive_abnormal`, `confidence`
- allowed metadata only if kept intentionally and size remains controlled
- excluded by default: `person_debug`, `posture_debug`

Manual red check: current `send_to_esp32()` includes both debug objects.

**Step 2: Run the red check**

Inspect `main.py:621-630`.

Expected: fail because oversized debug fields are included.

**Step 3: Write minimal implementation**

Change `send_to_esp32()` to serialize only the compact payload needed by ESP32. Keep `output_to_ide()` behavior unchanged unless it is accidentally coupled to UART payload construction.

**Step 4: Run verification**

Re-read `send_to_esp32()` and confirm:
- `person_debug` and `posture_debug` are no longer serialized for UART
- JSON still contains all fields consumed by `k230_parser.cpp`
- no ESP32 parser change is required

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Validate syntax and affected integrations

**Files:**
- Verify: `refactored/k230/src/main.py`
- Verify: `refactored/k230/src/config.py`
- Review: `refactored/posture_monitor/k230_parser.cpp`

**Step 1: Run syntax validation**

Run:
```bash
python -m py_compile "C:\Users\Tanfu\Desktop\2026\software\refactored\k230\src\main.py" "C:\Users\Tanfu\Desktop\2026\software\refactored\k230\src\config.py"
```

Expected: exit code 0 and no output.

**Step 2: Run diagnostics check**

Run LSP diagnostics on changed Python files.

Expected: zero errors.

**Step 3: Re-verify ESP32 consumer compatibility**

Confirm `refactored/posture_monitor/k230_parser.cpp` still parses the compact fields without requiring any change.

Expected: parser contract still matches.

**Step 4: Document manual device-side validation path**

When hardware is available, verify:
1. Start K230 recognition
2. Observe ESP32 serial logs for at least 30 seconds
3. Confirm `truncated` growth drops sharply or reaches zero
4. Confirm posture updates still arrive and `valid=true` remains stable

**Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: Optional tuning after first pass

**Files:**
- Modify if needed: `refactored/k230/src/config.py`

**Step 1: Manual evaluation**

If overflow persists after Tasks 1-4, tune `uart_send_interval_ms` upward (for example from 300ms to 500ms) before changing recognition cadence.

**Step 2: Re-run validation**

Repeat syntax checks and hardware log observation.

**Step 3: Escalation rule**

Only if compact payload + send throttling still fails should we consider changing `detection_interval` itself or raising UART baudrate after board stability confirmation.

---

## Verification Checklist

- [ ] `main.py` recognition loop still runs every frame
- [ ] UART send is throttled by explicit interval
- [ ] Default UART payload excludes `person_debug` and `posture_debug`
- [ ] `k230_parser.cpp` contract still matches emitted JSON
- [ ] `python -m py_compile` passes for changed K230 files
- [ ] LSP diagnostics are clean for changed files
- [ ] Manual hardware validation path is documented

## Notes for Implementation

- Preserve Chinese comments/docstrings style in `refactored/k230`
- Do not introduce host-only Python features incompatible with CanMV/MicroPython
- Do not change ESP32 parser unless the compact payload unexpectedly breaks compatibility
- Prefer the smallest diff that directly reduces UART pressure
