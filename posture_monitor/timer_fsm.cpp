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

// 计时核心变量：
// - _timerEndMs 保存“绝对结束时刻”，用于抗 loop 抖动；
// - _timerRemainSec 保存“可展示/可恢复”的剩余秒数，便于暂停与云端同步。
static TimerState _timerState = TIMER_IDLE;
static unsigned long _timerEndMs = 0;
static unsigned long _timerRemainSec = TIMER_DEFAULT_DURATION_SEC;
static bool _timerAdjustMode = false;

// 提醒策略独立状态：只负责“要不要提醒”，不直接参与计时器状态迁移。
static AlertPolicyState _alertPolicyState = ALERT_POLICY_IDLE;
static unsigned long _alertCooldownUntilMs = 0;

// =============================================================================
// 核心逻辑
// =============================================================================

/**
 * @brief 初始化定时器与提醒策略状态机。
 * @details 统一归位运行状态，避免复位/重连后沿用旧状态导致行为不一致。
 */
void timer_init() {
    _timerState = TIMER_IDLE;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
    _timerAdjustMode = false;
    _alertPolicyState = ALERT_POLICY_IDLE;
    _alertCooldownUntilMs = 0;
}

/**
 * @brief 推进定时器状态与倒计时。
 * @details 先处理“调时输入”，再处理“计时推进”，这个顺序可确保用户刚旋钮调时时不会被旧状态覆盖。
 * @param now 当前毫秒时间戳（主循环传入）
 */
void timer_tick(unsigned long now) {
    // 仅在“定时器模式 + 调时开启”时消费旋钮增量，防止其他模式误改时长。
    if (mode_getCurrent() == MODE_TIMER && _timerAdjustMode) {
        int delta = mode_takeRotationDelta();
        if (delta != 0) {
            timer_adjustDuration(delta);
        }
    }

    // 只有 RUNNING 才推进倒计时；IDLE/PAUSED/DONE 都不应减少剩余时间。
    if (_timerState != TIMER_RUNNING) {
        return;
    }

    // 使用“绝对结束时刻”反推剩余时间，比每秒递减更抗 loop 抖动与短暂阻塞。
    if (_timerEndMs > now) {
        _timerRemainSec = (_timerEndMs - now) / 1000UL;
        return;
    }

    // 到达或超过结束时刻：只在这里进入 DONE，确保结束行为只触发一次。
    _timerRemainSec = 0;
    _timerState = TIMER_DONE;
    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS * 2);
    if (alerts_voiceEnabled()) {
        voice_speak("定时器结束");
    }
    display_showMessage("Timer done", NULL);
}

/**
 * @brief 按冷却策略决定是否触发坐姿提醒。
 * @details 流程是“先判异常是否成立，再判是否在冷却期”，避免无效提醒。
 * @param now 当前毫秒时间戳
 * @param data K230 当前帧数据
 * @param monitoringEnabled 坐姿检测总开关
 * @param testForcePosture 是否启用测试姿态注入
 * @param testForceAbnormal 测试注入时是否强制异常
 */
void timer_alertPolicyTick(unsigned long now, const K230Data* data, bool monitoringEnabled, bool testForcePosture, bool testForceAbnormal, bool noPerson) {
    bool k230Ok = !k230_isTimeout(K230_TIMEOUT_MS);
    bool dataValid = data->valid;
    bool isAbnormal = data->isAbnormal;

    // 测试注入优先级最高：用于脱离真实相机输入验证提醒链路。
    if (testForcePosture) {
        k230Ok = true;
        noPerson = false;
        dataValid = true;
        isAbnormal = testForceAbnormal;
    }

    // 只有“检测开启 + 链路正常 + 有人 + 数据有效 + 判定异常”才算应提醒。
    bool abnormalNow = monitoringEnabled && k230Ok && !noPerson && dataValid && isAbnormal;

    // 一旦条件不成立，立即回到 IDLE。
    // 这么做的原因是：用户姿态恢复后，下次再次异常应立即允许提醒，不应被旧冷却期阻塞。
    if (!abnormalNow) {
        _alertPolicyState = ALERT_POLICY_IDLE;
        return;
    }

    // 条件成立但仍在冷却窗口：抑制重复提醒，仅维持策略状态。
    if (now < _alertCooldownUntilMs) {
        _alertPolicyState = ALERT_POLICY_COOLDOWN;
        return;
    }

    // 冷却结束且仍异常：触发一次提醒，并开启下一轮冷却。
    alerts_triggerBuzzerPulse(BUZZER_PULSE_MS);
    if (alerts_voiceEnabled()) {
        voice_speak("请调整坐姿");
    }
    _alertCooldownUntilMs = now + _runtimeCfg.cooldownMs;
    _alertPolicyState = ALERT_POLICY_COOLDOWN;
}

