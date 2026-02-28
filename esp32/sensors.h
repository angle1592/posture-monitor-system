/**
 * @file sensors.h
 * @brief 传感器模块 — PIR 人体红外 + 光敏传感器
 * 
 * PIR：检测是否有人（数字量，GPIO4）
 * 光敏：检测环境光线强度（模拟量 ADC，GPIO5）
 * 
 * 当 ENABLE_PIR = 0 / ENABLE_LIGHT_SENSOR = 0 时，对应函数为 stub。
 * stub 返回合理的默认值：有人 = true，光线 = 2000（中等亮度）。
 * 
 * @date 2026
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

// =============================================================================
// 公共接口
// =============================================================================

/**
 * @brief 初始化传感器
 */
void sensors_init();

/**
 * @brief 更新传感器读数（在 loop 中定时调用）
 */
void sensors_update();

/**
 * @brief 是否检测到有人
 * @return true 有人在（ENABLE_PIR=0 时始终返回 true）
 */
bool sensors_isPersonPresent();

/**
 * @brief 获取环境光线强度
 * @return 0~4095（0=暗，4095=亮）（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
int sensors_getLightLevel();

#endif // SENSORS_H