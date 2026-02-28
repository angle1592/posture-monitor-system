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
// 内部状态
// =============================================================================

static bool _pirPresent = true;         // PIR 检测结果
static int _lightLevel = 2000;          // 光线强度 0~4095

// =============================================================================
// 公共接口
// =============================================================================

/**
 * @brief 初始化传感器
 */
inline void sensors_init() {
#if ENABLE_PIR
    LOGI("PIR 传感器初始化: GPIO%d", PIR_PIN);
    pinMode(PIR_PIN, INPUT);
#else
    LOGI("PIR 未启用（ENABLE_PIR=0），默认: 有人");
#endif

#if ENABLE_LIGHT_SENSOR
    LOGI("光敏传感器初始化: GPIO%d (ADC)", LIGHT_SENSOR_PIN);
    // ESP32 ADC 默认配置即可，无需额外初始化
#else
    LOGI("光敏传感器未启用（ENABLE_LIGHT_SENSOR=0），默认: 2000");
#endif
}

/**
 * @brief 更新传感器读数（在 loop 中定时调用）
 */
inline void sensors_update() {
#if ENABLE_PIR
    _pirPresent = digitalRead(PIR_PIN) == HIGH;
#endif

#if ENABLE_LIGHT_SENSOR
    _lightLevel = analogRead(LIGHT_SENSOR_PIN);
#endif
}

/**
 * @brief 是否检测到有人
 * @return true 有人在（ENABLE_PIR=0 时始终返回 true）
 */
inline bool sensors_isPersonPresent() {
    // PIR 关闭时保持 true，确保“无外设”场景仍可走主检测链路。
    return _pirPresent;
}

/**
 * @brief 获取环境光线强度
 * @return 0~4095（0=暗，4095=亮）（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
inline int sensors_getLightLevel() {
    return _lightLevel;
}

#endif // SENSORS_H
