#include "mqtt_handler.h"

/**
 * @brief mqtt_handler.cpp 实现说明
 *
 * 实现要点：
 * - WiFi 连接采用“有限次重试 + 周期状态解释”策略，平衡启动速度与可诊断性。
 * - MQTT 连接成功后立即订阅核心主题，确保云端下发命令不丢。
 * - 主循环重连采用 millis() 非阻塞节流，避免网络异常时卡住本地控制逻辑。
 * - 上报与回复报文使用 sprintf 直接拼接 JSON，保持轻量且与既有协议实现一致。
 *
 * 内部状态变量含义：
 * - _wifiClient：PubSubClient 底层 TCP 传输对象。
 * - _postMsgId：OneNET 消息 id 递增计数器，用于请求-响应关联。
 */

// =============================================================================
// 全局变量定义
// =============================================================================

static WiFiClient _wifiClient;
PubSubClient mqttClient(_wifiClient);

// 消息 ID 计数器（简单自增，已验证可用）
static unsigned int _postMsgId = 0;

// =============================================================================
// 公共函数实现
// =============================================================================

bool mqtt_connectWiFi() {
    LOGI("正在连接到 WiFi: %s", WIFI_SSID);
    
    if (WiFi.status() == WL_CONNECTED) {
        LOGI("WiFi 已经连接！");
        return true;
    }
    
    WiFi.mode(WIFI_STA);
    // 先断开历史连接再重连，减少路由器侧保留状态导致的异常。
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    LOGI("等待 WiFi 连接...");
    
    int attempts = 0;
    // 轮询等待连接，期间仅短延时，不引入长阻塞。
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRY_ATTEMPTS) {
        delay(WIFI_RETRY_DELAY_MS);
        LOGD("WiFi 连接尝试 %d/%d...", attempts + 1, WIFI_MAX_RETRY_ATTEMPTS);

        // 每隔若干次输出一次详细状态，避免日志刷屏同时保留排障信息。
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

void mqtt_init(MQTT_CALLBACK_SIGNATURE) {
    LOGI("MQTT 初始化: %s:%d", MQTT_HOST, MQTT_PORT);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(callback);
    // 按项目约束保持默认参数，避免不同库版本下出现兼容性差异。
    // 关键：不调用 setBufferSize / setKeepAlive / setSocketTimeout
}

bool mqtt_connect() {
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

        // 连接完成后立刻订阅：
        // 1) 属性上报回复（确认上报结果）
        if (mqttClient.subscribe(TOPIC_PROPERTY_POST_REPLY)) {
            LOGI("  订阅成功: post_reply");
        } else {
            LOGW("  订阅失败: post_reply");
        }

        // 2) 属性设置命令（云端控制设备）
        if (mqttClient.subscribe(TOPIC_PROPERTY_SET)) {
            LOGI("  订阅成功: property/set");
        } else {
            LOGW("  订阅失败: property/set");
        }

        // 3) 属性查询请求（云端拉取当前状态）
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

bool mqtt_loop() {
    if (!mqttClient.connected()) {
        // 使用 static 变量实现非阻塞重连
        static unsigned long lastReconnectAttempt = 0;
        unsigned long now = millis();

        // 重连节流：未到窗口直接返回 false，让主循环继续跑传感器/显示/报警。
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_DELAY_MS) {
            lastReconnectAttempt = now;
            LOGW("MQTT 连接丢失，尝试重连...");
            if (mqtt_connect()) {
                // 重连成功后清零节流时间，后续断线可立即重新进入计时。
                lastReconnectAttempt = 0;
                return true;
            }
        }
        return false;
    }
    
    mqttClient.loop();
    return true;
}

bool mqtt_publishProperty(const char* paramsJson) {
    char jsonBuf[512];

    // 通过递增 id 构建请求，便于平台侧与回包对应。
    sprintf(jsonBuf, "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":%s}", _postMsgId++, paramsJson);
    
    LOGI("[MQTT 发布] 属性上报");
    LOGD("  主题: %s", TOPIC_PROPERTY_POST);
    LOGD("  内容: %s", jsonBuf);
    
    // 固定使用 2 参数 publish(topic, payload) 以保持稳定兼容。
    bool success = mqttClient.publish(TOPIC_PROPERTY_POST, jsonBuf);
    
    if (success) {
        LOGI("  属性发布成功 (id=%u)", _postMsgId - 1);
    } else {
        LOGE("  属性发布失败！MQTT 状态: %d", mqttClient.state());
    }
    
    return success;
}

bool mqtt_publishEvent(const char* eventId, const char* paramsJson) {
    char jsonBuf[512];
    
    sprintf(jsonBuf, "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{\"%s\":%s}}", 
            _postMsgId++, eventId, paramsJson);
    
    LOGI("[MQTT 发布] 事件上报: %s", eventId);
    LOGD("  内容: %s", jsonBuf);
    
    // 事件与属性上报分主题，平台可按事件流单独处理。
    bool success = mqttClient.publish(TOPIC_EVENT_POST, jsonBuf);
    
    if (success) {
        LOGI("  事件发布成功");
    } else {
        LOGE("  事件发布失败！");
    }
    
    return success;
}

bool mqtt_sendSetReply(const char* msgId) {
    char replyBuf[128];
    sprintf(replyBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", msgId);
    
    LOGI("[MQTT 回复] set_reply: %s", replyBuf);
    
    // set_reply 需要与下行命令 id 对齐，否则云端会判定设备未应答。
    bool success = mqttClient.publish(TOPIC_PROPERTY_SET_REPLY, replyBuf);
    
    if (success) {
        LOGI("  set_reply 发送成功");
    } else {
        LOGE("  set_reply 发送失败！");
    }
    
    return success;
}

bool mqtt_sendGetReply(const char* msgId, const char* paramsJson) {
    char replyBuf[512];
    sprintf(replyBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":%s}", msgId, paramsJson);
    
    LOGI("[MQTT 回复] get_reply: %s", replyBuf);
    
    // get_reply 在 data 字段返回当前属性快照。
    bool success = mqttClient.publish(TOPIC_PROPERTY_GET_REPLY, replyBuf);
    
    if (success) {
        LOGI("  get_reply 发送成功");
    } else {
        LOGE("  get_reply 发送失败！");
    }
    
    return success;
}

bool mqtt_extractId(const char* message, char* idOut, int maxLen) {
    // 轻量解析策略：先定位 "id":"，再找下一个双引号作为结束边界。
    // 不依赖 JSON 库，适配资源受限固件场景。
    const char* idStart = strstr(message, "\"id\":\"");
    if (idStart == NULL) {
        // 兜底输出 "0"，避免调用方使用未初始化字符串。
        strcpy(idOut, "0");
        return false;
    }

    idStart += 6;  // 跳过 "id":"
    const char* idEnd = strchr(idStart, '"');
    if (idEnd == NULL || (idEnd - idStart) >= maxLen) {
        // 结束符缺失或超出缓冲区都视为解析失败，防止越界。
        strcpy(idOut, "0");
        return false;
    }
    
    strncpy(idOut, idStart, idEnd - idStart);
    idOut[idEnd - idStart] = '\0';
    return true;
}
