# K230 Posture Geometry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the K230 posture classifier with a simple 5-keypoint geometric rule set that outputs only `normal`, `hunchback`, `head_down`, `unknown`, or `no_person`, while simplifying the UART contract to a single `posture_type` field.

**Architecture:** Keep YOLOv8n-pose detection and person-presence filtering in place, but remove the old `legacy` / `decoupled` posture scoring branches. Centralize posture classification around two image-coordinate angles, expose a single stable `posture_type`, and clean up K230 docs so README, architecture docs, and comments match the implementation.

**Tech Stack:** CanMV MicroPython on K230, YOLOv8n-pose inference, UART JSON line protocol to ESP32, Markdown documentation.

---

## File Map

- Modify: `refactored/k230/src/pose_detector.py`
  Purpose: replace old multi-path posture analysis with the new 5-keypoint geometric classifier and minimal stable-state logic.
- Modify: `refactored/k230/src/config.py`
  Purpose: delete obsolete posture config keys and define the new threshold/config names.
- Modify: `refactored/k230/src/main.py`
  Purpose: remove `posture_type_fine`, simplify payload construction, and emit UART JSON with only `posture_type`.
- Modify: `refactored/k230/README.md`
  Purpose: document new posture categories, algorithm description, and UART contract.
- Modify: `refactored/docs/architecture.md`
  Purpose: update system-level posture-analysis wording and output-state definitions.
- Optional create: `refactored/k230/docs/posture-algorithm.md`
  Purpose: record the geometric variable definitions and rules if the existing README becomes too dense.
- Review: `refactored/posture_monitor/k230_parser.cpp`
  Purpose: verify whether ESP32 currently depends on removed fields or coarse `bad` status.

---

### Task 1: Lock the new posture contract and remove obsolete config

**Files:**
- Modify: `refactored/k230/src/config.py`
- Review: `refactored/k230/src/pose_detector.py`
- Review: `refactored/k230/src/main.py`

- [ ] **Step 1: Write the failing acceptance check**

Define the target config surface before editing code. The final posture config should contain only the values needed by the new algorithm:

```python
POSTURE_THRESHOLDS = {
    "torso_tilt_threshold_deg": 18.0,
    "head_tilt_threshold_deg": 24.0,
    "required_keypoint_confidence": 0.25,
    "stable_frame_count": 1,
}
```

Manual red check: current `config.py` still contains obsolete keys such as `posture_mode`, `risk_*`, `baseline_*`, `class_*`, `shoulder_roll_ratio_max`, and `head_forward_ratio_max`.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/config.py` and confirm the old posture config still includes risk-fusion and side-tilt fields.

Expected: fail because the config surface is much larger than the new design requires.

- [ ] **Step 3: Write minimal implementation**

Replace the old posture block with a compact one like this, preserving the Chinese explanation style:

```python
POSTURE_THRESHOLDS = {
    # torso_tilt_threshold_deg: 躯干线相对图像竖直方向的容忍角度（度）
    # 调大 -> 更不敏感；调小 -> 更容易判驼背。
    "torso_tilt_threshold_deg": 18.0,
    # head_tilt_threshold_deg: 鼻子到肩中点连线相对图像竖直方向的容忍角度（度）
    # 调大 -> 更不敏感；调小 -> 更容易判低头。
    "head_tilt_threshold_deg": 24.0,
    # required_keypoint_confidence: 5 个必需关键点的最低置信度要求
    "required_keypoint_confidence": 0.25,
    # stable_frame_count: 同一类别连续出现多少帧后才确认输出
    # 单张图片测试默认保持 1，实时视频可调整为 2。
    "stable_frame_count": 1,
}
```

Do not change unrelated camera, model, or UART pin settings in this task.

- [ ] **Step 4: Run verification**

Re-read `config.py` and confirm:

- the obsolete posture keys are gone
- the four new keys are documented in Chinese comments
- `POSTURE_THRESHOLDS` names match the design spec exactly

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Replace the old classifier in `pose_detector.py`

**Files:**
- Modify: `refactored/k230/src/pose_detector.py`
- Review: `refactored/k230/src/config.py`

- [ ] **Step 1: Write the failing acceptance check**

Define the final behavior of `_analyze_posture...` before changing code:

```python
# Target behavior
pose_type, posture_debug = detector._analyze_posture(keypoints)

