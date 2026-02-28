#include "alerts.h"

/**
 * @brief alerts.cpp 实现说明
 *
 * 实现要点：
 * - 通过统一入口 alerts_update() 按优先级映射状态到 LED 颜色/闪烁策略。
 * - 蜂鸣器采用“脉冲到时自动关闭”逻辑，避免阻塞式 delay 控制。
 * - 支持指示灯锁定模式，便于产测或调试时稳定观察 WS2812 输出。
 *
 * 内部状态变量含义：
 * - _alertLastBlink/_alertBlinkState：异常状态下红灯闪烁节拍。
 * - _alertModeMask：报警模式位（LED/蜂鸣/语音）。
 * - _indicatorLocked + _indicatorLockRGB：指示灯锁定状态与锁定颜色。
 * - _buzzerActive/_buzzerUntilMs：蜂鸣器脉冲活动与结束时刻。
 */

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
    // 板载状态灯只支持亮灭，按 RGB 是否全 0 做映射。
    bool on = (r | g | b) != 0;
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#endif
#if ENABLE_WS2812
    // WS2812 支持真彩显示，用于表达更丰富状态（红/绿/灭/闪烁）。
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
        // 模式位关闭蜂鸣时，忽略触发请求。
        return;
    }
    // 记录结束时间并立即拉高引脚，后续在 update 中自动回落。
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
        // 到时关闭蜂鸣器，形成“非阻塞脉冲”。
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
        // 灯光模式关闭时强制灭灯，不再继续后续颜色状态判定。
        _alertsSetIndicatorColor(0, 0, 0);
        return;
    }

    // 颜色映射优先级（高 -> 低）：
    // 1) MQTT 断连：红色常亮（链路故障）
    // 2) K230 无效或无人：灭灯（无有效姿态）
    // 3) 姿态异常：红色闪烁（提醒纠正）
    // 4) 姿态正常：绿色常亮（状态正常）
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
        // 300ms 翻转一次闪烁态：兼顾可见性与不过度刺眼。
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
    // 统一关闭可见/可听输出，供模式切换或系统收尾阶段调用。
    _alertsSetIndicatorColor(0, 0, 0);
#if ENABLE_BUZZER
    _buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
#endif
}
