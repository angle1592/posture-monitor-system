#ifndef TIMER_FSM_H
#define TIMER_FSM_H

#include <Arduino.h>
#include "config.h"
#include "k230_parser.h"

// =============================================================================
// 枚举与结构体定义
// =============================================================================

enum TimerState {
    TIMER_IDLE = 0,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE
};

enum AlertPolicyState {
    ALERT_POLICY_IDLE = 0,
    ALERT_POLICY_COOLDOWN
};

typedef struct {
    uint8_t alertModeMask;
    unsigned long cooldownMs;
    unsigned long timerDurationSec;
    unsigned long cfgVersion;
} RuntimeConfig;

// =============================================================================
// 函数声明
// =============================================================================

void timer_init();
void timer_tick(unsigned long now);
void timer_alertPolicyTick(unsigned long now, const K230Data* data, bool monitoringEnabled, bool testForcePosture, bool testForceAbnormal);
void timer_applyConfig();

// Timer state accessors
TimerState timer_getState();
unsigned long timer_getRemainSec();
void timer_start();
void timer_pause();
void timer_resume();
void timer_reset();
bool timer_isAdjustMode();
void timer_setAdjustMode(bool enabled);
void timer_adjustDuration(int deltaMins);

// RuntimeConfig accessors
RuntimeConfig* timer_getConfig();
void timer_setAlertModeMask(uint8_t mask);
void timer_setCooldownMs(unsigned long ms);
void timer_setTimerDurationSec(unsigned long sec);
void timer_setTimerRunning(bool running);

#endif // TIMER_FSM_H
