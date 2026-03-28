#include "sensors.h"

#if ENABLE_LIGHT_SENSOR
#include <Wire.h>
#endif

/**
 * @brief sensors.cpp 实现说明
 *
 * 实现要点：
 * - 根据 ENABLE_PIR / ENABLE_LIGHT_SENSOR 分别编译真实采样或 stub 路径。
 * - 采样逻辑保持轻量：PIR 读数字电平，BH1750 读 I2C lux。
 * - 对上层始终返回可用数据，避免可选硬件缺失时主流程中断。
 *
 * 内部状态变量含义：
 * - _pirPresent：最近一次人体检测结果。
 * - _lightLevel：最近一次光照 lux 值。
 */

// =============================================================================
// 内部状态
// =============================================================================

static bool _pirPresent = true;
static bool _pirReady = true;
static unsigned long _pirBootMs = 0;
static int _lightLevel = 2000;
static bool _lightReady = true;

#if ENABLE_LIGHT_SENSOR
namespace {
constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONT_HIGH_RES = 0x10;

bool sendBh1750Command(uint8_t command) {
    Wire.beginTransmission(BH1750_I2C_ADDR);
    Wire.write(command);
    return Wire.endTransmission() == 0;
}

bool initBh1750() {
    if (!sendBh1750Command(BH1750_POWER_ON)) {
        LOGE("BH1750 上电命令失败: addr=0x%02X", BH1750_I2C_ADDR);
        return false;
    }
    if (!sendBh1750Command(BH1750_CONT_HIGH_RES)) {
        LOGE("BH1750 模式设置失败: addr=0x%02X", BH1750_I2C_ADDR);
        return false;
    }
    delay(BH1750_MEASUREMENT_DELAY_MS);
    return true;
}

bool readBh1750Lux(int* luxOut) {
    if (luxOut == NULL) {
        return false;
    }

    if (Wire.requestFrom((int)BH1750_I2C_ADDR, 2) != 2) {
        return false;
    }

    uint16_t raw = ((uint16_t)Wire.read() << 8) | (uint16_t)Wire.read();
    *luxOut = (int)((raw + 1) / 1.2f);
    return true;
}
}
#endif

/**
 * @brief 初始化传感器
 */
void sensors_init() {
#if ENABLE_PIR
    LOGI("PIR 传感器初始化: GPIO%d", PIR_PIN);
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    _pirBootMs = millis();
    _pirReady = false;
    _pirPresent = true;
#else
    LOGI("PIR 未启用（ENABLE_PIR=0），默认: 有人");
#endif

#if ENABLE_LIGHT_SENSOR
    LOGI("BH1750 初始化: SDA=GPIO%d SCL=GPIO%d addr=0x%02X", BH1750_SDA_PIN, BH1750_SCL_PIN, BH1750_I2C_ADDR);
    Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
    _lightReady = initBh1750();
    if (!_lightReady) {
        LOGW("BH1750 初始化失败，维持默认 lux=%d", _lightLevel);
    }
#else
    LOGI("光敏传感器未启用（ENABLE_LIGHT_SENSOR=0），默认: 2000");
#endif
}

/**
 * @brief 更新传感器读数（在 loop 中定时调用）
 */
void sensors_update() {
#if ENABLE_PIR
    if (!_pirReady) {
        if (millis() - _pirBootMs >= PIR_WARMUP_MS) {
            _pirReady = true;
            LOGI("PIR 预热完成");
        } else {
            _pirPresent = true;
        }
    }
    if (_pirReady) {
        _pirPresent = digitalRead(PIR_PIN) == HIGH;
    }
#endif

#if ENABLE_LIGHT_SENSOR
    int lux = 0;
    if (readBh1750Lux(&lux)) {
        _lightLevel = lux;
        _lightReady = true;
    } else if (_lightReady) {
        LOGW("BH1750 读取失败，沿用上次 lux=%d", _lightLevel);
        _lightReady = false;
    }
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
 * @return lux 整数值（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
int sensors_getLightLevel() {
    return _lightLevel;
}

bool sensors_isPirReady() {
    return _pirReady;
}

bool sensors_isLightSensorReady() {
    return _lightReady;
}
