#ifndef ALERTS_H
#define ALERTS_H

/**
 * @brief 报警执行模块（WS2812 + 蜂鸣器 + 语音开关）
 *
 * 模块职责：
 * - 根据系统状态驱动 LED 指示与蜂鸣器脉冲。
 * - 维护报警模式掩码（灯光/蜂鸣/语音）和指示灯锁定状态。
 * - 提供统一的对外报警控制接口。
 *
 * 对外提供的核心 API：
 * - alerts_init() / alerts_update() / alerts_off()
 * - alerts_setAlertMode() / alerts_getAlertMode() / alerts_voiceEnabled()
 * - alerts_setColor() / alerts_lockIndicator()
 * - alerts_triggerBuzzerPulse() / alerts_beep()
 *
 * 依赖关系：
 * - config.h（ENABLE_WS2812、ENABLE_BUZZER、引脚定义）
 * - Arduino GPIO 与 neopixelWrite
 * - utils.h（日志）
 *
 * 是否可选：
 * - WS2812 与蜂鸣器分别受 ENABLE_WS2812 / ENABLE_BUZZER 控制。
 * - 语音告警是否启用由模式位 ALERT_MODE_VOICE 决定。
 */

#include <Arduino.h>
#include "config.h"
#include "utils.h"

#define ALERT_MODE_LED      0x01
#define ALERT_MODE_BUZZER   0x02
#define ALERT_MODE_VOICE    0x04

// 初始化报警相关外设与内部状态。
void alerts_init();
// 设置报警模式位掩码（灯光/蜂鸣/语音）。
void alerts_setAlertMode(uint8_t modeMask);
// 读取当前报警模式位掩码。
uint8_t alerts_getAlertMode();
// 直接设置当前指示灯颜色。
void alerts_setColor(uint8_t r, uint8_t g, uint8_t b);
// 锁定或解锁指示灯自动刷新（锁定时保持指定颜色）。
void alerts_lockIndicator(bool lock, uint8_t r, uint8_t g, uint8_t b);
// 触发一个定长的指示灯脉冲，优先级高于普通状态灯。
void alerts_triggerIndicatorPulse(unsigned long durationMs, uint8_t r, uint8_t g, uint8_t b);
// 触发蜂鸣器脉冲一段时间（毫秒）。
void alerts_triggerBuzzerPulse(unsigned long durationMs);
// 简化接口：按给定时长蜂鸣一次。
void alerts_beep(int durationMs);
// 查询当前是否允许语音告警。
bool alerts_voiceEnabled();
// 周期更新报警状态机（颜色映射、闪烁、蜂鸣器到时关闭）。
void alerts_update(bool mqttConnected, bool personPresent, bool shouldAlert);
// 关闭所有报警输出。
void alerts_off();

#endif // ALERTS_H
