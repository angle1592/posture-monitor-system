#ifndef TIMER_FSM_H
#define TIMER_FSM_H

#include <Arduino.h>
#include "config.h"
#include "k230_parser.h"

// =============================================================================
// 枚举与结构体定义
// =============================================================================

/*
 * 定时器主状态机：
 * IDLE -> RUNNING -> PAUSED / DONE。
 * 这样拆分的原因是把“是否在计时”和“是否已完成”区分开，
 * 便于 UI 与云端分别做不同反馈。
 */
enum TimerState {
    TIMER_IDLE = 0,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE
};

/*
 * 提醒策略状态机：
 * IDLE 表示当前允许触发提醒，COOLDOWN 表示处于提醒冷却期。
 * 与 TimerState 分离，避免“计时器流程”和“提醒节流流程”相互耦合。
 */
enum AlertPolicyState {
    ALERT_POLICY_IDLE = 0,
    ALERT_POLICY_COOLDOWN
};

/*
 * 运行期可调配置：
 * 这些值可由云端/本地输入修改，并通过 cfgVersion 递增反映配置变更。
 */
typedef struct {
    uint8_t alertModeMask;           // 提醒输出位掩码：LED/蜂鸣器/语音
    unsigned long cooldownMs;        // 异常提醒冷却时间，防止短时间重复播报
    unsigned long timerDurationSec;  // 定时器总时长（秒）
    unsigned long cfgVersion;        // 配置版本号，每次参数变更自增
} RuntimeConfig;

// =============================================================================
// 函数声明
// =============================================================================

/**
 * @brief 初始化定时器与提醒策略状态。
 * @details 在系统启动时统一归位状态，避免上电残留值影响首次运行。
 */
void timer_init();

/**
 * @brief 推进定时器状态机并处理倒计时。
 * @details 之所以要求外部传入 now，是为了统一用主循环时基，减少重复调用 millis() 的抖动。
 * @param now 当前系统毫秒时间戳（通常为 millis()）
 */
void timer_tick(unsigned long now);

/**
 * @brief 根据当前姿态与冷却策略决定是否触发提醒。
 * @details 该函数将“异常判定”与“提醒节流”集中管理，避免多处重复判断导致行为不一致。
 * @param now 当前系统毫秒时间戳
 * @param data 最新 K230 数据帧
 * @param monitoringEnabled 坐姿检测总开关
 * @param testForcePosture 是否启用测试注入
 * @param testForceAbnormal 测试注入时是否强制异常
 * @param noPerson 当前是否可判定为无人
 */
void timer_alertPolicyTick(unsigned long now, const K230Data* data, bool monitoringEnabled, bool testForcePosture, bool testForceAbnormal, bool noPerson);

/**
 * @brief 将运行时配置同步到提醒执行层。
 * @details 配置变更后显式调用，保证 alertModeMask 能立即生效。
 */
void timer_applyConfig();

// Timer state accessors

/**
 * @brief 获取当前定时器状态。
 * @return TimerState 当前状态枚举值
 */
TimerState timer_getState();

/**
 * @brief 获取当前剩余秒数。
 * @return unsigned long 剩余时间（秒）
 */
unsigned long timer_getRemainSec();

/**
 * @brief 启动定时器。
 * @details 启动时会重置剩余时间为配置时长，并计算绝对结束时间。
 */
void timer_start();

/**
 * @brief 暂停定时器。
 * @details 暂停时先把绝对结束时间换算成剩余秒数，便于后续恢复。
 */
void timer_pause();

/**
 * @brief 恢复定时器。
 * @details 恢复时基于当前剩余秒数重新计算绝对结束时间。
 */
void timer_resume();

/**
 * @brief 重置定时器到空闲态。
 * @details 用配置时长覆盖剩余时间，确保下一次 start 行为可预测。
 */
void timer_reset();

/**
 * @brief 查询是否处于“旋钮调时”模式。
 * @return true 表示旋钮输入优先用于修改时长
 */
bool timer_isAdjustMode();

/**
 * @brief 设置“旋钮调时”模式开关。
 * @param enabled true 开启调时模式，false 关闭
 */
void timer_setAdjustMode(bool enabled);

/**
 * @brief 按分钟增量调整定时总时长。
 * @details 统一在此函数做上下限约束，避免不同入口出现不一致。
 * @param deltaMins 分钟增量，可为负值
 */
void timer_adjustDuration(int deltaMins);

// RuntimeConfig accessors

/**
 * @brief 获取运行时配置对象指针。
 * @return RuntimeConfig* 可读写配置指针
 */
RuntimeConfig* timer_getConfig();

/**
 * @brief 设置提醒输出位掩码。
 * @param mask bit0=LED, bit1=蜂鸣器, bit2=语音
 */
void timer_setAlertModeMask(uint8_t mask);

/**
 * @brief 设置提醒冷却时间。
 * @param ms 冷却时长（毫秒）
 */
void timer_setCooldownMs(unsigned long ms);

/**
 * @brief 设置定时器总时长。
 * @param sec 总时长（秒）
 */
void timer_setTimerDurationSec(unsigned long sec);

/**
 * @brief 统一入口设置定时器“运行/暂停”状态。
 * @details 主要给云端属性控制使用，避免直接改内部状态破坏一致性。
 * @param running true 表示进入运行态，false 表示暂停运行
 */
void timer_setTimerRunning(bool running);

#endif // TIMER_FSM_H
