# refactored/k230

K230-side vision module for real-time human posture detection. The program captures camera frames, runs YOLOv8n-pose inference on device, classifies posture state, and sends structured JSON data to ESP32 through UART.

## Features

- Camera acquisition on K230
- YOLOv8n-pose inference (`.kmodel`)
- Posture analysis based on 5 keypoints (`normal`, `head_down`, `hunchback`, `unknown`, `no_person`)
- Uses shoulder midpoint and hip midpoint to form the torso line for hunchback detection
- Uses the line from nose to shoulder midpoint to estimate head tilt for head-down detection
- UART JSON output for downstream IoT firmware (`{"posture_type":"head_down"}`)
- Structured runtime logging and fault-tolerant startup

## Repository Layout

```text
refactored/k230/
├── src/
│   ├── main.py
│   ├── config.py
│   ├── pose_detector.py
│   ├── camera.py
│   ├── canmv_api.py
│   └── logger.py
├── models/
│   └── yolov8n_pose.kmodel
├── docs/
└── README.md
```

Note: legacy copies may still use the `K230Vision` name, but the active subtree for current development is `refactored/k230`.

## Environment

- Hardware: K230 dev board + camera
- Runtime: CanMV/MicroPython on K230
- Model: `models/yolov8n_pose.kmodel`

## Run

1. Connect K230 board in CanMV IDE.
2. Upload `src/*.py` to the board.
3. Upload `models/yolov8n_pose.kmodel` to device storage. The current code/config commonly references `/sdcard/examples/kmodel/`.
4. Run `main.py`.

## Testing

- No verified current host-side test module was found under `refactored/k230/tests` during this cleanup.
- Prefer board-level/manual verification unless a real test file is added and documented.

## UART Contract (to ESP32)

Program outputs JSON lines over UART at `115200` baud. The default payload contains only one field:

- `posture_type`

Allowed values:

- `normal`
- `head_down`
- `hunchback`
- `unknown`
- `no_person`

## Docs

- Product docs index: `refactored/docs/index.md`
- `docs/k230-pinout.md`: K230 pin notes and wiring-related references.
- `docs/integration-debug-summary-2026-02.md`: integration notes with ESP32.
- `docs/worklog-2026-02-22.md`: historical development worklog.

## Troubleshooting

- Model load failure: verify model file path and name.
- Camera init failure: check sensor connection and reboot board.
- UART issues: verify wiring and baudrate on both ends.

## License

MIT
