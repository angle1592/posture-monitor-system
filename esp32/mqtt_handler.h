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

extern PubSubClient mqttClient;

// =============================================================================
// 公共函数声明
// =============================================================================

/**
 * @brief 连接到 WiFi
 * @return true 连接成功
 */
bool mqtt_connectWiFi();

/**
 * @brief 初始化 MQTT 客户端
 * @param callback MQTT 消息回调函数
 */
void mqtt_init(MQTT_CALLBACK_SIGNATURE);

/**
 * @brief 连接到 OneNET MQTT 服务器
 * 
 * 连接成功后订阅必要的主题。
 * @return true 连接成功
 */
bool mqtt_connect();

/**
 * @brief MQTT 主循环处理（心跳 + 断线重连）
 * 
 * 在 loop() 中调用。处理 MQTT 心跳包和消息接收。
 * 如果断线则尝试重连。
 * 
 * @return true MQTT 当前已连接
 */
bool mqtt_loop();

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
bool mqtt_publishProperty(const char* paramsJson);

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
bool mqtt_publishEvent(const char* eventId, const char* paramsJson);

/**
 * @brief 发送 property/set 回复
 * 
 * 收到云端 property/set 命令后，必须回复确认。
 * id 必须与收到的消息中的 id 一致。
 * 
 * @param msgId 收到的消息 ID（字符串）
 * @return true 发送成功
 */
bool mqtt_sendSetReply(const char* msgId);

/**
 * @brief 发送 property/get 回复
 * 
 * 收到云端 property/get 请求后，回复当前属性值。
 * 
 * @param msgId 收到的消息 ID（字符串）
 * @param paramsJson 当前属性值 JSON 片段
 * @return true 发送成功
 */
bool mqtt_sendGetReply(const char* msgId, const char* paramsJson);

/**
 * @brief 从 MQTT 消息中提取 id 字段
 * 
 * @param message 收到的 JSON 消息
 * @param idOut 输出缓冲区（至少 32 字节）
 * @return true 提取成功
 */
bool mqtt_extractId(const char* message, char* idOut, int maxLen);

#endif // MQTT_HANDLER_H