assert pose_type in ("normal", "hunchback", "head_down", "unknown")
assert "torso_tilt_deg" in posture_debug
assert "head_tilt_deg" in posture_debug
assert "torso_excess_ratio" in posture_debug
assert "head_excess_ratio" in posture_debug
```

Manual red check: current `pose_detector.py` still routes through `_analyze_posture_detailed()` and `_analyze_posture_decoupled()`, returns old keys like `neck`, `back`, `risk`, `risk_ema`, and still knows about `tilt`.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/pose_detector.py` and confirm all of the following are still present:

- `self.posture_mode`
- `self.posture_risk_ema`
- `self.class_ema`
- `_analyze_posture_detailed`
- `_analyze_posture_decoupled`
- `_dominant_abnormal`

Expected: fail because the file still implements the old architecture.

- [ ] **Step 3: Write minimal implementation**

Refactor `PoseDetector` to keep only the state required by the new design. The constructor should read the new config names and define a minimal stability state, for example:

```python
self.required_keypoint_confidence = float(
    self.posture_thresholds.get("required_keypoint_confidence", 0.25)
)
self.stable_frame_count = int(self.posture_thresholds.get("stable_frame_count", 1))
self.stable_posture_type = "normal"
self.pending_posture_type = None
self.pending_posture_count = 0
```

Then replace the two old analysis branches with a single method such as:

```python
def _analyze_posture(self, keypoints):
    required = self._extract_required_keypoints(keypoints)
    if required is None:
        return "unknown", {
            "required_keypoints_ready": False,
            "torso_tilt_deg": 0.0,
            "head_tilt_deg": 0.0,
            "torso_excess_ratio": 0.0,
            "head_excess_ratio": 0.0,
        }

    nose = required["nose"]
    left_shoulder = required["left_shoulder"]
    right_shoulder = required["right_shoulder"]
    left_hip = required["left_hip"]
    right_hip = required["right_hip"]

    shoulder_midpoint = self._midpoint(left_shoulder, right_shoulder)
    hip_midpoint = self._midpoint(left_hip, right_hip)
    torso_length = self._distance(shoulder_midpoint, hip_midpoint)
    if torso_length <= 1e-6:
        return "unknown", {
            "required_keypoints_ready": False,
            "torso_tilt_deg": 0.0,
            "head_tilt_deg": 0.0,
            "torso_excess_ratio": 0.0,
            "head_excess_ratio": 0.0,
        }

    torso_tilt_deg = self._tilt_from_vertical(shoulder_midpoint, hip_midpoint)
    head_tilt_deg = self._tilt_from_vertical(nose, shoulder_midpoint)

    torso_threshold = float(self.posture_thresholds.get("torso_tilt_threshold_deg", 18.0))
    head_threshold = float(self.posture_thresholds.get("head_tilt_threshold_deg", 24.0))

    torso_excess_ratio = self._excess_ratio(torso_tilt_deg, torso_threshold)
    head_excess_ratio = self._excess_ratio(head_tilt_deg, head_threshold)

    if torso_excess_ratio <= 0.0 and head_excess_ratio <= 0.0:
        pose_type = "normal"
    elif torso_excess_ratio > 0.0 and head_excess_ratio <= 0.0:
        pose_type = "hunchback"
    elif head_excess_ratio > 0.0 and torso_excess_ratio <= 0.0:
        pose_type = "head_down"
    elif torso_excess_ratio > head_excess_ratio:
        pose_type = "hunchback"
    else:
        pose_type = "head_down"

    stable_pose_type = self._stabilize_posture_type(pose_type)
    return stable_pose_type, {
        "required_keypoints_ready": True,
        "shoulder_midpoint": shoulder_midpoint,
        "hip_midpoint": hip_midpoint,
        "torso_length": float(torso_length),
        "torso_tilt_deg": float(torso_tilt_deg),
        "head_tilt_deg": float(head_tilt_deg),
        "torso_excess_ratio": float(torso_excess_ratio),
        "head_excess_ratio": float(head_excess_ratio),
        "stable_frame_count": int(self.stable_frame_count),
    }
```

Add focused helpers rather than reusing old risk helpers:

