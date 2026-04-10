#include "sensors.h"

#if ENABLE_LIGHT_SENSOR
#include <Wire.h>
#endif

/**
 * @brief sensors.cpp 实现说明
 *
 * 实现要点：
 * - 根据 ENABLE_PIR / ENABLE_LIGHT_SENSOR 分别编译真实采样或 stub 路径。
 * - 人体存在传感器支持 PIR 与对射式红外两种模式。
 * - 采样逻辑保持轻量：存在传感器读数字电平，BH1750 读 I2C lux。
 * - 对上层始终返回可用数据，避免可选硬件缺失时主流程中断。
 *
 * 内部状态变量含义：
 * - _presencePresent：最近一次人体存在结果。
 * - _lightLevel：最近一次光照 lux 值。
 */

// =============================================================================
// 内部状态
// =============================================================================

static bool _presencePresent = true;
static bool _presenceReady = true;
static unsigned long _presenceBootMs = 0;
static unsigned long _presenceLastActiveMs = 0;
static int _lightLevel = 2000;
static bool _lightReady = true;
static bool _fillLightOn = false;

namespace {
bool _presenceRawActive() {
    return digitalRead(PERSON_SENSOR_PIN) == PERSON_SENSOR_ACTIVE_LEVEL;
}

void _updatePirPresence(unsigned long now) {
    if (!_presenceReady) {
        if (now - _presenceBootMs >= PERSON_SENSOR_WARMUP_MS) {
            _presenceReady = true;
            LOGI("人体存在传感器预热完成 (PIR)");
        } else {
            _presencePresent = true;
            return;
        }
    }

    bool rawActive = _presenceRawActive();
    if (rawActive) {
        _presenceLastActiveMs = now;
        _presencePresent = true;
        return;
    }

    if (PERSON_SENSOR_HOLD_MS > 0 && now - _presenceLastActiveMs < PERSON_SENSOR_HOLD_MS) {
        _presencePresent = true;
    } else {
        _presencePresent = false;
    }
}

void _updateBeamBreakPresence(unsigned long now) {
    (void)now;
    _presenceReady = true;
    _presencePresent = _presenceRawActive();
}
}

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
    LOGI("人体存在传感器初始化: type=%d pin=GPIO%d activeLevel=%d hold=%lums", PERSON_SENSOR_TYPE, PERSON_SENSOR_PIN, PERSON_SENSOR_ACTIVE_LEVEL, PERSON_SENSOR_HOLD_MS);
    pinMode(PERSON_SENSOR_PIN, INPUT);
    _presenceBootMs = millis();
    _presenceLastActiveMs = _presenceBootMs;
    _presenceReady = PERSON_SENSOR_TYPE == PERSON_SENSOR_BEAM_BREAK ? true : false;
    _presencePresent = true;
#else
    LOGI("人体存在传感器未启用（ENABLE_PIR=0），默认: 有人");
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

#if ENABLE_FILL_LIGHT
    LOGI("补光 LED 初始化: GPIO%d", FILL_LIGHT_PIN);
    pinMode(FILL_LIGHT_PIN, OUTPUT);
    digitalWrite(FILL_LIGHT_PIN, LOW);
    _fillLightOn = false;
#else
    LOGI("补光 LED 未启用（ENABLE_FILL_LIGHT=0）");
#endif
}

/**
 * @brief 更新传感器读数（在 loop 中定时调用）
 */
void sensors_update() {
#if ENABLE_PIR
    unsigned long now = millis();
    if (PERSON_SENSOR_TYPE == PERSON_SENSOR_BEAM_BREAK) {
        _updateBeamBreakPresence(now);
    } else {
        _updatePirPresence(now);
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
    // 旧接口兼容包装：保持对上层 API 稳定。
    return sensors_isPresencePresent();
}

/**
 * @brief 获取环境光线强度
 * @return lux 整数值（ENABLE_LIGHT_SENSOR=0 时返回 2000）
 */
int sensors_getLightLevel() {
    return _lightLevel;
}

bool sensors_isPresenceReady() {
    return _presenceReady;
}

bool sensors_isPresencePresent() {
    // 人体存在传感器关闭时保持 true，确保"无外设"场景仍可走主检测链路。
    return _presencePresent;
}

bool sensors_isPirReady() {
    return sensors_isPresenceReady();
}

bool sensors_isLightSensorReady() {
    return _lightReady;
}

void sensors_setFillLight(bool on) {
#if ENABLE_FILL_LIGHT
    _fillLightOn = on;
    digitalWrite(FILL_LIGHT_PIN, on ? HIGH : LOW);
#else
    _fillLightOn = false;
    (void)on;
#endif
}

bool sensors_isFillLightOn() {
    return _fillLightOn;
}
