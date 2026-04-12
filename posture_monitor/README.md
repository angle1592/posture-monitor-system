# posture_monitor

ESP32-S3 firmware for a posture monitoring device. The board receives posture results from a K230 vision module over UART, reports device properties to OneNET by MQTT, and drives local alert outputs.

## Features

- UART JSON parsing from K230 (`k230_parser.h`)
- WiFi + MQTT telemetry and cloud command handling (`mqtt_handler.h`)
- Onboard status LED indication and optional buzzer alerts (`alerts.h`)
- Optional modules with compile-time stubs: OLED, EC11, presence sensor, light sensor, voice
- Non-blocking loop scheduling with `millis()` timers

## Hardware

- ESP32-S3 (Freenove ESP32-S3 WROOM board)
- K230 vision module (UART)
- Optional peripherals controlled by `ENABLE_*` flags in `config.h`

## Repository Layout

```text
refactored/posture_monitor/
├── posture_monitor.ino
├── config.h
├── platformio.ini
├── utils.h / utils.cpp
├── mqtt_handler.h / mqtt_handler.cpp
├── k230_parser.h / k230_parser.cpp
├── mode_manager.h / mode_manager.cpp
├── display.h / display.cpp
├── sensors.h / sensors.cpp
├── voice.h / voice.cpp
├── alerts.h / alerts.cpp
└── timer_fsm.h / timer_fsm.cpp
```

Generated files under `.arduino-build/` are build artifacts and should not be treated as the source of truth for normal edits.

## Build and Flash

### Arduino IDE

1. Board: `ESP32S3 Dev Module`
2. USB CDC On Boot: `Enabled`
3. Select correct COM port
4. Upload (`Ctrl+U`)

### PlatformIO (optional)

```bash
pio run
pio run -t upload
pio device monitor
```

Serial monitor: `115200` baud, line ending `LF` or `CR LF`.

No repo-evidenced automated test command is currently configured for this subtree.

## Arduino Library Dependencies

- `U8g2` (required when `ENABLE_OLED=1`)

If `U8g2` is missing and `ENABLE_OLED=1`, compilation will fail with missing `U8g2lib.h`.

## Configuration

All runtime configuration is in `config.h`:

- Pin mapping and UART settings
- WiFi and MQTT parameters
- Module enable switches (`ENABLE_OLED`, `ENABLE_VOICE`, etc.)
- Cloud topics and timing intervals

## Notes

- This project uses manual JSON parsing/building (`strstr`/`sprintf`) and does not use ArduinoJson.
- Keep real credentials out of version control.
- The current presence sensor path is a beam-break infrared sensor; HC-SR501 demos remain only for isolated historical troubleshooting.

## Additional Docs

- Product docs index: `refactored/docs/index.md`
- `docs/hardware-wiring.md`: wiring map, power notes, and timer interaction design.
- `docs/onenet-property-model.md`: unified OneNET property identifiers, types, ranges, and JSON examples.
- `docs/integration-debug-summary-2026-02.md`: integration notes and troubleshooting history.
- `demos/README.md`: standalone troubleshooting sketches, including historical HC-SR501 tests and current beam-break-related wiring notes.
