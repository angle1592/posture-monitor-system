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
 */
inline void initSerial() {
    Serial.begin(SERIAL_BAUD_RATE);
    
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 5000) {
        ; // 等待串口连接（原生 USB 开发板需要）
    }
    
    Serial.println();
    Serial.println(F("========================================================"));
    Serial.println(F("    智能语音坐姿提醒器 - 主固件 v1.0"));
    Serial.println(F("    Smart Voice Posture Reminder - Main Firmware"));
    Serial.println(F("    Freenove ESP32-S3 WROOM + K230 Vision"));
    Serial.println(F("========================================================"));
    Serial.println();
}

/**
 * @brief 打印 WiFi 状态代码的人类可读解释
 */
inline void printWiFiStatusExplanation(int status) {
    LOG_INFO("WiFi 状态解释:");
    switch (status) {
        case WL_IDLE_STATUS:
            LOGI("  -> WL_IDLE_STATUS: WiFi 正在状态切换过程中");
            break;
        case WL_NO_SSID_AVAIL:
            LOGI("  -> WL_NO_SSID_AVAIL: 配置的 SSID 无法访问");
            LOGI("    检查: SSID 拼写、2.4GHz 网络、是否在范围内");
            break;
        case WL_CONNECTED:
            LOGI("  -> WL_CONNECTED: 成功连接到 WiFi");
            break;
        case WL_CONNECT_FAILED:
            LOGI("  -> WL_CONNECT_FAILED: 连接失败");
            LOGI("    检查: 密码是否正确、MAC 过滤、设备数量限制");
            break;
        case WL_CONNECTION_LOST:
            LOGI("  -> WL_CONNECTION_LOST: 连接丢失，将自动重连");
            break;
        case WL_DISCONNECTED:
            LOGI("  -> WL_DISCONNECTED: 已断开连接");
            break;
        default:
            LOGW("  -> 未知状态代码: %d", status);
            break;
    }
}

/**
 * @brief 打印格式化的横幅
 */
inline void printBanner(const char* title, int width = 60) {
    int titleLen = strlen(title);
    int padding = (width - titleLen - 2) / 2;
    
    Serial.println();
    for (int i = 0; i < width; i++) Serial.print("=");
    Serial.println();
    
    for (int i = 0; i < padding; i++) Serial.print(" ");
    Serial.print(" ");
    Serial.print(title);
    Serial.print(" ");
    for (int i = 0; i < (width - padding - titleLen - 2); i++) Serial.print(" ");
    Serial.println();
    
    for (int i = 0; i < width; i++) Serial.print("=");
    Serial.println();
    Serial.println();
}

/**
 * @brief 将 PubSubClient MQTT 状态代码转换为可读字符串
 */
inline const char* mqttStateToString(int state) {
    switch (state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT - 服务器未及时响应";
        case -3: return "MQTT_CONNECTION_LOST - 网络连接断开";
        case -2: return "MQTT_CONNECT_FAILED - 网络连接失败";
        case -1: return "MQTT_DISCONNECTED - 客户端已断开连接";
        case 0:  return "MQTT_CONNECTED - 成功连接";
        case 1:  return "MQTT_CONNECT_BAD_PROTOCOL - 协议版本不支持";
        case 2:  return "MQTT_CONNECT_BAD_CLIENT_ID - Client ID 被拒绝";
        case 3:  return "MQTT_CONNECT_UNAVAILABLE - 服务器不可用";
        case 4:  return "MQTT_CONNECT_BAD_CREDENTIALS - 认证失败";
        case 5:  return "MQTT_CONNECT_UNAUTHORIZED - 未授权";
        default: return "未知的 MQTT 状态";
    }
}

#endif // UTILS_H
