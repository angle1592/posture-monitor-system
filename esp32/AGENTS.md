# AGENTS.md — 智能语音坐姿提醒器 (Smart Voice Posture Reminder)

This document provides coding guidelines for agentic AI assistants working on this ESP32-S3 Arduino firmware project.

---

## Project Overview

**Platform**: ESP32-S3 (Freenove ESP32-S3 WROOM Board)
**Framework**: Arduino
**Hardware**: K230 Vision Module + WS2812 LED + Optional OLED/EC11/PIR/Voice
**Cloud**: OneNET MQTT Platform

---

## Build / Flash Commands

### Arduino IDE
1. Board: `ESP32S3 Dev Module`
2. USB CDC On Boot: `Enabled`
3. Port: Select correct COM port
4. Upload: Click Upload button (or `Ctrl+U`)

### PlatformIO (if used)
```bash
pio run              # Build
pio run -t upload    # Flash to board
pio device monitor   # Serial monitor (115200 baud)
```

### Serial Monitor
- Baud rate: **115200**
- Line ending: `LF` or `CR LF`

---

## Project Structure

```
posture_monitor/
├── posture_monitor.ino   # Main firmware entry point
├── config.h              # All configuration (pins, WiFi, MQTT, timing)
├── utils.h               # Logging macros and utility functions
├── mqtt_handler.h        # WiFi + MQTT connection and messaging
├── k230_parser.h         # K230 UART data parsing
├── mode_manager.h        # System mode switching (EC11 encoder)
├── display.h             # OLED display (optional)
├── sensors.h             # PIR + light sensors (optional)
├── voice.h               # SYN-6288 voice synthesis (optional)
└── alerts.h              # WS2812 LED + buzzer alerts
```

---

## Code Style Guidelines

### Header Guards
Always use `#ifndef/#define/#endif` pattern:
```c
#ifndef MODULE_NAME_H
#define MODULE_NAME_H
// ... content ...
#endif // MODULE_NAME_H
```

### Include Order
1. Arduino core: `#include <Arduino.h>`
2. External libraries: `#include <WiFi.h>`, `#include <PubSubClient.h>`
3. Project headers: `#include "config.h"` → `#include "utils.h"` → other modules

### Functions
- Use `inline` for header-only implementations
- Static/file-local functions prefix with underscore: `_parseData()`
- Use `IRAM_ATTR` for interrupt service routines

### Logging (from `utils.h`)
```c
LOGV("verbose message: %s", value);  // Verbose (LOG_LEVEL_VERBOSE)
LOGD("debug info: %d", count);        // Debug (LOG_LEVEL_DEBUG)
LOGI("status: connected");            // Info (LOG_LEVEL_INFO)
LOGW("warning: retry %d", attempt);   // Warning (LOG_LEVEL_WARN)
LOGE("error: code %d", errCode);      // Error (LOG_LEVEL_ERROR)
```

Set `DEBUG_ENABLED` in `config.h` to control log verbosity.

---

## Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Functions | camelCase | `mqtt_connect()`, `k230_read()` |
| Constants/Macros | SCREAMING_SNAKE_CASE | `K230_UART_BAUD`, `ENABLE_OLED` |
| Types/Enums | PascalCase | `SystemMode`, `K230Data`, `LogLevel` |
| Static variables | underscore prefix | `_k230Data`, `_currentMode` |
| GPIO pins | `_PIN` suffix | `WS2812_PIN`, `OLED_SDA_PIN` |

---

## Conditional Compilation Pattern

Modules are enabled/disabled via `config.h`:

```c
// In config.h
#define ENABLE_OLED     0   // 0 = disabled (stub), 1 = enabled

// In module header
inline void display_init() {
#if ENABLE_OLED
    // Actual implementation
    LOGI("OLED initialized");
#else
    // Stub - does nothing but logs
    LOGI("OLED not enabled (ENABLE_OLED=0)");
#endif
}
```

**Always** provide stub implementations when `ENABLE_*=0` to ensure compilation succeeds.

---

## JSON Handling

**Do NOT use ArduinoJson library.** Use `strstr()`, `sprintf()`, and string parsing:

```c
// Parsing (use strstr)
const char* value = strstr(json, "\"key\":\"");
if (value != NULL) {
    value += 8;  // Skip "key":"
    // Extract value...
}

// Building (use sprintf)
char jsonBuf[256];
sprintf(jsonBuf, "{\"id\":\"%u\",\"params\":%s}", msgId, params);
```

---

## MQTT Best Practices

1. **Use 2-parameter `publish()`**: `mqttClient.publish(topic, payload)`
2. **Do NOT call**: `setBufferSize()`, `setKeepAlive()`, `setSocketTimeout()`
3. **Message ID**: Simple incrementing integer (`_postMsgId++`)
4. **Topics**: Defined in `config.h` with `TOPIC_` prefix

---

## Timing and Non-Blocking Code

Always use non-blocking patterns in `loop()`:

```c
void loop() {
    unsigned long now = millis();
    
    // Non-blocking timer
    if (now - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        lastUpdateTime = now;
        doSomething();
    }
    
    // Short delay to prevent CPU hogging
    delay(10);
}
```

---

## Error Handling

- Return `bool` for success/failure
- Log errors with `LOGE()` before returning
- Provide sensible defaults in stub implementations
- Never leave catch blocks empty

---

## Documentation

Use Doxygen-style comments for public functions:

```c
/**
 * @brief Initialize the K230 UART communication
 * 
 * Uses hardware UART1 with configurable TX/RX pins.
 * 
 * @return true if initialization successful
 */
inline bool k230_init();
```

---

## Key Constraints

1. **GPIO Restrictions**: Avoid GPIO35/36/37 (PSRAM), GPIO19/20 (USB), GPIO0 (Boot), GPIO46 (LOG), GPIO43/44 (UART0 debug)
2. **Memory**: ESP32-S3 has limited RAM; avoid large static buffers
3. **WiFi**: Only 2.4GHz networks supported
4. **SNTP**: Built-in ESP32 SNTP for time sync (config in `config.h`)

---

## File Naming

- Header files: `lowercase.h`
- All modules are header-only (`.h` files, no `.cpp` separation)
- Main sketch: `posture_monitor.ino`

---

## Git Commit Convention

- Commit messages **must be written in Chinese**.
- Keep commit subjects concise and action-oriented.
- Prefer one clear intent per commit (feature, fix, docs, refactor, etc.).

---

## When Adding New Features

1. Add configuration constants to `config.h`
2. Add `ENABLE_*` macro for optional features
3. Create new header file following existing patterns
4. Include in `posture_monitor.ino` after `utils.h`
5. Initialize in `setup()`, update in `loop()`
6. Provide stub implementation when disabled
