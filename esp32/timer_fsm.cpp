#include "timer_fsm.h"
#include "utils.h"
#include "alerts.h"
#include "voice.h"
#include "mode_manager.h"
#include "display.h"

// =============================================================================
// 静态变量
// =============================================================================

static RuntimeConfig _runtimeCfg = {
    (ALERT_MODE_LED | ALERT_MODE_BUZZER | ALERT_MODE_VOICE),
    ALERT_COOLDOWN_MS,
    TIMER_DEFAULT_DURATION_SEC,
    1
};

static TimerState _timerState = TIMER_IDLE;
static unsigned long _timerEndMs = 0;
static unsigned long _timerRemainSec = TIMER_DEFAULT_DURATION_SEC;
static bool _timerAdjustMode = false;

static AlertPolicyState _alertPolicyState = ALERT_POLICY_IDLE;
static unsigned long _alertCooldownUntilMs = 0;

// =============================================================================
// 核心逻辑
// =============================================================================

void timer_init() {
    _timerState = TIMER_IDLE;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
    _timerAdjustMode = false;
    _alertPolicyState = ALERT_POLICY_IDLE;
    _alertCooldownUntilMs = 0;
}

void timer_tick(unsigned long now) {
    if (mode_getCurrent() == MODE_TIMER && _timerAdjustMode) {
        int delta = mode_takeRotationDelta();
        if (delta != 0) {
            timer_adjustDuration(delta);
        }
    }

    if (_timerState != TIMER_RUNNING) {
        return;
    }

    if (_timerEndMs > now) {
        _timerRemainSec = (_timerEndMs - now) / 1000UL;
        return;
    }

    _timerRemainSec = 0;
    _timerState = TIMER_DONE;
    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS * 2);
    if (alerts_voiceEnabled()) {
        voice_speak("定时器结束");
    }
    display_showMessage("Timer done", NULL);
}

void timer_alertPolicyTick(unsigned long now, const K230Data* data, bool monitoringEnabled, bool testForcePosture, bool testForceAbnormal) {
    bool k230Ok = !k230_isTimeout(K230_TIMEOUT_MS);
    bool noPerson = (strcmp(data->postureType, "no_person") == 0);
    bool dataValid = data->valid;
    bool isAbnormal = data->isAbnormal;

    if (testForcePosture) {
        k230Ok = true;
        noPerson = false;
        dataValid = true;
        isAbnormal = testForceAbnormal;
    }

    bool abnormalNow = monitoringEnabled && k230Ok && !noPerson && dataValid && isAbnormal;

    if (!abnormalNow) {
        // 条件恢复后立即退出冷却态，保证后续异常能重新触发提醒。
        _alertPolicyState = ALERT_POLICY_IDLE;
        return;
    }

    if (now < _alertCooldownUntilMs) {
        _alertPolicyState = ALERT_POLICY_COOLDOWN;
        return;
    }

    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS);
    if (alerts_voiceEnabled()) {
        voice_speak("请调整坐姿");
    }
    _alertCooldownUntilMs = now + _runtimeCfg.cooldownMs;
    _alertPolicyState = ALERT_POLICY_COOLDOWN;
}

void timer_applyConfig() {
    alerts_setAlertMode(_runtimeCfg.alertModeMask);
}

// =============================================================================
// 访问器函数
// =============================================================================

TimerState timer_getState() {
    return _timerState;
}

unsigned long timer_getRemainSec() {
    return _timerRemainSec;
}

void timer_start() {
    _timerState = TIMER_RUNNING;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
    _timerEndMs = millis() + _timerRemainSec * 1000UL;
}

void timer_pause() {
    if (_timerState == TIMER_RUNNING) {
        unsigned long now = millis();
        if (_timerEndMs > now) {
            _timerRemainSec = (_timerEndMs - now) / 1000UL;
        } else {
            _timerRemainSec = 0;
        }
        _timerState = TIMER_PAUSED;
    }
}

void timer_resume() {
    if (_timerState == TIMER_PAUSED) {
        _timerState = TIMER_RUNNING;
        _timerEndMs = millis() + _timerRemainSec * 1000UL;
    }
}

void timer_reset() {
    _timerState = TIMER_IDLE;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
}

bool timer_isAdjustMode() {
    return _timerAdjustMode;
}

void timer_setAdjustMode(bool enabled) {
    _timerAdjustMode = enabled;
}

void timer_adjustDuration(int deltaMins) {
    long nextSec = (long)_runtimeCfg.timerDurationSec + (long)deltaMins * 60L;
    if (nextSec < 60L) nextSec = 60L;
    if (nextSec > TIMER_MAX_DURATION_SEC) nextSec = TIMER_MAX_DURATION_SEC;
    _runtimeCfg.timerDurationSec = (unsigned long)nextSec;
    _runtimeCfg.cfgVersion++;
    if (_timerState == TIMER_IDLE || _timerState == TIMER_DONE) {
        _timerRemainSec = _runtimeCfg.timerDurationSec;
    }
    LOGI("[EC11] timerDurationSec=%lu (delta=%d)", _runtimeCfg.timerDurationSec, deltaMins);
}

RuntimeConfig* timer_getConfig() {
    return &_runtimeCfg;
}

void timer_setAlertModeMask(uint8_t mask) {
    _runtimeCfg.alertModeMask = (mask & ALERT_MODE_MASK_ALL);
    _runtimeCfg.cfgVersion++;
}

void timer_setCooldownMs(unsigned long ms) {
    if (ms < COOLDOWN_MIN_MS) ms = COOLDOWN_MIN_MS;
    if (ms > COOLDOWN_MAX_MS) ms = COOLDOWN_MAX_MS;
    _runtimeCfg.cooldownMs = ms;
    _runtimeCfg.cfgVersion++;
}

void timer_setTimerDurationSec(unsigned long sec) {
    if (sec < 60) sec = 60;
    if (sec > TIMER_MAX_DURATION_SEC) sec = TIMER_MAX_DURATION_SEC;
    _runtimeCfg.timerDurationSec = sec;
    _runtimeCfg.cfgVersion++;
    if (_timerState == TIMER_IDLE || _timerState == TIMER_DONE) {
        _timerRemainSec = _runtimeCfg.timerDurationSec;
    }
}

void timer_setTimerRunning(bool running) {
    if (running) {
        if (_timerState != TIMER_RUNNING) {
            _timerState = TIMER_RUNNING;
            _timerEndMs = millis() + _timerRemainSec * 1000UL;
        }
    } else {
        if (_timerState == TIMER_RUNNING) {
            timer_pause();
        }
    }
}
