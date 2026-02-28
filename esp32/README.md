# posture_monitor

ESP32-S3 firmware for a posture monitoring device. The board receives posture results from a K230 vision module over UART, reports device properties to OneNET by MQTT, and drives local alert outputs.

## Features

- UART JSON parsing from K230 (`k230_parser.h`)
- WiFi + MQTT telemetry and cloud command handling (`mqtt_handler.h`)
- Onboard status LED indication and optional buzzer alerts (`alerts.h`)
- Optional modules with compile-time stubs: OLED, EC11, PIR, light sensor, voice
- Non-blocking loop scheduling with `millis()` timers

## Hardware

- ESP32-S3 (Freenove ESP32-S3 WROOM board)
- K230 vision module (UART)
- Optional peripherals controlled by `ENABLE_*` flags in `config.h`

## Repository Layout

```text
posture_monitor/
├── posture_monitor.ino
├── config.h
├── utils.h
├── mqtt_handler.h
├── k230_parser.h
├── mode_manager.h
├── display.h
├── sensors.h
├── voice.h
└── alerts.h
```

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

## Additional Docs

- `docs/hardware-wiring-and-controls.md`: wiring map, power notes, and timer interaction design.
- `docs/onenet-property-model.md`: unified OneNET property identifiers, types, ranges, and JSON examples.
