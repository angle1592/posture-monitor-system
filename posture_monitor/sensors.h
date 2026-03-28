/**
 * @file sensors.h
 * @brief 传感器模块 — PIR 人体红外 + 光敏传感器
 * 
 * PIR：检测是否有人（数字量，GPIO4）
 * 光敏：检测环境光线强度（BH1750 lux，GPIO1/GPIO2 I2C）
 * 
 * 当 ENABLE_PIR = 0 / ENABLE_LIGHT_SENSOR = 0 时，对应函数为 stub。
 * stub 返回合理的默认值：有人 = true，光线 = 2000（中等亮度）。
 * 
 * @date 2026
 */

#ifndef SENSORS_H
#define SENSORS_H

/**
 * @brief 传感器采集模块（PIR + 光照，可选）
 *
 * 模块职责：
 * - 管理 PIR 人体红外与 BH1750 光照传感器的初始化和周期采样。
 * - 对上层提供统一读数接口，屏蔽硬件启用/禁用差异。
 *
 * 对外提供的核心 API：
 * - sensors_init() / sensors_update()
 * - sensors_isPersonPresent() / sensors_getLightLevel()
 *
 * 依赖关系：
 * - Arduino GPIO/ADC 读写接口
 * - config.h（ENABLE_PIR、ENABLE_LIGHT_SENSOR 与引脚定义）
 * - utils.h（日志输出）
 *
 * 是否可选：
 * - PIR 受 ENABLE_PIR 控制。
 * - 光照传感器受 ENABLE_LIGHT_SENSOR 控制。
 * - 未启用时返回安全默认值，保证主流程可运行。
 */

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
 * @brief 获取环境光线强度（lux）
 * @return lux 整数值（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
int sensors_getLightLevel();

/**
 * @brief PIR 是否已结束预热
 * @return true 预热完成或 PIR 未启用
 */
bool sensors_isPirReady();

/**
 * @brief BH1750 是否已成功初始化并拿到有效读数
 * @return true 可认为 lux 数据有效
 */
bool sensors_isLightSensorReady();

#endif // SENSORS_H
