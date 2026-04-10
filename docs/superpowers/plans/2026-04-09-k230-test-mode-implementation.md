# K230 Test Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated K230 IDE test mode that forces IDE preview on, disables ESP32 UART output, and prints per-frame `frame / posture / latency_ms / fps` logs to the IDE serial console.

**Architecture:** Introduce a single `test_mode` config switch in `k230/src/config.py`, derive runtime behavior from that switch inside `k230/src/main.py`, and keep `camera.py` unchanged unless preview verification proves a runtime issue. The feature is a runtime behavior override, not a new algorithm path.

**Tech Stack:** CanMV MicroPython on K230, YOLOv8n-pose runtime, CanMV IDE preview/display pipeline, UART JSON output path.

---

## File Map

- Modify: `refactored/k230/src/config.py`
  Purpose: add `test_mode` and document its effect.
- Modify: `refactored/k230/src/main.py`
  Purpose: derive effective runtime switches from `test_mode`, print test-mode startup banner, and emit per-frame IDE serial output.
- Review: `refactored/k230/src/camera.py`
  Purpose: confirm existing `ide_preview=True` path is sufficient; do not change unless verification shows it is necessary.
- Optional modify: `refactored/k230/README.md`
  Purpose: mention test mode if code changes make usage more discoverable.

---

### Task 1: Add `test_mode` to K230 config

**Files:**
- Modify: `refactored/k230/src/config.py`

- [ ] **Step 1: Write the failing acceptance check**

Define the target config surface for testing:

```python
APP_CONFIG = {
    "test_mode": True,
    "enable_esp32_uart": False,
    "ide_preview": False,
}
```

Manual red check: `config.py` currently has `enable_esp32_uart` and `ide_preview`, but no explicit `test_mode` switch that documents the testing workflow.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/config.py`.

Expected: fail because `test_mode` is missing.

- [ ] **Step 3: Write minimal implementation**

Add `test_mode` near the runtime behavior flags and document it clearly:

```python
    # 测试模式开关：用于在 CanMV IDE 中验证算法效果。
    # 开启后会强制打开 IDE 预览、关闭发往 ESP32 的 UART，并在 IDE 串口逐帧输出状态。
    "test_mode": True,
```

Do not directly rewrite `enable_esp32_uart` or `ide_preview` values in `config.py` for this task; only add the new flag and comment.

- [ ] **Step 4: Run verification**

Re-read `config.py` and confirm:

- `test_mode` exists
- the comment explicitly mentions IDE preview, UART disable, and per-frame IDE output
- no unrelated config keys changed in this task

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Derive effective runtime behavior from `test_mode`

**Files:**
- Modify: `refactored/k230/src/main.py`

- [ ] **Step 1: Write the failing acceptance check**

Define the expected runtime behavior:

```python
if self.config.get("test_mode", False):
    self.test_mode = True
    self.enable_esp32_uart = False
    self.ide_preview_enabled = True
else:
    self.test_mode = False
    self.enable_esp32_uart = bool(self.config.get("enable_esp32_uart", False))
    self.ide_preview_enabled = bool(self.config.get("ide_preview", False))
```

Manual red check: current `main.py` reads `enable_esp32_uart` directly from config, does not have `test_mode`, and does not print a clear testing banner.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/main.py` and confirm:

- there is no `self.test_mode`
- UART init is still gated directly by `self.config.get("enable_esp32_uart", False)`
- startup logs do not announce a dedicated IDE testing mode

Expected: fail because runtime override logic is missing.

- [ ] **Step 3: Write minimal implementation**

In `__init__`, derive effective runtime flags once:

```python
self.test_mode = bool(self.config.get("test_mode", False))
self.enable_esp32_uart = bool(self.config.get("enable_esp32_uart", False))
self.ide_preview_enabled = bool(self.config.get("ide_preview", False))

if self.test_mode:
    self.enable_esp32_uart = False
    self.ide_preview_enabled = True
    self.runtime_verbose_logs = False
    self.metrics_enabled = False
```

Then update initialization logs to make the mode obvious:

```python
if self.test_mode:
    print("[*] 当前运行于测试模式：IDE预览=开，ESP32 UART=关，逐帧日志=开")
```

Update all UART gating in `initialize()` and `run()` to use `self.enable_esp32_uart` instead of repeatedly reading the raw config.

- [ ] **Step 4: Run verification**

Re-read `main.py` and confirm:

- `test_mode` exists as a derived runtime flag
- UART decisions use `self.enable_esp32_uart`
- startup prints the test-mode banner when enabled
- `runtime_verbose_logs` and `metrics_enabled` are forced off in test mode

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Add per-frame IDE serial output for testing

