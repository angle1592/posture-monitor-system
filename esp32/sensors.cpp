#include "sensors.h"

/**
 * @brief sensors.cpp 实现说明
 *
 * 实现要点：
 * - 根据 ENABLE_PIR / ENABLE_LIGHT_SENSOR 分别编译真实采样或 stub 路径。
 * - 采样逻辑保持轻量：PIR 读数字电平，光照读 ADC 原始值。
 * - 对上层始终返回可用数据，避免可选硬件缺失时主流程中断。
 *
 * 内部状态变量含义：
 * - _pirPresent：最近一次人体检测结果。
 * - _lightLevel：最近一次光照 ADC 值（0~4095）。
 */

// =============================================================================
// 内部状态
// =============================================================================

static bool _pirPresent = true;         // PIR 检测结果
static int _lightLevel = 2000;          // 光线强度 0~4095

/**
 * @brief 初始化传感器
 */
void sensors_init() {
#if ENABLE_PIR
    // 启用 PIR 时按数字输入模式读取高低电平。
    LOGI("PIR 传感器初始化: GPIO%d", PIR_PIN);
    pinMode(PIR_PIN, INPUT);
#else
    // 未启用时保持默认“有人”，防止误触发无人分支导致功能不可用。
    LOGI("PIR 未启用（ENABLE_PIR=0），默认: 有人");
#endif

#if ENABLE_LIGHT_SENSOR
    LOGI("光敏传感器初始化: GPIO%d (ADC)", LIGHT_SENSOR_PIN);
    // ESP32 ADC 默认配置即可，无需额外初始化
#else
    // 未启用时返回中值，方便上层在无光敏硬件时仍可运行阈值逻辑。
    LOGI("光敏传感器未启用（ENABLE_LIGHT_SENSOR=0），默认: 2000");
#endif
}

/**
 * @brief 更新传感器读数（在 loop 中定时调用）
 */
void sensors_update() {
#if ENABLE_PIR
    // PIR 常见输出为 HIGH=检测到人体，LOW=无人。
    _pirPresent = digitalRead(PIR_PIN) == HIGH;
#endif

#if ENABLE_LIGHT_SENSOR
    // ADC 原始值直接透传，上层可按场景自行做滤波与阈值判断。
    _lightLevel = analogRead(LIGHT_SENSOR_PIN);
#endif
}

/**
 * @brief 是否检测到有人
 * @return true 有人在（ENABLE_PIR=0 时始终返回 true）
 */
bool sensors_isPersonPresent() {
    // PIR 关闭时保持 true，确保"无外设"场景仍可走主检测链路。
    return _pirPresent;
}

/**
 * @brief 获取环境光线强度
 * @return 0~4095（0=暗，4095=亮）（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
int sensors_getLightLevel() {
    return _lightLevel;
}
