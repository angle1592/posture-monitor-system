#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @brief OLED 显示模块（可选）
 *
 * 模块职责：
 * - 在 OLED 上显示网络状态、姿态状态、时钟与定时器信息。
 * - 提供短时消息覆盖显示（如提示语、状态通知）。
 *
 * 对外提供的核心 API：
 * - display_init() / display_update()
 * - display_setConnectivity() / display_setTimerStatus()
 * - display_showSplash() / display_showMessage()
 *
 * 依赖关系：
 * - ENABLE_OLED=1 时依赖 Wire + U8g2lib。
 * - 依赖 config.h 中的 OLED 引脚与模式枚举常量。
 *
 * 是否可选：
 * - 受 ENABLE_OLED 控制。
 * - ENABLE_OLED=0 时接口保留但不驱动实际硬件（stub 行为）。
 */

#include <Arduino.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "utils.h"

#if ENABLE_OLED
#include <Wire.h>
#include <U8g2lib.h>
#define DISPLAY_U8G2_AVAILABLE 1
#endif

#define DISPLAY_TIMER_IDLE    0
#define DISPLAY_TIMER_RUNNING 1
#define DISPLAY_TIMER_PAUSED  2
#define DISPLAY_TIMER_DONE    3

// 更新连接状态（WiFi/MQTT），供下次渲染使用。
void display_setConnectivity(bool wifiConnected, bool mqttConnected);
// 更新定时器剩余时间和运行状态，供定时器页面显示。
void display_setTimerStatus(int remainSec, int totalSec, int timerState);
// 初始化 OLED 与绘图库（若启用）。
void display_init();
// 周期刷新显示内容：根据 mode 渲染对应页面。
void display_update(int mode, const char* postureType, bool isAbnormal);
// 显示启动页（品牌/版本）。
void display_showSplash();
// 显示短消息窗口，覆盖主界面一段时间。
void display_showMessage(const char* line1, const char* line2 = NULL);

#endif // DISPLAY_H
