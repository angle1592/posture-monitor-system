/**
 * @file utils.h
 * @brief 调试和日志记录的实用函数和宏
 * 
 * 提供标准化的日志宏，可以根据不同的构建配置轻松启用/禁用。
 * 支持多级日志和格式化输出，类似于标准的日志框架。
 * 
 * 沿用自 Stage 2（02_command_test），针对主固件微调启动横幅。
 * 
 * @date 2026
 */

#ifndef UTILS_H
#define UTILS_H

/**
 * @brief 工具与日志模块（公共基础模块）
 *
 * 模块职责：
 * - 提供统一日志宏（按 LOG_LEVEL 控制输出等级）。
 * - 提供串口初始化、横幅打印、WiFi/MQTT 状态解释等通用工具。
 *
 * 对外核心 API：
 * - initSerial()
 * - printWiFiStatusExplanation()
 * - printBanner()
 * - mqttStateToString()
 *
 * 依赖关系：
 * - Arduino Serial / millis
 * - WiFi 状态码定义（WiFi.h）
 * - 项目配置（config.h）
 *
 * 是否可选：
 * - 非可选模块，属于其它模块的基础依赖。
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// =============================================================================
// 日志级别定义
// =============================================================================

enum LogLevel {
    LOG_LEVEL_NONE = 0,     ///< 无日志输出
    LOG_LEVEL_ERROR = 1,    ///< 仅关键错误
    LOG_LEVEL_WARN = 2,     ///< 警告和错误
    LOG_LEVEL_INFO = 3,     ///< 一般信息
    LOG_LEVEL_DEBUG = 4,    ///< 详细的调试信息
    LOG_LEVEL_VERBOSE = 5   ///< 最大详细程度
};

// =============================================================================
// 编译时配置
// =============================================================================

#ifndef LOG_LEVEL
    #if DEBUG_ENABLED
        #define LOG_LEVEL LOG_LEVEL_DEBUG
    #else
        #define LOG_LEVEL LOG_LEVEL_INFO
    #endif
#endif

// =============================================================================
// 内部实现宏
// =============================================================================

#define _LOG_PREFIX(level, level_str) \
    do { \
        if (LOG_LEVEL >= level) { \
            Serial.print("["); \
            Serial.print(millis() / 1000 / 60 / 60 % 24); \
            Serial.print(":"); \
            Serial.print(millis() / 1000 / 60 % 60); \
            Serial.print(":"); \
            Serial.print(millis() / 1000 % 60); \
            Serial.print("."); \
            Serial.print(millis() % 1000); \
            Serial.print("]["); \
            Serial.print(level_str); \
            Serial.print("] "); \
        } \
    } while(0)

#define _LOG_FORMAT(level, level_str, fmt, ...) \
    do { \
        if (LOG_LEVEL >= level) { \
            _LOG_PREFIX(level, level_str); \
            Serial.printf(fmt, ##__VA_ARGS__); \
            Serial.println(); \
        } \
    } while(0)

#define _LOG_VALUE(level, level_str, value) \
    do { \
        if (LOG_LEVEL >= level) { \
            _LOG_PREFIX(level, level_str); \
            Serial.println(value); \
        } \
    } while(0)

// =============================================================================
// 公共日志宏
// =============================================================================

#define LOGV(fmt, ...) _LOG_FORMAT(LOG_LEVEL_VERBOSE, "详细", fmt, ##__VA_ARGS__)
#define LOG_VERBOSE(value) _LOG_VALUE(LOG_LEVEL_VERBOSE, "详细", value)

#define LOGD(fmt, ...) _LOG_FORMAT(LOG_LEVEL_DEBUG, "调试", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(value) _LOG_VALUE(LOG_LEVEL_DEBUG, "调试", value)

#define LOGI(fmt, ...) _LOG_FORMAT(LOG_LEVEL_INFO, "信息", fmt, ##__VA_ARGS__)
#define LOG_INFO(value) _LOG_VALUE(LOG_LEVEL_INFO, "信息", value)

#define LOGW(fmt, ...) _LOG_FORMAT(LOG_LEVEL_WARN, "警告", fmt, ##__VA_ARGS__)
#define LOG_WARN(value) _LOG_VALUE(LOG_LEVEL_WARN, "警告", value)

#define LOGE(fmt, ...) _LOG_FORMAT(LOG_LEVEL_ERROR, "错误", fmt, ##__VA_ARGS__)
#define LOG_ERROR(value) _LOG_VALUE(LOG_LEVEL_ERROR, "错误", value)

// =============================================================================
// 实用函数
// =============================================================================

/**
 * @brief 初始化调试串口
 *
 * 在系统启动早期调用，确保后续模块日志可见。
 */
void initSerial();

/**
 * @brief 打印 WiFi 状态代码的人类可读解释
 *
 * 用于连接失败时快速定位问题（SSID、密码、频段、覆盖等）。
 */
void printWiFiStatusExplanation(int status);

/**
 * @brief 打印格式化的横幅
 *
 * 用于串口日志分段，提升调试可读性。
 */
void printBanner(const char* title, int width = 60);

/**
 * @brief 将 PubSubClient MQTT 状态代码转换为可读字符串
 *
 * 便于在连接失败日志中直接输出原因文本。
 */
const char* mqttStateToString(int state);

#endif // UTILS_H
