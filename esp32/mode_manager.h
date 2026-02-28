#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

enum ModeClickEvent {
    MODE_CLICK_NONE = 0,
    MODE_CLICK_SHORT,
    MODE_CLICK_LONG
};

static SystemMode _currentMode = MODE_POSTURE;
static bool _modeChanged = false;
static bool _rotationLocked = false;
static int _pendingDelta = 0;
static ModeClickEvent _pendingClick = MODE_CLICK_NONE;

inline const char* mode_getName();

#if ENABLE_EC11
static volatile int _ec11RawDelta = 0;
static int _ec11LastClk = HIGH;
static bool _btnRawPressed = false;
static bool _btnStablePressed = false;
static unsigned long _btnLastChangeMs = 0;
static unsigned long _btnPressStartMs = 0;

static void IRAM_ATTR _ec11ISR() {
    int clkState = digitalRead(EC11_S1_PIN);
    if (clkState != _ec11LastClk && clkState == LOW) {
        // 仅在 CLK 下降沿判向，降低抖动下的重复计数概率。
        if (digitalRead(EC11_S2_PIN) != clkState) {
            _ec11RawDelta++;
        } else {
            _ec11RawDelta--;
        }
    }
    _ec11LastClk = clkState;
}
#endif

inline void mode_init() {
    _currentMode = MODE_POSTURE;
    _modeChanged = false;
    _rotationLocked = false;
    _pendingDelta = 0;
    _pendingClick = MODE_CLICK_NONE;

#if ENABLE_EC11
    pinMode(EC11_S1_PIN, INPUT_PULLUP);
    pinMode(EC11_S2_PIN, INPUT_PULLUP);
    pinMode(EC11_KEY_PIN, INPUT_PULLUP);

    _ec11LastClk = digitalRead(EC11_S1_PIN);
    _ec11RawDelta = 0;
    _btnRawPressed = digitalRead(EC11_KEY_PIN) == LOW;
    _btnStablePressed = _btnRawPressed;
    _btnLastChangeMs = millis();
    _btnPressStartMs = 0;

    attachInterrupt(digitalPinToInterrupt(EC11_S1_PIN), _ec11ISR, CHANGE);
    LOGI("EC11 初始化完成");
#else
    LOGI("EC11 未启用（ENABLE_EC11=0）");
#endif
}

inline void mode_update() {
    _modeChanged = false;

#if ENABLE_EC11
    noInterrupts();
    int delta = _ec11RawDelta;
    _ec11RawDelta = 0;
    interrupts();

    if (delta != 0) {
        if (_rotationLocked) {
            // 锁定时不改模式，把旋转量缓存给上层做参数调节（如定时时长）。
            _pendingDelta += delta;
        } else {
            int nextMode = (int)_currentMode;
            if (delta > 0) {
                nextMode = (nextMode + 1) % MODE_COUNT;
            } else {
                nextMode = (nextMode + MODE_COUNT - 1) % MODE_COUNT;
            }

            if (nextMode != (int)_currentMode) {
                _currentMode = (SystemMode)nextMode;
                _modeChanged = true;
                LOGI("[模式切换] -> %s", mode_getName());
            }
        }
    }

    unsigned long now = millis();
    bool rawPressed = digitalRead(EC11_KEY_PIN) == LOW;
    if (rawPressed != _btnRawPressed) {
        _btnRawPressed = rawPressed;
        _btnLastChangeMs = now;
    }

    if ((now - _btnLastChangeMs) >= EC11_DEBOUNCE_MS) {
        if (_btnStablePressed != _btnRawPressed) {
            _btnStablePressed = _btnRawPressed;
            if (_btnStablePressed) {
                _btnPressStartMs = now;
            } else {
                // 在按键释放时判短按/长按，避免按下瞬间误触发动作。
                unsigned long pressMs = now - _btnPressStartMs;
                if (pressMs >= EC11_LONG_PRESS_MS) {
                    _pendingClick = MODE_CLICK_LONG;
                } else {
                    _pendingClick = MODE_CLICK_SHORT;
                }
            }
        }
    }
#endif
}

inline SystemMode mode_getCurrent() {
    return _currentMode;
}

inline bool mode_setCurrent(SystemMode mode) {
    if ((int)mode < MODE_POSTURE || (int)mode >= MODE_COUNT) {
        return false;
    }
    if (_currentMode != mode) {
        _currentMode = mode;
        _modeChanged = true;
        LOGI("[模式切换] -> %s", mode_getName());
    }
    return true;
}

inline bool mode_hasChanged() {
    return _modeChanged;
}

inline const char* mode_getName() {
    switch (_currentMode) {
        case MODE_POSTURE: return "坐姿检测";
        case MODE_CLOCK: return "时钟";
        case MODE_TIMER: return "定时器";
        default: return "未知";
    }
}

inline void mode_setRotationLocked(bool locked) {
    _rotationLocked = locked;
    if (!locked) {
        _pendingDelta = 0;
    }
}

inline int mode_takeRotationDelta() {
    int delta = _pendingDelta;
    _pendingDelta = 0;
    return delta;
}

inline ModeClickEvent mode_takeClickEvent() {
    ModeClickEvent click = _pendingClick;
    _pendingClick = MODE_CLICK_NONE;
    return click;
}

#endif // MODE_MANAGER_H