```python
def _midpoint(self, p1, p2):
    return ((float(p1[0]) + float(p2[0])) / 2.0, (float(p1[1]) + float(p2[1])) / 2.0)

def _excess_ratio(self, value, threshold):
    if threshold <= 1e-6:
        return 0.0
    return max(0.0, (float(value) - float(threshold)) / float(threshold))
```

For keypoint readiness, introduce an explicit extractor so `unknown` is decided in one place:

```python
def _extract_required_keypoints(self, keypoints):
    required_names = (
        "nose",
        "left_shoulder",
        "right_shoulder",
        "left_hip",
        "right_hip",
    )
    result = {}
    for name in required_names:
        kp = keypoints[self.KEYPOINT_INDICES[name]]
        if len(kp) < 3 or self._kp_conf(kp) < self.required_keypoint_confidence:
            return None
        result[name] = kp
    return result
```

Delete the old methods and fields once the new path is in place.

- [ ] **Step 4: Run verification**

Re-read `pose_detector.py` and confirm:

- there is only one posture-analysis path
- no code references `tilt`, `legacy`, `decoupled`, `risk_ema`, or `baseline`
- the only abnormal outputs are `hunchback` and `head_down`
- `unknown` is returned when the required 5 keypoints are not ready

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Simplify K230 frame output and UART payload

**Files:**
- Modify: `refactored/k230/src/main.py`
- Review: `refactored/posture_monitor/k230_parser.cpp`

- [ ] **Step 1: Write the failing acceptance check**

Define the new frame contract:

```python
result = {
    "posture_type": "head_down",
}
```