**Files:**
- Modify: `refactored/k230/src/main.py`

- [ ] **Step 1: Write the failing acceptance check**

Define the target line format:

```text
frame=37 posture=head_down latency_ms=82 fps=8.9
```

Manual red check: current non-verbose `output_to_ide()` only prints `posture_type`, and current main loop does not pass `infer_ms` or `fps` into the IDE output path.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/main.py` and confirm:

- `output_to_ide()` in quiet mode prints only `posture_type`
- `output_payload` does not include `infer_ms` or `fps`

Expected: fail because per-frame test output is missing.

- [ ] **Step 3: Write minimal implementation**

In the main loop, add `infer_ms` and `fps` into `output_payload`:

```python
output_payload = {
    "frame_id": self.frame_count,
    "posture_type": result["posture_type"],
    "infer_ms": infer_ms,
    "fps": fps,
    "person_debug": result.get("person_debug", {}),
    "posture_debug": result.get("posture_debug", {}),
}
```

Then update `output_to_ide()` so test mode prints exactly one concise line per frame:

```python
if self.test_mode:
    print(
        "frame={} posture={} latency_ms={} fps={:.1f}".format(
            data.get("frame_id", 0),
            data.get("posture_type", "unknown"),
            int(data.get("infer_ms", 0)),
            float(data.get("fps", 0.0)),
        )
    )
    return
```

Keep the existing verbose JSON-style IDE output path for non-test debugging.

- [ ] **Step 4: Run verification**

Re-read `main.py` and confirm:

- `output_payload` contains `infer_ms` and `fps`
- `output_to_ide()` has a dedicated `self.test_mode` branch
- the per-frame line format is exactly `frame / posture / latency_ms / fps`
- non-test verbose output still exists for deeper diagnostics

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Verify preview path and runtime behavior

**Files:**
- Review: `refactored/k230/src/camera.py`
- Verify: `refactored/k230/src/main.py`
- Verify: `refactored/k230/src/config.py`

- [ ] **Step 1: Review preview dependency**

Inspect `camera.py` and verify that preview is controlled by:

```python
if APP_CONFIG.get("ide_preview", True):
    Display.init(..., to_ide=True)
```

Expected: current implementation depends on `APP_CONFIG` directly, not a runtime flag supplied by `main.py`.

- [ ] **Step 2: Resolve preview override path**

If preview is still read only from `APP_CONFIG`, add the smallest safe override so test mode actually takes effect. Use one of these patterns:

```python
effective_preview = self.ide_preview_enabled
```

or, if `camera.py` must remain config-driven, update `APP_CONFIG["ide_preview"]` before camera initialization in `main.py`:

```python
if self.test_mode:
    self.config["ide_preview"] = True
    APP_CONFIG["ide_preview"] = True
```

Prefer the smallest correct change that guarantees `camera.py` sees the overridden value before `CameraModule()` initializes.

- [ ] **Step 3: Run syntax verification**

Run:

```bash
python -m py_compile "C:\Users\Tanfu\Desktop\2026\software\refactored\.worktrees\k230-posture-geometry\k230\src\config.py" "C:\Users\Tanfu\Desktop\2026\software\refactored\.worktrees\k230-posture-geometry\k230\src\main.py" "C:\Users\Tanfu\Desktop\2026\software\refactored\.worktrees\k230-posture-geometry\k230\src\camera.py"
```

Expected: exit code `0` and no output.

- [ ] **Step 4: Run manual behavior check**

Verify by inspection that test mode now implies:

- startup banner says test mode is active
- effective UART send path is disabled
- IDE output path is always used
- preview path will be enabled before camera initialization

If hardware is unavailable, explicitly report that final on-device preview verification is still pending.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

## Verification Checklist

- [ ] `config.py` contains a documented `test_mode`
- [ ] `main.py` derives `self.test_mode`, `self.enable_esp32_uart`, and preview behavior from config
- [ ] test mode forces ESP32 UART off
- [ ] test mode forces IDE preview on
- [ ] IDE serial output prints one line per frame with `frame / posture / latency_ms / fps`
- [ ] `python -m py_compile` passes for changed K230 files
- [ ] Any preview override needed by `camera.py` is explicitly handled

## Notes for Implementation

- Keep the change minimal; this is a runtime mode override, not a new pipeline
- Do not remove the existing verbose IDE JSON output path
- Do not reintroduce any old `bad` / `posture_type_fine` UART behavior
- Do not touch ESP32 code in this plan unless a new blocker is discovered
- Do not commit unless the user explicitly asks
