#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

/**
 * @brief 模式管理模块（EC11 输入 + 系统模式切换）
 *
 * 模块职责：
 * - 管理系统当前模式（坐姿/时钟/定时器）。
 * - 读取 EC11 旋转编码器与按键事件，并转换为可消费的高层事件。
 * - 支持“旋转锁定”机制：锁定时旋转用于参数调节而非切换模式。
 *
 * 对外提供的核心 API：
 * - mode_init() / mode_update()
 * - mode_getCurrent() / mode_setCurrent() / mode_hasChanged() / mode_getName()
 * - mode_setRotationLocked() / mode_takeRotationDelta() / mode_takeClickEvent()
 *
 * 依赖关系：
 * - config.h（SystemMode 枚举、EC11 引脚、消抖与长按阈值）
 * - Arduino GPIO / 中断接口
 * - utils.h（日志输出）
 *
 * 是否可选：
 * - EC11 输入路径受 ENABLE_EC11 控制。
 * - ENABLE_EC11=0 时模块仍可工作，但仅支持软件侧设置模式。
 */

#include <Arduino.h>
#include "config.h"
#include "utils.h"

enum ModeClickEvent {
    MODE_CLICK_NONE = 0,
    MODE_CLICK_SHORT,
    MODE_CLICK_LONG
};

// 事件输入初始化：配置引脚、中断和按键状态机初值。
void mode_init();
// 周期更新：消费编码器脉冲、处理按键消抖，并生成模式变化/点击事件。
void mode_update();
// 获取当前系统模式。
SystemMode mode_getCurrent();
// 主动设置模式（带范围校验）。
bool mode_setCurrent(SystemMode mode);
// 查询本轮更新是否发生模式切换。
bool mode_hasChanged();
// 获取当前模式的人类可读名称。
const char* mode_getName();
// 开关旋转锁：锁定后旋转不切模式，改为累积调节增量。
void mode_setRotationLocked(bool locked);
bool mode_isRotationLocked();
// 读取并清空累计旋转增量（一次性消费）。
int mode_takeRotationDelta();
// 读取并清空按键点击事件（短按/长按）。
ModeClickEvent mode_takeClickEvent();
int mode_getKeyLevel();
bool mode_getRawPressed();
bool mode_getStablePressed();
ModeClickEvent mode_peekClickEvent();
int mode_getRawDeltaSnapshot();
int mode_getPendingDeltaSnapshot();

#endif // MODE_MANAGER_H
