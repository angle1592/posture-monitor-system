/**
 * @file mqtt_handler.h
 * @brief MQTT 通信模块 — 连接、发布、订阅、回调处理
 * 
 * 从 Stage 2 (02_command_test) 提取并模块化封装。
 * 严格沿用已验证的 MQTT 模式：
 * - 2 参数 publish(topic, payload)
 * - sprintf 构建 JSON
 * - strstr 解析 JSON
 * - 简单自增 postMsgId
 * - 不用 ArduinoJson / setBufferSize / setKeepAlive / setSocketTimeout
 * 
 * @date 2026
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "utils.h"

// =============================================================================
// 全局 MQTT 对象（供主程序使用）
// =============================================================================

static WiFiClient _wifiClient;
static PubSubClient mqttClient(_wifiClient);

// 消息 ID 计数器（简单自增，已验证可用）
static unsigned int _postMsgId = 0;

// =============================================================================
// WiFi 连接（从 Stage 2 搬过来，不修改）
// =============================================================================

/**
 * @brief 连接到 WiFi
 * @return true 连接成功
 */
inline bool mqtt_connectWiFi() {
    LOGI("正在连接到 WiFi: %s", WIFI_SSID);
    
    if (WiFi.status() == WL_CONNECTED) {
        LOGI("WiFi 已经连接！");
        return true;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    LOGI("等待 WiFi 连接...");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRY_ATTEMPTS) {
        delay(WIFI_RETRY_DELAY_MS);
        LOGD("WiFi 连接尝试 %d/%d...", attempts + 1, WIFI_MAX_RETRY_ATTEMPTS);
        
        if (attempts % 5 == 0 && attempts > 0) {
            printWiFiStatusExplanation(WiFi.status());
        }
        
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        LOGI("WiFi 连接成功！");
        LOGI("  IP 地址: %s", WiFi.localIP().toString().c_str());
        LOGI("  RSSI: %d dBm", WiFi.RSSI());
        return true;
    } else {
        LOGE("WiFi 连接失败！");
        printWiFiStatusExplanation(WiFi.status());
        return false;
    }
}

// =============================================================================
// MQTT 初始化与连接
// =============================================================================

/**
 * @brief 初始化 MQTT 客户端
 * @param callback MQTT 消息回调函数
 */
inline void mqtt_init(MQTT_CALLBACK_SIGNATURE) {
    LOGI("MQTT 初始化: %s:%d", MQTT_HOST, MQTT_PORT);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(callback);
    // 关键：不调用 setBufferSize / setKeepAlive / setSocketTimeout
}

/**
 * @brief 连接到 OneNET MQTT 服务器
 * 
 * 连接成功后订阅必要的主题。
 * @return true 连接成功
 */
inline bool mqtt_connect() {
    LOGI("正在连接到 MQTT 服务器...");
    LOGI("  服务器: %s:%d", MQTT_HOST, MQTT_PORT);
    LOGI("  Client ID: %s", ONENET_DEVICE_NAME);
    
    if (WiFi.status() != WL_CONNECTED) {
        LOGE("无法连接 MQTT：WiFi 未连接！");
        return false;
    }
    
    if (mqttClient.connected()) {
        LOGI("MQTT 已经连接！");
        return true;
    }
    
    bool connected = mqttClient.connect(
        ONENET_DEVICE_NAME,
        ONENET_PRODUCT_ID,
        ONENET_DEVICE_TOKEN
    );
    
    if (connected) {
        LOGI("MQTT 连接成功！");
        
        // 订阅属性上报回复
        if (mqttClient.subscribe(TOPIC_PROPERTY_POST_REPLY)) {
            LOGI("  订阅成功: post_reply");
        } else {
            LOGW("  订阅失败: post_reply");
        }
        
        // 订阅属性设置命令（云端下发）
        if (mqttClient.subscribe(TOPIC_PROPERTY_SET)) {
            LOGI("  订阅成功: property/set");
        } else {
            LOGW("  订阅失败: property/set");
        }
        
        // 订阅属性查询请求（云端查询）
        if (mqttClient.subscribe(TOPIC_PROPERTY_GET)) {
            LOGI("  订阅成功: property/get");
        } else {
            LOGW("  订阅失败: property/get");
        }
        
        return true;
    } else {
        int state = mqttClient.state();
        LOGE("MQTT 连接失败！错误: %d (%s)", state, mqttStateToString(state));
        return false;
    }
}

/**
 * @brief MQTT 主循环处理（心跳 + 断线重连）
 * 
 * 在 loop() 中调用。处理 MQTT 心跳包和消息接收。
 * 如果断线则尝试重连。
 * 
 * @return true MQTT 当前已连接
 */
inline bool mqtt_loop() {
    if (!mqttClient.connected()) {
        // 使用 static 变量实现非阻塞重连
        static unsigned long lastReconnectAttempt = 0;
        unsigned long now = millis();
        
        // 退避窗口内直接返回，让主循环继续处理本地任务，避免阻塞控制逻辑。
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_DELAY_MS) {
            lastReconnectAttempt = now;
            LOGW("MQTT 连接丢失，尝试重连...");
            if (mqtt_connect()) {
                lastReconnectAttempt = 0;
                return true;
            }
        }
        return false;
    }
    
    mqttClient.loop();
    return true;
}

