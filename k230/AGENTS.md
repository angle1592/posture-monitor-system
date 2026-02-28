# AGENTS.md - K230 Vision Module

## Project Overview

K230Vision is an embedded vision system for the K230 development board that performs real-time human pose detection and sitting posture analysis using YOLOv8n-pose. It communicates detection results to an ESP32 controller via UART for IoT integration.

**Technology Stack:**
- Language: Python (MicroPython variant for CanMV/K230)
- ML Framework: nncase_runtime for NPU-accelerated inference
- Model: YOLOv8n-pose (kmodel format)
- Hardware: K230 dev board with camera, ESP32 for cloud connectivity

---

## Build/Lint/Test Commands

### Testing

```bash
# Run algorithm unit tests on host machine (no K230 hardware required)
python -m unittest tests/test_pose_algorithms.py -v

# Run a single test class
python -m unittest tests.test_pose_algorithms.TestPoseAlgorithms -v

# Run a single test method
python -m unittest tests.test_pose_algorithms.TestPoseAlgorithms.test_iou -v
```

### Deployment

No build step required. Deploy via CanMV IDE:
1. Open CanMV IDE and connect to K230 board via USB
2. Upload `src/*.py` files to the board
3. Upload `models/yolov8n_pose.kmodel` to `/sdcard/models/` or `/sdcard/examples/kmodel/`
4. Run `main.py` from IDE

### Linting

No linting configuration exists. Follow PEP8 conventions as documented below.

---

## Code Style Guidelines

### Imports

Organize imports in three groups, separated by blank lines:

```python
# Standard library
import time
import gc
import math

# Third-party / Hardware APIs
from machine import UART
import nncase_runtime as nn
import ulab.numpy as np

# 导入自定义模块 (Local modules - with Chinese comment)
from config import APP_CONFIG, MODEL_CONFIG
from logger import get_logger
from camera import CameraModule
```

**Rules:**
- Use absolute imports: `from config import APP_CONFIG` (not `from .config`)
- No relative imports
- Group local imports with the comment `# 导入自定义模块`

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Variables | `snake_case` | `frame_count`, `is_running`, `posture_type` |
| Functions | `snake_case` | `initialize()`, `process_frame()`, `detect_pose()` |
| Classes | `PascalCase` | `PoseDetector`, `CameraModule`, `ProjectLogger` |
| Constants | `UPPER_CASE` | `MODEL_CONFIG`, `APP_CONFIG`, `_LEVELS` |
| Private methods | `_leading_underscore` | `_calculate_iou()`, `_nms()`, `_log()` |
| Class attributes | `snake_case` | `self.is_initialized`, `self.frame_count` |

### Type Annotations

Not used in this codebase. Rely on docstrings for type documentation.

### Docstrings

Use Chinese docstrings for all classes and public methods. Format:

```python
class PoseDetector:
    """
    YOLOv8n-pose 姿态检测器

    主要功能：
        1. 加载 kmodel 模型文件
        2. 预处理图像输入
        3. 运行 NPU 推理
        4. 后处理输出并分析姿态

    使用示例：
        >>> detector = PoseDetector(model_path="/sdcard/models/yolov8n_pose.kmodel")
        >>> if detector.initialize():
        ...     result = detector.detect_pose(image_array)
    """

    def detect_pose(self, input_np):
        """
        检测图像中的人体姿态

        参数:
            input_np: numpy数组，图像数据 (H, W, C) 或 (C, H, W)

        返回:
            dict: 检测结果，包含:
                - pose_type: str 姿态类型
                - confidence: float 检测置信度
                - keypoints: list 关键点列表
                - bbox: tuple 边界框 (x1, y1, x2, y2)
            若未检测到人体，返回 None
        """
```

### Comments

- **File headers**: Detailed Chinese docstrings describing module purpose and architecture
- **Inline comments**: English for technical explanations, Chinese for user-facing notes
- **Configuration comments**: Explain parameter meaning, adjustment guidance, and recommended ranges

```python
# 候选框置信度阈值（第一道过滤）
# 含义：postprocess 阶段保留候选框的最低置信度。
# 调大 -> 误检更少但可能漏检；调小 -> 更容易出框但噪声框更多。
# 建议步长：0.02；建议范围：0.10 ~ 0.35。
"conf_threshold": 0.20,
```

### Error Handling

```python
# Pattern 1: Import errors with user guidance
try:
    from config import APP_CONFIG
except ImportError as e:
    print("[-] 错误：无法导入模块")
    print(f"    详细错误: {str(e)}")
    print("[*] 请确保以下文件在同一目录:")
    raise

# Pattern 2: Runtime errors with exception details
try:
    output_data = self._infer(input_tensor)
except Exception as e:
    self._log("[-] detect_pose failed: {}".format(str(e)))
    import sys
    sys.print_exception(e)
    return None

# Pattern 3: Graceful degradation
try:
    self.uart = UART(uart_id, baudrate=baudrate)
except Exception as e:
    print("[-] UART初始化失败: {}".format(str(e)))
    print("[*] 继续运行，但无法与ESP32通信")
    self.uart = None
```

### Logging

Use the custom logger module:

```python
from logger import get_logger

logger = get_logger("module_name", debug=False)
logger.debug("详细调试信息")  # Only shown when debug=True
logger.info("正常运行信息")
logger.warn("警告信息")
logger.error("错误信息")
```

Output format: `[LEVEL][module_name] message`

---

## Project Structure

```
K230Vision/
├── src/
│   ├── __init__.py         # Package marker
│   ├── main.py             # Main entry point, SittingPostureMonitor class
│   ├── config.py           # Configuration constants (MODEL_CONFIG, APP_CONFIG, etc.)
│   ├── pose_detector.py    # YOLOv8n-pose detection and posture analysis
│   ├── camera.py           # K230 camera initialization and capture
│   ├── canmv_api.py        # CanMV API utilities (path, sensor config)
│   └── logger.py           # Simple logging utility
├── models/
│   └── yolov8n_pose.kmodel # Model file (upload separately)
├── tests/
│   └── test_pose_algorithms.py  # Unit tests (stubs CanMV dependencies)
├── docs/                   # Documentation
└── README.md               # Project documentation
```

---

## Configuration

All configuration is centralized in `src/config.py`:

- `MODEL_CONFIG`: Model paths, input size, detection thresholds
- `APP_CONFIG`: Camera, UART, runtime behavior settings
- `PERSON_FILTER`: Human presence detection thresholds
- `POSTURE_THRESHOLDS`: Posture analysis parameters

When modifying thresholds, update the config file and document the change in comments.

---

## Hardware Requirements

- K230 development board with camera
- ESP32 for UART communication (optional)
- CanMV IDE for development and deployment

---

## Important Notes for Agents

1. **No type hints**: This codebase doesn't use Python type annotations
2. **MicroPython limitations**: Some standard library features unavailable on K230
3. **Chinese comments**: Primary documentation language is Chinese
4. **NPU inference**: Model inference uses `nncase_runtime` with AI2D preprocessing
5. **Testing**: Tests run on host Python with CanMV stubs; not on actual hardware
6. **No git**: Project is not version controlled (has .history for local edits)
