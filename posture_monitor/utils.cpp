#include "utils.h"

/**
 * @brief utils.cpp 实现说明
 *
 * 实现要点：
 * - 串口初始化采用“最长等待 5 秒”策略，兼容原生 USB 开发板上电后串口枚举延迟。
 * - WiFi/MQTT 状态解释函数将数字状态码转为可读文本，减少调试时查表成本。
 * - 横幅函数用于串口日志分区，便于观察不同阶段输出。
 *
 * 内部状态变量含义：
 * - 本模块不维护静态状态，所有工具函数无共享可变状态。
 */

/**
 * @brief 初始化调试串口
 */
void initSerial() {
    // 统一串口入口：主固件和各外设模块均依赖此串口输出日志。
    Serial.begin(SERIAL_BAUD_RATE);
    
    unsigned long start = millis();
    // 原生 USB 设备在刚上电时 Serial 可能尚未就绪，这里等待一小段时间。
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
void printWiFiStatusExplanation(int status) {
    // 将 WiFi.status() 数值映射为排障提示，避免“只看到数字码”的低效调试。
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
void printBanner(const char* title, int width) {
    // 通过左右填充让标题居中显示，便于串口日志视觉分段。
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
const char* mqttStateToString(int state) {
    // PubSubClient 的 state() 返回整型状态码，这里集中做可读化转换。
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