/**
 * @brief 应用当前运行时配置到提醒模块。
 * @details 把配置写入执行层，保证 LED/蜂鸣器/语音掩码立即生效。
 */
void timer_applyConfig() {
    alerts_setAlertMode(_runtimeCfg.alertModeMask);
}

// =============================================================================
// 访问器函数
// =============================================================================

/**
 * @brief 获取定时器当前状态。
 * @return TimerState 当前状态
 */
TimerState timer_getState() {
    return _timerState;
}

/**
 * @brief 获取当前剩余秒数。
 * @return unsigned long 剩余秒数
 */
unsigned long timer_getRemainSec() {
    return _timerRemainSec;
}

/**
 * @brief 启动定时器。
 * @details 启动时重置剩余时间并计算 _timerEndMs，避免沿用旧的暂停现场。
 */
void timer_start() {
    _timerState = TIMER_RUNNING;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
    _timerEndMs = millis() + _timerRemainSec * 1000UL;
}

/**
 * @brief 暂停定时器。
 * @details 暂停时把“绝对结束时间”回写为“剩余秒数”，这样 resume 才能从正确位置继续。
 */
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

/**
 * @brief 恢复定时器。
 * @details 恢复时不改剩余时间，只重新计算 _timerEndMs。
 */
void timer_resume() {
    if (_timerState == TIMER_PAUSED) {
        _timerState = TIMER_RUNNING;
        _timerEndMs = millis() + _timerRemainSec * 1000UL;
    }
}

/**
 * @brief 重置定时器到初始空闲状态。
 * @details 该函数用于“停止并回到默认时长”，而不是暂停。
 */
void timer_reset() {
    _timerState = TIMER_IDLE;
    _timerRemainSec = _runtimeCfg.timerDurationSec;
}

/**
 * @brief 查询是否处于调时模式。
 * @return true 表示旋钮输入用于改时长
 */
bool timer_isAdjustMode() {
    return _timerAdjustMode;
}

/**
 * @brief 设置调时模式。
 * @param enabled true 开启，false 关闭
 */
void timer_setAdjustMode(bool enabled) {
    _timerAdjustMode = enabled;
}

/**
 * @brief 按分钟增量调整定时总时长。
 * @details 对上下限统一钳制，并在空闲/完成态同步更新显示剩余值。
 * @param deltaMins 增减分钟数，可正可负
 */
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

/**
 * @brief 获取运行时配置指针。
 * @return RuntimeConfig* 配置对象地址
 */
RuntimeConfig* timer_getConfig() {
    return &_runtimeCfg;
}

/**
 * @brief 设置提醒模式位掩码。
 * @details 用 ALERT_MODE_MASK_ALL 兜底裁剪非法位，避免写入无效输出通道。
 * @param mask 输入掩码
 */
void timer_setAlertModeMask(uint8_t mask) {
    _runtimeCfg.alertModeMask = (mask & ALERT_MODE_MASK_ALL);
    _runtimeCfg.cfgVersion++;
}

/**
 * @brief 设置提醒冷却时间。
 * @details 在入口钳制最小/最大值，防止参数过小导致刷屏提醒，或过大导致长时间静默。
 * @param ms 冷却毫秒数
 */
void timer_setCooldownMs(unsigned long ms) {
    if (ms < COOLDOWN_MIN_MS) ms = COOLDOWN_MIN_MS;
    if (ms > COOLDOWN_MAX_MS) ms = COOLDOWN_MAX_MS;
    _runtimeCfg.cooldownMs = ms;
    _runtimeCfg.cfgVersion++;
}

/**
 * @brief 设置定时器总时长（秒）。
 * @details 仅在 IDLE/DONE 时同步到 _timerRemainSec，避免打断正在 RUNNING/PAUSED 的当前会话。
 * @param sec 目标总时长（秒）
 */
void timer_setTimerDurationSec(unsigned long sec) {
    if (sec < 60) sec = 60;
    if (sec > TIMER_MAX_DURATION_SEC) sec = TIMER_MAX_DURATION_SEC;
    _runtimeCfg.timerDurationSec = sec;
    _runtimeCfg.cfgVersion++;
    if (_timerState == TIMER_IDLE || _timerState == TIMER_DONE) {
        _timerRemainSec = _runtimeCfg.timerDurationSec;
    }
}

/**
 * @brief 统一控制“是否运行计时器”。
 * @details 这是给云端属性控制的门面函数：
 * - running=true：若当前未运行，则按当前剩余时间继续；
 * - running=false：若当前在运行，则转为暂停。
 * @param running 目标运行状态
 */
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
