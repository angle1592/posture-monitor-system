#include "mqtt_handler.h"

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

void mqtt_init(MQTT_CALLBACK_SIGNATURE) {
    LOGI("MQTT 初始化: %s:%d", MQTT_HOST, MQTT_PORT);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(callback);
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

bool mqtt_loop() {
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

bool mqtt_publishProperty(const char* paramsJson) {
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

bool mqtt_publishEvent(const char* eventId, const char* paramsJson) {
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

bool mqtt_sendSetReply(const char* msgId) {
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

bool mqtt_sendGetReply(const char* msgId, const char* paramsJson) {
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

bool mqtt_extractId(const char* message, char* idOut, int maxLen) {
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