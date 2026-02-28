# K230Vision

K230-side vision module for real-time human posture detection. The program captures camera frames, runs YOLOv8n-pose inference on device, classifies posture state, and sends structured JSON data to ESP32 through UART.

## Features

- Camera acquisition on K230
- YOLOv8n-pose inference (`.kmodel`)
- Posture analysis (`normal`, `head_down`, `hunchback`, `tilt`, `no_person`)
- UART JSON output for downstream IoT firmware
- Structured runtime logging and fault-tolerant startup

## Repository Layout

```text
K230Vision/
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
└── tests/
```

## Environment

- Hardware: K230 dev board + camera
- Runtime: CanMV/MicroPython on K230
- Model: `models/yolov8n_pose.kmodel`

## Run

1. Connect K230 board in CanMV IDE.
2. Upload `src/*.py` to the board.
3. Upload `models/yolov8n_pose.kmodel` to device storage (for example `/sdcard/models/`).
4. Run `main.py`.

## UART Contract (to ESP32)

Program outputs JSON lines over UART at `115200` baud, including fields such as:

- `frame_id`
- `posture_type`
- `is_abnormal`
- `consecutive_abnormal`
- `confidence`
- `timestamp`

## Troubleshooting

- Model load failure: verify model file path and name.
- Camera init failure: check sensor connection and reboot board.
- UART issues: verify wiring and baudrate on both ends.

## License

MIT