// =============================================================================
// MQTT 发布函数
// =============================================================================

/**
 * @brief 发布属性到 OneNET
 * 
 * 将 params JSON 片段包装成完整的 OneNET 属性上报消息并发布。
 * 
 * @param paramsJson 属性 JSON 片段，例如：{"isPosture":{"value":true}}
 * @return true 发布成功
 * 
 * 生成的完整 JSON 格式：
 * {"id":"123","version":"1.0","params":{"isPosture":{"value":true}}}
 */
inline bool mqtt_publishProperty(const char* paramsJson) {
    char jsonBuf[512];
    
    // 关键：id 使用简单自增整数
    sprintf(jsonBuf, "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":%s}", _postMsgId++, paramsJson);
    
    LOGI("[MQTT 发布] 属性上报");
    LOGD("  主题: %s", TOPIC_PROPERTY_POST);
    LOGD("  内容: %s", jsonBuf);
    
    // 关键：使用 2 参数字符串版本的 publish()
    bool success = mqttClient.publish(TOPIC_PROPERTY_POST, jsonBuf);
    
    if (success) {
        LOGI("  属性发布成功 (id=%u)", _postMsgId - 1);
    } else {
        LOGE("  属性发布失败！MQTT 状态: %d", mqttClient.state());
    }
    
    return success;
}

/**
 * @brief 发布事件到 OneNET
 * 
 * @param eventId 事件标识符，例如 "postureAlert"
 * @param paramsJson 事件参数 JSON 片段
 * @return true 发布成功
 * 
 * 生成的 JSON 格式：
 * {"id":"123","version":"1.0","params":{"postureAlert":{"value":{"type":"head_down"},"time":1234567890}}}
 */
inline bool mqtt_publishEvent(const char* eventId, const char* paramsJson) {
    char jsonBuf[512];
    
    sprintf(jsonBuf, "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{\"%s\":%s}}", 
            _postMsgId++, eventId, paramsJson);
    
    LOGI("[MQTT 发布] 事件上报: %s", eventId);
    LOGD("  内容: %s", jsonBuf);
    
    bool success = mqttClient.publish(TOPIC_EVENT_POST, jsonBuf);
    
    if (success) {
        LOGI("  事件发布成功");
    } else {
        LOGE("  事件发布失败！");
    }
    
    return success;
}

/**
 * @brief 发送 property/set 回复
 * 
 * 收到云端 property/set 命令后，必须回复确认。
 * id 必须与收到的消息中的 id 一致。
 * 
 * @param msgId 收到的消息 ID（字符串）
 * @return true 发送成功
 */
inline bool mqtt_sendSetReply(const char* msgId) {
    char replyBuf[128];
    sprintf(replyBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", msgId);
    
    LOGI("[MQTT 回复] set_reply: %s", replyBuf);
    
    bool success = mqttClient.publish(TOPIC_PROPERTY_SET_REPLY, replyBuf);
    
    if (success) {
        LOGI("  set_reply 发送成功");
    } else {
        LOGE("  set_reply 发送失败！");
    }
    
    return success;
}

/**
 * @brief 发送 property/get 回复
 * 
 * 收到云端 property/get 请求后，回复当前属性值。
 * 
 * @param msgId 收到的消息 ID（字符串）
 * @param paramsJson 当前属性值 JSON 片段
 * @return true 发送成功
 */
inline bool mqtt_sendGetReply(const char* msgId, const char* paramsJson) {
    char replyBuf[512];
    sprintf(replyBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":%s}", msgId, paramsJson);
    
    LOGI("[MQTT 回复] get_reply: %s", replyBuf);
    
    bool success = mqttClient.publish(TOPIC_PROPERTY_GET_REPLY, replyBuf);
    
    if (success) {
        LOGI("  get_reply 发送成功");
    } else {
        LOGE("  get_reply 发送失败！");
    }
    
    return success;
}

// =============================================================================
// MQTT 消息解析工具
// =============================================================================

/**
 * @brief 从 MQTT 消息中提取 id 字段
 * 
 * @param message 收到的 JSON 消息
 * @param idOut 输出缓冲区（至少 32 字节）
 * @return true 提取成功
 */
inline bool mqtt_extractId(const char* message, char* idOut, int maxLen) {
    const char* idStart = strstr(message, "\"id\":\"");
    if (idStart == NULL) {
        strcpy(idOut, "0");
        return false;
    }
    
    idStart += 6;  // 跳过 "id":"
    const char* idEnd = strchr(idStart, '"');
    if (idEnd == NULL || (idEnd - idStart) >= maxLen) {
        strcpy(idOut, "0");
        return false;
    }
    
    strncpy(idOut, idStart, idEnd - idStart);
    idOut[idEnd - idStart] = '\0';
    return true;
}

#endif // MQTT_HANDLER_H
