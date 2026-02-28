#include "alerts.h"

static unsigned long _alertLastBlink = 0;
static bool _alertBlinkState = false;
static uint8_t _alertModeMask = (ALERT_MODE_LED | ALERT_MODE_BUZZER | ALERT_MODE_VOICE);
static bool _indicatorLocked = false;
static uint8_t _indicatorLockR = 0;
static uint8_t _indicatorLockG = 0;
static uint8_t _indicatorLockB = 0;

#if ENABLE_BUZZER
static bool _buzzerActive = false;
static unsigned long _buzzerUntilMs = 0;
#endif

static void _alertsSetIndicatorColor(uint8_t r, uint8_t g, uint8_t b) {
#if STATUS_LED_PIN >= 0
    bool on = (r | g | b) != 0;
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#endif
#if ENABLE_WS2812
    neopixelWrite(WS2812_PIN, r, g, b);
#endif
}

void alerts_init() {
#if STATUS_LED_PIN >= 0
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
#endif

#if ENABLE_WS2812
    _alertsSetIndicatorColor(0, 0, 0);
#endif

#if ENABLE_BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
#endif

    _alertLastBlink = 0;
    _alertBlinkState = false;
    _alertModeMask = (ALERT_MODE_LED | ALERT_MODE_BUZZER | ALERT_MODE_VOICE);
    LOGI("报警模块初始化完成");
}

void alerts_setAlertMode(uint8_t modeMask) {
    _alertModeMask = modeMask;
}

uint8_t alerts_getAlertMode() {
    return _alertModeMask;
}

void alerts_setColor(uint8_t r, uint8_t g, uint8_t b) {
    _alertsSetIndicatorColor(r, g, b);
}

void alerts_lockIndicator(bool lock, uint8_t r, uint8_t g, uint8_t b) {
    // 测试命令需要锁定指示灯，避免被 alerts_update 的自动状态刷新立即覆盖。
    _indicatorLocked = lock;
    _indicatorLockR = r;
    _indicatorLockG = g;
    _indicatorLockB = b;
    if (lock) {
        _alertsSetIndicatorColor(r, g, b);
    }
}

void alerts_triggerBuzzerPulse(unsigned long durationMs) {
#if ENABLE_BUZZER
    if ((_alertModeMask & ALERT_MODE_BUZZER) == 0) {
        return;
    }
    _buzzerActive = true;
    _buzzerUntilMs = millis() + durationMs;
    digitalWrite(BUZZER_PIN, HIGH);
#else
    (void)durationMs;
#endif
}

void alerts_beep(int durationMs) {
    alerts_triggerBuzzerPulse((unsigned long)durationMs);
}

bool alerts_voiceEnabled() {
    return (_alertModeMask & ALERT_MODE_VOICE) != 0;
}

void alerts_update(bool mqttConnected, bool k230Valid, bool isAbnormal, bool noPerson) {
#if ENABLE_BUZZER
    if (_buzzerActive && millis() >= _buzzerUntilMs) {
        _buzzerActive = false;
        digitalWrite(BUZZER_PIN, LOW);
    }
#endif

    if (_indicatorLocked) {
        // 锁定期间持续输出同一颜色，便于肉眼确认板载WS2812工作状态。
        _alertsSetIndicatorColor(_indicatorLockR, _indicatorLockG, _indicatorLockB);
        return;
    }

    if ((_alertModeMask & ALERT_MODE_LED) == 0) {
        _alertsSetIndicatorColor(0, 0, 0);
        return;
    }

    if (!mqttConnected) {
        // 红色常亮优先级最高：先暴露链路故障，再谈姿态状态。
        _alertsSetIndicatorColor(64, 0, 0);
        return;
    }

    if (!k230Valid || noPerson) {
        _alertsSetIndicatorColor(0, 0, 0);
        return;
    }

    if (isAbnormal) {
        unsigned long now = millis();
        if (now - _alertLastBlink >= 300) {
            _alertLastBlink = now;
            _alertBlinkState = !_alertBlinkState;
#if STATUS_LED_PIN >= 0
            digitalWrite(STATUS_LED_PIN, _alertBlinkState ? HIGH : LOW);
#endif
            if (_alertBlinkState) {
                _alertsSetIndicatorColor(64, 0, 0);
            } else {
                _alertsSetIndicatorColor(0, 0, 0);
            }
        }
        return;
    }

    _alertsSetIndicatorColor(0, 32, 0);
}

void alerts_off() {
    _indicatorLocked = false;
    _alertsSetIndicatorColor(0, 0, 0);
#if ENABLE_BUZZER
    _buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
#endif
}