Internal debug fields may still exist inside K230 for IDE observation, but UART must serialize only this one field. Manual red check: current `process_frame()` still builds `posture_type_fine`, `is_abnormal`, `confidence`, `person_debug`, and `posture_debug`, while `send_to_esp32()` serializes four fields.

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/src/main.py` and confirm all of the following are still present:

- `posture_type_fine`
- `bad`
- `is_abnormal`
- `consecutive_abnormal`
- `confidence` in the UART JSON output

Expected: fail because the file still emits the old contract.

- [ ] **Step 3: Write minimal implementation**

Reduce `process_frame()` so the primary returned value is the final `posture_type`. Keep optional in-memory debug data only if it still helps local IDE inspection. The relevant shape should become:

```python
result = {
    "posture_type": "unknown",
    "posture_debug": {},
    "person_debug": {},
}
```

When `detect_pose()` returns a classification:

```python
pose_type = detection_result.get("pose_type", "unknown")
result["posture_type"] = pose_type
result["posture_debug"] = detection_result.get("angles", {})
result["person_debug"] = detection_result.get("person_debug", {})
```

When no person is detected:

```python
result["posture_type"] = "no_person"
result["posture_debug"] = {}
result["person_debug"] = getattr(self.detector, "last_person_debug", {})
```

Then shrink `send_to_esp32()` to:

```python
output = {
    "posture_type": data.get("posture_type", "unknown"),
}
json_str = json.dumps(output) + "\n"
```

In the main loop, remove K230-side abnormal counters if they only served the old UART contract. If any counter is still needed for local logging, keep it internal and do not serialize it.

- [ ] **Step 4: Run verification**

Verify by inspection that:

- UART payload contains only `posture_type`
- the literal `"bad"` is no longer used in `main.py`
- `posture_type_fine` no longer exists
- the output state set matches `normal`, `hunchback`, `head_down`, `unknown`, `no_person`

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Update docs to match the new algorithm and protocol

**Files:**
- Modify: `refactored/k230/README.md`
- Modify: `refactored/docs/architecture.md`
- Optional create: `refactored/k230/docs/posture-algorithm.md`

- [ ] **Step 1: Write the failing acceptance check**

List the stale documentation that must disappear:

- references to `tilt` as an active posture class
- references to `legacy` / `decoupled` as the current runtime path
- UART examples containing `bad`, `posture_type_fine`, `is_abnormal`, or other removed fields

Expected target wording:

```markdown
- Posture analysis (`normal`, `hunchback`, `head_down`, `unknown`, `no_person`)
- UART JSON output: {"posture_type":"head_down"}
```

- [ ] **Step 2: Run the red check**

Inspect `refactored/k230/README.md` and `refactored/docs/architecture.md`.

Expected: fail because both docs still mention the old categories and old output format.

- [ ] **Step 3: Write minimal implementation**

Update `refactored/k230/README.md` so the feature section and UART contract read like this:

```markdown
- Posture analysis based on 5 keypoints (`normal`, `hunchback`, `head_down`, `unknown`, `no_person`)
- Uses the midpoint of both shoulders and the midpoint of both hips to form the torso line
- Uses the line from nose to shoulder midpoint to estimate head tilt
- UART JSON output for downstream firmware: `{"posture_type":"head_down"}`
```

Update `refactored/docs/architecture.md` so the K230 section describes:

```markdown
当前 K230 视觉端采用 5 个关键点的几何规则判定路径：
- 双肩中点与双髋中点连线相对图像竖直方向的夹角用于识别驼背
- 鼻子与双肩中点连线相对图像竖直方向的夹角用于识别低头
- 关键点不齐全时输出 unknown
```

If the README becomes too dense, add `refactored/k230/docs/posture-algorithm.md` with the variable definitions and link to it from the README.

- [ ] **Step 4: Run verification**

Re-read both documents and confirm:

- no stale `tilt` runtime wording remains
- no stale `legacy` / `decoupled` current-runtime wording remains
- all UART examples show the one-field contract
- the posture type set matches the implementation

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: Verify syntax and integration boundaries

**Files:**
- Verify: `refactored/k230/src/pose_detector.py`
- Verify: `refactored/k230/src/config.py`
- Verify: `refactored/k230/src/main.py`
- Review: `refactored/posture_monitor/k230_parser.cpp`

- [ ] **Step 1: Run syntax validation**

Run:

```bash
python -m py_compile "C:\Users\Tanfu\Desktop\2026\software\refactored\k230\src\pose_detector.py" "C:\Users\Tanfu\Desktop\2026\software\refactored\k230\src\config.py" "C:\Users\Tanfu\Desktop\2026\software\refactored\k230\src\main.py"
```

Expected: exit code `0` and no output.

- [ ] **Step 2: Run diagnostics review**

Run LSP diagnostics or equivalent editor diagnostics on the three changed Python files.

Expected: zero syntax errors and no unresolved references introduced by the cleanup.

- [ ] **Step 3: Re-check ESP32 parser assumptions**

Inspect `refactored/posture_monitor/k230_parser.cpp` and confirm whether it currently requires removed fields like `is_abnormal` or `consecutive_abnormal`. If it does, either:

- update the parser in the same implementation session, or
- stop and explicitly record that K230 and ESP32 must be changed together before claiming integration is complete.

Expected: integration requirement clearly resolved, not guessed.

- [ ] **Step 4: Run manual posture validation**

Use single-image samples and verify the expected output path:

1. Normal sample -> `normal`
2. Hunchback sample -> `hunchback`
3. Head-down sample -> `head_down`
4. Missing keypoint sample -> `unknown`
5. No-person sample -> `no_person`

If board hardware is unavailable, document the exact manual steps that remain pending instead of claiming full validation.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

## Verification Checklist

- [ ] `config.py` contains only the new posture thresholds and `stable_frame_count`
- [ ] `pose_detector.py` has one posture-analysis path with no `legacy` / `decoupled` leftovers
- [ ] `pose_detector.py` can output only `normal`, `hunchback`, `head_down`, `unknown`, `no_person`
- [ ] `main.py` no longer uses `bad` or `posture_type_fine`
- [ ] UART payload contains only `posture_type`
- [ ] `k230/README.md` and `docs/architecture.md` match the new algorithm and protocol
- [ ] `python -m py_compile` passes for changed K230 files
- [ ] Any ESP32 parser dependency on removed fields has been explicitly resolved

## Notes for Implementation

- Preserve the Chinese docstring/comment style used in `refactored/k230`
- Prefer the smallest correct cleanup; do not add new abstraction layers unless they directly simplify the file
- Avoid host-only Python features that are risky on CanMV/MicroPython
- Do not keep compatibility shims for `tilt`, `bad`, or `posture_type_fine` unless a real downstream dependency is confirmed
- Do not commit unless the user explicitly asks
