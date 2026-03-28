/**
 * @file posture_monitor.ino
 * @brief 智能语音坐姿提醒器 — 主固件程序
 * 
 * 系统架构：
 * - K230 视觉模块通过 UART1 发送坐姿检测 JSON 数据
 * - ESP32-S3 接收 K230 数据，通过 MQTT 上报到 OneNET 云平台
 * - 云端可下发 property/set 命令控制设备（如开关检测）
 * - 云端可下发 property/get 请求查询当前状态
 * - WS2812 RGB LED 指示系统状态（板载，已启用）
 * - 其他外设（OLED、EC11、PIR、语音、蜂鸣器）通过 ENABLE 宏按需启用
 * 
 * 关键约束（已验证）：
 * - publish() 使用 2 参数字符串版本
 * - 不使用 ArduinoJson，用 sprintf/strstr 处理 JSON
 * - 不调用 setBufferSize / setKeepAlive / setSocketTimeout
 * - id 字段使用简单自增整数
 * 
 * @date 2026
 */

// =============================================================================
// 头文件包含（顺序很重要：config → utils → 各模块）
// =============================================================================

#include "config.h"
#include "utils.h"
#include "mqtt_handler.h"
#include "k230_parser.h"
#include "mode_manager.h"
#include "display.h"
#include "sensors.h"
#include "voice.h"
#include "alerts.h"
#include "timer_fsm.h"

// =============================================================================
// 全局状态变量
// =============================================================================

// --- 定时器 ---
static unsigned long lastPublishTime = 0;       // 上次 MQTT 属性上报时间
static unsigned long lastSensorTime = 0;        // 上次传感器读取时间
static unsigned long lastDisplayTime = 0;       // 上次 OLED 刷新时间
static unsigned long lastAlertTime = 0;         // 上次报警检查时间

// --- 云端控制 ---
static bool monitoringEnabled = true;           // 坐姿检测开关（云端可控）

enum PublishDecisionReason {
    DECISION_MONITOR_OFF = 0,
    DECISION_NO_PERSON,
    DECISION_STARTUP_WAIT,
    DECISION_STALE_HOLD,
    DECISION_FALLBACK_TRUE,
    DECISION_LIVE_FRAME
};

typedef struct {
    bool isPostureOk;
    PublishDecisionReason reason;
    unsigned long dataAgeMs;
} PublishDecision;


static bool testForcePostureEnabled = false;
static bool testForceAbnormal = true;
static char serialCmdBuffer[96];
static size_t serialCmdLen = 0;
static bool pendingImmediatePublish = false;
static bool lastSyncedMonitoringEnabled = true;
static int lastSyncedMode = MODE_POSTURE;
static int lastSyncedTimerState = TIMER_IDLE;
static unsigned long lastSyncedCfgVersion = 0;

// =============================================================================
// 函数声明
// =============================================================================

// MQTT 回调
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handlePropertySet(const char* message);
void handlePropertyGet(const char* message);
void handleModeAndTimerInput();

void handleSerialTestInput();
void processSerialTestCommand(const char* line);

// 定时任务
void handlePeriodicPublish();
PublishDecision evaluatePeriodicPublishDecision(const K230Data* data, unsigned long now, bool noPerson);
const char* publishDecisionReasonText(PublishDecisionReason reason);
void runActuatorSelfTest();
void buildPropertyPayloads(char* statusParams, size_t statusLen, char* configParams, size_t configLen, char* sensorParams, size_t sensorLen, PublishDecision* decisionOut);
void requestImmediatePublish();
void syncImmediatePublishState();
bool flushPendingPropertyPublish(unsigned long now);
void syncTimerAdjustInputLock();

bool jsonFindValueStart(const char* message, const char* key, const char** valueStart);
bool jsonFindParamValueStart(const char* message, const char* key, const char** valueStart);
bool jsonParseBool(const char* p, bool* outValue);
bool jsonParseULong(const char* p, unsigned long* outValue);

void refreshDisplayForTest();

// 工具函数
void blinkLED(int times = 1, int duration = 100);

// =============================================================================
// setup() — 系统初始化
// =============================================================================

/**
 * @brief 初始化系统各模块并建立首轮云连接。
 * @details 按“底层通信 -> 外设 -> 网络 -> 云端”的顺序启动，目的是先确保依赖项可用再暴露上层能力。
 */
void setup() {
    // 1. 初始化调试串口
    initSerial();
    LOGI("系统启动中...");
    LOGD("编译日期: %s %s", __DATE__, __TIME__);
    
    // 2. 初始化板载 LED
    #if STATUS_LED_PIN >= 0
        pinMode(STATUS_LED_PIN, OUTPUT);
        digitalWrite(STATUS_LED_PIN, LOW);
    #endif
    blinkLED(2, 100);
    
    // 3. 初始化 K230 UART 通信
    printBanner("K230 UART");
    k230_init();
    
    // 4. 配置 SNTP 时钟同步（ESP32 内置，用于时钟模式）
    printBanner("SNTP 时钟");
    LOGI("配置 SNTP: %s, %s", SNTP_SERVER1, SNTP_SERVER2);
    LOGI("时区: UTC+%d", SNTP_GMT_OFFSET / 3600);
    configTime(SNTP_GMT_OFFSET, SNTP_DAYLIGHT, SNTP_SERVER1, SNTP_SERVER2);
    LOGI("SNTP 配置完成（WiFi 连接后自动同步）");
    
    // 5. 初始化各外设模块（ENABLE=0 的模块为 stub，不影响编译）
    printBanner("外设初始化");
    mode_init();
    display_init();
    sensors_init();
    voice_init();
    alerts_init();
    // 启动阶段先把默认配置同步到提醒模块，确保后续行为与 config.h 一致。
    timer_applyConfig();
    timer_init();
    lastSyncedMonitoringEnabled = monitoringEnabled;
    lastSyncedMode = (int)mode_getCurrent();
    lastSyncedTimerState = (int)timer_getState();
    lastSyncedCfgVersion = timer_getConfig()->cfgVersion;
    
    // 6. 连接 WiFi
    printBanner("WiFi 连接");
    if (!mqtt_connectWiFi()) {
        LOGE("WiFi 连接失败！将在主循环中重试。");
    }
    
    // 7. 初始化 MQTT 并连接
    printBanner("MQTT 连接");
    mqtt_init(mqttCallback);
    
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt_connect()) {
            LOGE("初始 MQTT 连接失败！将在主循环中重试。");
        }
    } else {
        LOGW("WiFi 未连接，跳过初始 MQTT 连接。");
    }
    
    // 8. 显示启动画面
    display_showSplash();
    
    // 9. 启动完成
    printBanner("系统就绪");
    LOGI("坐姿检测: %s", monitoringEnabled ? "开启" : "关闭");
    LOGI("MQTT 上报间隔: %d 秒", PUBLISH_INTERVAL_MS / 1000);
    LOGI("K230 超时阈值: %d 秒", K230_TIMEOUT_MS / 1000);
    LOGI("等待 K230 数据和云端命令...");
    
    blinkLED(3, 50);
}

// =============================================================================
// loop() — 主循环
// =============================================================================

/**
 * @brief 主循环：以非阻塞方式轮询通信、状态机与显示刷新。
 * @details 所有周期任务共用 millis() 时基，避免单点阻塞导致整机卡顿。
 */
void loop() {
    unsigned long now = millis();
    
    // 1. MQTT 心跳 + 断线重连（非阻塞）
    bool mqttOk = mqtt_loop();
    
    // 2. 读取 K230 UART 数据（非阻塞）
    k230_read();
    voice_update();
    handleSerialTestInput();

    syncTimerAdjustInputLock();
    
    // 3. 更新模式（EC11 旋转检测）
    mode_update();
    if (mode_hasChanged()) {
        requestImmediatePublish();
    }
    handleModeAndTimerInput();
    timer_tick(now);
    syncImmediatePublishState();
    
    // 4. 定时读取传感器
    if (now - lastSensorTime >= SENSOR_READ_INTERVAL_MS) {
        lastSensorTime = now;
        sensors_update();
    }
    
    // 5. 定时发布属性到 OneNET
    if (now - lastPublishTime >= PUBLISH_INTERVAL_MS) {
        lastPublishTime = now;
        handlePeriodicPublish();
    }

    if (flushPendingPropertyPublish(now)) {
        lastPublishTime = now;
    }
    
    // 6. 定时更新报警指示（WS2812 + 蜂鸣器）
    if (now - lastAlertTime >= ALERT_CHECK_INTERVAL_MS) {
        lastAlertTime = now;

        const K230Data* data = k230_getData();
        bool k230Ok = !k230_isTimeout(K230_TIMEOUT_MS);
        bool noPerson = (strcmp(data->postureType, "no_person") == 0) || (sensors_isPirReady() && !sensors_isPersonPresent());

        timer_alertPolicyTick(now, data, monitoringEnabled, testForcePostureEnabled, testForceAbnormal, noPerson);

        alerts_update(mqttOk, k230Ok, data->isAbnormal, noPerson);
    }
    
    // 7. 定时刷新 OLED 显示
    if (now - lastDisplayTime >= DISPLAY_UPDATE_INTERVAL_MS) {
        lastDisplayTime = now;

        const K230Data* data = k230_getData();
        display_setConnectivity(WiFi.status() == WL_CONNECTED, mqttOk);
        display_setSensorStatus(sensors_isPirReady(), sensors_isPersonPresent(), sensors_isLightSensorReady(), sensors_getLightLevel());
        
        // 读取定时器状态并映射到显示层状态机，确保界面与逻辑状态一致。
        TimerState tState = timer_getState();
        int tRemain = (int)timer_getRemainSec();
        int tDuration = (int)timer_getConfig()->timerDurationSec;
        
        if (tState == TIMER_RUNNING) {
            display_setTimerStatus(tRemain, tDuration, DISPLAY_TIMER_RUNNING);
        } else if (tState == TIMER_PAUSED) {
            display_setTimerStatus(tRemain, tDuration, DISPLAY_TIMER_PAUSED);
        } else if (tState == TIMER_DONE) {
            display_setTimerStatus(tRemain, tDuration, DISPLAY_TIMER_DONE);
        } else {
            display_setTimerStatus(tDuration, tDuration, DISPLAY_TIMER_IDLE);
        }
        display_update(mode_getCurrent(), data->postureType, data->isAbnormal);
    }
    
    // 短暂延时，避免占用过多 CPU
    delay(LOOP_YIELD_DELAY_MS);
}

/**
 * @brief 在标准 OneNET 属性结构中定位某 key 对应的 value 起始位置。
 * @details 先找 key 再找 "value"，是为了兼容 params 内嵌对象格式，降低误匹配概率。
 * @param message 输入 JSON 字符串
 * @param key 目标属性名（含引号）
 * @param valueStart 输出参数，返回 value 起始指针
 * @return true 表示定位成功
 */
bool jsonFindValueStart(const char* message, const char* key, const char** valueStart) {
    if (message == NULL || key == NULL || valueStart == NULL) {
        return false;
    }

    const char* keyPos = strstr(message, key);
    if (keyPos == NULL) {
        return false;
    }

    const char* valuePos = strstr(keyPos, "\"value\":");
    if (valuePos == NULL) {
        return false;
    }

    valuePos += 8;
    while (*valuePos == ' ' || *valuePos == '\t' || *valuePos == '\r' || *valuePos == '\n') {
        valuePos++;
    }

    *valueStart = valuePos;
    return true;
}

/**
 * @brief 在兼容格式下定位参数值起始位置。
 * @details 先走标准 value 结构；若失败再回退到 key:xxx 直取，保证新旧协议都能解析。
 * @param message 输入 JSON 字符串
 * @param key 目标属性名（含引号）
 * @param valueStart 输出参数，返回 value 起始指针
 * @return true 表示定位成功
 */
bool jsonFindParamValueStart(const char* message, const char* key, const char** valueStart) {
    if (message == NULL || key == NULL || valueStart == NULL) {
        return false;
    }

    if (jsonFindValueStart(message, key, valueStart)) {
        return true;
    }

    const char* keyPos = strstr(message, key);
    if (keyPos == NULL) {
        return false;
    }

    const char* valuePos = strchr(keyPos + strlen(key), ':');
    if (valuePos == NULL) {
        return false;
    }

    valuePos++;
    while (*valuePos == ' ' || *valuePos == '\t' || *valuePos == '\r' || *valuePos == '\n') {
        valuePos++;
    }

    *valueStart = valuePos;
    return true;
}

/**
 * @brief 解析 JSON 文本中的布尔值。
 * @details 只接受 true/false 文本，避免把非预期字符串静默当作有效值。
 * @param p 待解析文本起始指针
 * @param outValue 输出布尔值
 * @return true 表示解析成功
 */
bool jsonParseBool(const char* p, bool* outValue) {
    if (p == NULL || outValue == NULL) {
        return false;
    }
    if (strncmp(p, "true", 4) == 0) {
        *outValue = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0) {
        *outValue = false;
        return true;
    }
    return false;
}

/**
 * @brief 解析无符号整数参数。
 * @details 使用 strtoul 支持连续数字解析，并用 endPtr 判断是否真的读到了数字。
 * @param p 待解析文本起始指针
 * @param outValue 输出数值
 * @return true 表示解析成功
 */
bool jsonParseULong(const char* p, unsigned long* outValue) {
    if (p == NULL || outValue == NULL) {
        return false;
    }
    char* endPtr = NULL;
    unsigned long v = strtoul(p, &endPtr, 10);
    if (endPtr == p) {
        return false;
    }
    *outValue = v;
    return true;
}



/**
 * @brief 执行执行器自检（LED/蜂鸣器/语音）。
 * @details 临时强制开启所有提醒通道，验证硬件链路后再恢复原配置，避免污染运行态。
 */
void runActuatorSelfTest() {
    LOGI("[SELFTEST] 执行语音/灯光/蜂鸣器自检");

    uint8_t oldMask = timer_getConfig()->alertModeMask;
    alerts_setAlertMode(ALERT_MODE_LED | ALERT_MODE_BUZZER | ALERT_MODE_VOICE);

    alerts_lockIndicator(true, 0, 0, 48);
    alerts_triggerBuzzerPulse(BUZZER_MELODY_NOTE_MS);
    if (alerts_voiceEnabled()) {
        // 走正常语音接口（内部会优先匹配已验证可用的固定中文包）。
        voice_speak("欢迎使用");
    }
    delay(BUZZER_MELODY_GAP_MS);

    alerts_lockIndicator(true, 48, 0, 0);
    alerts_triggerBuzzerPulse(BUZZER_MELODY_NOTE_MS);
    delay(BUZZER_MELODY_GAP_MS);

    alerts_lockIndicator(true, 0, 48, 0);
    // 第二段语音省略，避免 Busy 窗口内被跳过造成误判。
    delay(BUZZER_MELODY_GAP_MS);

    alerts_lockIndicator(false, 0, 0, 0);
    alerts_setAlertMode(oldMask);
}

/**
 * @brief 非阻塞读取串口测试命令并按行分发。
 * @details 采用缓冲区累积到换行再执行，避免半包命令导致误触发。
 */
void handleSerialTestInput() {
    while (Serial.available() > 0) {
        int b = Serial.read();
        if (b < 0) {
            return;
        }

        char c = (char)b;
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            serialCmdBuffer[serialCmdLen] = '\0';
            if (serialCmdLen > 0) {
                processSerialTestCommand(serialCmdBuffer);
            }
            serialCmdLen = 0;
            continue;
        }

        if (serialCmdLen < sizeof(serialCmdBuffer) - 1) {
            serialCmdBuffer[serialCmdLen++] = c;
        }
    }
}

/**
 * @brief 处理调试串口测试命令。
 * @details 该函数是人工联调入口，覆盖注入姿态、外设自检、GPIO 观察等诊断能力。
 * @param line 一整行命令文本（以换行分隔）
 */
void processSerialTestCommand(const char* line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0 || strcmp(line, "test help") == 0) {
        LOGI("[TEST] 命令:");
        LOGI("[TEST]  test on              -> 开启姿态测试注入(异常)");
        LOGI("[TEST]  test off             -> 关闭姿态测试注入");
        LOGI("[TEST]  test abnormal        -> 注入异常姿态");
        LOGI("[TEST]  test normal          -> 注入正常姿态");
        LOGI("[TEST]  test monitor on/off  -> 开启/关闭检测");
        LOGI("[TEST]  test mask <0..7>     -> 设置提醒输出位掩码");
        LOGI("[TEST]  test cooldown <ms>   -> 设置提醒冷却(1000..600000)");
        LOGI("[TEST]  test alert           -> 立即触发一次提醒输出");
        LOGI("[TEST]  test voice           -> 强制语音播报(忽略mask)");
        LOGI("[TEST]  test voice p00       -> 用frameParam=0x00发声");
        LOGI("[TEST]  test voice p21       -> 用frameParam=0x21发声");
        LOGI("[TEST]  test voice demo      -> 发送商家Arduino示例固定数据帧");
        LOGI("[TEST]  test voice zh        -> 发送GBK中文短句(欢迎使用)");
        LOGI("[TEST]  test voice raw       -> 原始串口发送 [v8][t3]hello");
        LOGI("[TEST]  test voice rawcrlf   -> 原始串口发送并追加CRLF");
        LOGI("[TEST]  test voice log on/off-> 打开/关闭语音回传日志");
        LOGI("[TEST]  test led high/low    -> 锁定板载WS2812亮/灭");
        LOGI("[TEST]  test led blink       -> 板载WS2812快速闪烁3次");
        LOGI("[TEST]  test led auto        -> 恢复LED自动状态显示");
        LOGI("[TEST]  test ec11            -> 打印EC11引脚电平与阈值");
        LOGI("[TEST]  test ec11 watch      -> 连续5秒采样EC11引脚变化");
        LOGI("[TEST]  test ec11 state      -> 连续5秒打印KEY原始/消抖/待触发状态");
        LOGI("[TEST]  test ec11 scan       -> 扫描4..12脚并输出变化(按键/旋转定位)");
        LOGI("[TEST]  test ec11 scanext    -> 扫描扩展引脚集合定位EC11信号");
        LOGI("[TEST]  test pin scanall     -> 扫描常用引脚集合并输出变化");
        LOGI("[TEST]  test pin <gpio>      -> 读取任意GPIO电平(内部上拉)");
        LOGI("[TEST]  test pin watch <gpio>-> 连续5秒采样任意GPIO变化");
        LOGI("[TEST]  status               -> 打印当前测试状态");
        return;
    }

    if (strcmp(line, "status") == 0) {
        LOGI("[TEST] inject=%s, abnormal=%s, monitor=%s, mask=%u, cooldownMs=%lu",
             testForcePostureEnabled ? "on" : "off",
             testForceAbnormal ? "true" : "false",
             monitoringEnabled ? "on" : "off",
             timer_getConfig()->alertModeMask,
             timer_getConfig()->cooldownMs);
        LOGI("[TEST] pirReady=%s, personPresent=%s, lightReady=%s, lux=%d",
             sensors_isPirReady() ? "true" : "false",
             sensors_isPersonPresent() ? "true" : "false",
             sensors_isLightSensorReady() ? "true" : "false",
             sensors_getLightLevel());
        return;
    }

    if (strcmp(line, "test on") == 0) {
        testForcePostureEnabled = true;
        testForceAbnormal = true;
        LOGI("[TEST] 姿态测试注入=ON (abnormal=true)");
        return;
    }

    if (strcmp(line, "test off") == 0) {
        testForcePostureEnabled = false;
        LOGI("[TEST] 姿态测试注入=OFF");
        return;
    }

    if (strcmp(line, "test abnormal") == 0) {
        testForcePostureEnabled = true;
        testForceAbnormal = true;
        LOGI("[TEST] 注入姿态=abnormal");
        return;
    }

    if (strcmp(line, "test normal") == 0) {
        testForcePostureEnabled = true;
        testForceAbnormal = false;
        LOGI("[TEST] 注入姿态=normal");
        return;
    }

    if (strncmp(line, "test monitor ", 13) == 0) {
        const char* v = line + 13;
        if (strcmp(v, "on") == 0) {
            monitoringEnabled = true;
            LOGI("[TEST] monitoring=ON");
        } else if (strcmp(v, "off") == 0) {
            monitoringEnabled = false;
            LOGI("[TEST] monitoring=OFF");
        } else {
            LOGW("[TEST] monitor 参数无效: %s", v);
        }
        return;
    }

    if (strncmp(line, "test mask ", 10) == 0) {
        unsigned long v = strtoul(line + 10, NULL, 10);
        timer_setAlertModeMask((uint8_t)v);
        timer_applyConfig();
        LOGI("[TEST] alertModeMask=%u", timer_getConfig()->alertModeMask);
        return;
    }

    if (strncmp(line, "test cooldown ", 14) == 0) {
        unsigned long v = strtoul(line + 14, NULL, 10);
        timer_setCooldownMs(v);
        LOGI("[TEST] cooldownMs=%lu", timer_getConfig()->cooldownMs);
        return;
    }

    if (strcmp(line, "test alert") == 0) {
        alerts_triggerBuzzerPulse(BUZZER_PULSE_MS);
        if (alerts_voiceEnabled()) {
            voice_speak("请调整坐姿");
        }
        // 这里不直接改提醒策略内部状态，只做一次“立即输出”用于人工验证。
        // 这样可以避免测试命令污染正常运行中的冷却时序。
        LOGI("[TEST] 手动触发一次提醒");
        return;
    }

    if (strcmp(line, "test voice") == 0) {
        voice_speak("hello");
        LOGI("[TEST] 强制语音播报: hello");
        return;
    }

    if (strcmp(line, "test voice p00") == 0) {
        voice_speakWithParam("hello", 0x00);
        LOGI("[TEST] 语音参数测试: frameParam=0x00");
        return;
    }

    if (strcmp(line, "test voice p21") == 0) {
        voice_speakWithParam("hello", 0x21);
        LOGI("[TEST] 语音参数测试: frameParam=0x21");
        return;
    }

    if (strcmp(line, "test voice demo") == 0) {
        voice_sendDemoPacket();
        LOGI("[TEST] 已发送商家示例数据帧");
        return;
    }

    if (strcmp(line, "test voice zh") == 0) {
        // 与 test voice demo 同协议，固定发送“欢迎使用”中文包，用于排除源码编码差异。
        static const uint8_t zhPacket[] = {
            0xFD, 0x00, 0x13, 0x01, 0x00,
            0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
            0xBB, 0xB6, 0xD3, 0xAD, 0xCA, 0xB9, 0xD3, 0xC3, 0xF6
        };
        voice_sendPacket(zhPacket, sizeof(zhPacket));
        LOGI("[TEST] 已发送中文测试包(欢迎使用)");
        return;
    }

    if (strcmp(line, "test voice raw") == 0) {
        voice_speakRaw("[v8][t3]hello", false);
        LOGI("[TEST] 原始串口发送: [v8][t3]hello");
        return;
    }

    if (strcmp(line, "test voice rawcrlf") == 0) {
        voice_speakRaw("[v8][t3]hello", true);
        LOGI("[TEST] 原始串口发送+CRLF: [v8][t3]hello");
        return;
    }

    if (strcmp(line, "test voice log on") == 0) {
        voice_setFeedbackLogEnabled(true);
        LOGI("[TEST] 语音回传日志=ON");
        return;
    }

    if (strcmp(line, "test voice log off") == 0) {
        voice_setFeedbackLogEnabled(false);
        LOGI("[TEST] 语音回传日志=OFF");
        return;
    }

    if (strcmp(line, "test led high") == 0) {
        alerts_lockIndicator(true, 64, 64, 64);
        alerts_setColor(64, 64, 64);
        LOGI("[TEST] LED indicator = ON");
        return;
    }

    if (strcmp(line, "test led low") == 0) {
        alerts_lockIndicator(true, 0, 0, 0);
        alerts_setColor(0, 0, 0);
        LOGI("[TEST] LED indicator = OFF");
        return;
    }

    if (strcmp(line, "test led blink") == 0) {
        alerts_lockIndicator(false, 0, 0, 0);
        blinkLED(3, 120);
        LOGI("[TEST] LED blink done");
        return;
    }

    if (strcmp(line, "test led auto") == 0) {
        alerts_lockIndicator(false, 0, 0, 0);
        LOGI("[TEST] LED indicator = AUTO");
        return;
    }

    if (strcmp(line, "test ec11") == 0) {
#if ENABLE_EC11
        int s1 = digitalRead(EC11_S1_PIN);
        int s2 = digitalRead(EC11_S2_PIN);
        int key = digitalRead(EC11_KEY_PIN);
        char l1[22];
        char l2[22];
        snprintf(l1, sizeof(l1), "S1:%d S2:%d", s1, s2);
        snprintf(l2, sizeof(l2), "KEY:%d", key);
        display_showMessage(l1, l2);
        refreshDisplayForTest();
        LOGI("[TEST][EC11] S1(GPIO%d)=%d S2(GPIO%d)=%d KEY(GPIO%d)=%d",
             EC11_S1_PIN, s1, EC11_S2_PIN, s2, EC11_KEY_PIN, key);
        LOGI("[TEST][EC11] short=%u..%ums, long>=%ums, click在按键释放时触发",
             (unsigned int)EC11_DEBOUNCE_MS,
             EC11_LONG_PRESS_MS - 1,
             EC11_LONG_PRESS_MS);
#else
        LOGI("[TEST][EC11] ENABLE_EC11=0");
#endif
        return;
    }

    if (strcmp(line, "test ec11 watch") == 0) {
#if ENABLE_EC11
        int lastS1 = digitalRead(EC11_S1_PIN);
        int lastS2 = digitalRead(EC11_S2_PIN);
        int lastKey = digitalRead(EC11_KEY_PIN);
        unsigned long start = millis();
        display_showMessage("EC11 watch 5s", "operate now");
        refreshDisplayForTest();
        LOGI("[TEST][EC11] watch start 5s, 初始 S1=%d S2=%d KEY=%d", lastS1, lastS2, lastKey);
        while (millis() - start < 5000UL) {
            int s1 = digitalRead(EC11_S1_PIN);
            int s2 = digitalRead(EC11_S2_PIN);
            int key = digitalRead(EC11_KEY_PIN);
            if (s1 != lastS1 || s2 != lastS2 || key != lastKey) {
                char l1[22];
                char l2[22];
                snprintf(l1, sizeof(l1), "S1:%d S2:%d", s1, s2);
                snprintf(l2, sizeof(l2), "KEY:%d", key);
                display_showMessage(l1, l2);
                refreshDisplayForTest();
                LOGI("[TEST][EC11] t=%lums S1=%d S2=%d KEY=%d",
                     millis() - start, s1, s2, key);
                lastS1 = s1;
                lastS2 = s2;
                lastKey = key;
            }
            refreshDisplayForTest();
            delay(20);
        }
        LOGI("[TEST][EC11] watch end");
#else
        LOGI("[TEST][EC11] ENABLE_EC11=0");
#endif
        return;
    }

    if (strcmp(line, "test ec11 state") == 0) {
#if ENABLE_EC11
        unsigned long start = millis();
        display_showMessage("EC11 state 5s", "press key now");
        refreshDisplayForTest();
        LOGI("[TEST][EC11] state start 5s, KEY(GPIO%d)", EC11_KEY_PIN);
        while (millis() - start < 5000UL) {
            int key = mode_getKeyLevel();
            ModeClickEvent click = mode_peekClickEvent();
            LOGI("[TEST][EC11] t=%lums key=%d raw=%s stable=%s pending=%s",
                 millis() - start,
                 key,
                 mode_getRawPressed() ? "true" : "false",
                 mode_getStablePressed() ? "true" : "false",
                 click == MODE_CLICK_LONG ? "long" : (click == MODE_CLICK_SHORT ? "short" : "none"));
            LOGI("[TEST][EC11] mode=%s adjust=%s lock=%s",
                 mode_getName(),
                 timer_isAdjustMode() ? "true" : "false",
                 mode_isRotationLocked() ? "true" : "false");
            LOGI("[TEST][EC11] rawDelta=%d pendingDelta=%d",
                 mode_getRawDeltaSnapshot(),
                 mode_getPendingDeltaSnapshot());
            refreshDisplayForTest();
            delay(100);
        }
        LOGI("[TEST][EC11] state end");
#else
        LOGI("[TEST][EC11] ENABLE_EC11=0");
#endif
        return;
    }

    if (strcmp(line, "test ec11 scan") == 0) {
#if ENABLE_EC11
        const int pins[] = {4, 5, 6, 7, 8, 9, 10, 11, 12};
        const int pinCount = (int)(sizeof(pins) / sizeof(pins[0]));
        int lastVals[9];

        for (int i = 0; i < pinCount; ++i) {
            pinMode(pins[i], INPUT_PULLUP);
            lastVals[i] = digitalRead(pins[i]);
        }

        display_showMessage("EC11 scan 8s", "press/rotate now");
        refreshDisplayForTest();
        LOGI("[TEST][EC11] scan start 8s (GPIO4..12), 请按键并旋转");

        unsigned long start = millis();
        while (millis() - start < 8000UL) {
            for (int i = 0; i < pinCount; ++i) {
                int v = digitalRead(pins[i]);
                if (v != lastVals[i]) {
                    char l2[22];
                    snprintf(l2, sizeof(l2), "GPIO%d=%d", pins[i], v);
                    display_showMessage("EC11 scan change", l2);
                    refreshDisplayForTest();
                    LOGI("[TEST][EC11] t=%lums GPIO%d=%d", millis() - start, pins[i], v);
                    lastVals[i] = v;
                }
            }
            refreshDisplayForTest();
            delay(20);
        }

        LOGI("[TEST][EC11] scan end");
#else
        LOGI("[TEST][EC11] ENABLE_EC11=0");
#endif
        return;
    }

    if (strcmp(line, "test ec11 scanext") == 0) {
#if ENABLE_EC11
        const int pins[] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 21, 38, 39, 40, 41, 42, 47};
        const int pinCount = (int)(sizeof(pins) / sizeof(pins[0]));
        int lastVals[16];

        for (int i = 0; i < pinCount; ++i) {
            pinMode(pins[i], INPUT_PULLUP);
            lastVals[i] = digitalRead(pins[i]);
        }

        display_showMessage("EC11 scanext 10s", "press/rotate now");
        refreshDisplayForTest();
        LOGI("[TEST][EC11] scanext start 10s (7,8,9,10,11,12,13,14,16,21,38,39,40,41,42,47)");

        unsigned long start = millis();
        while (millis() - start < 10000UL) {
            for (int i = 0; i < pinCount; ++i) {
                int v = digitalRead(pins[i]);
                if (v != lastVals[i]) {
                    char l2[22];
                    snprintf(l2, sizeof(l2), "GPIO%d=%d", pins[i], v);
                    display_showMessage("SCANEXT change", l2);
                    refreshDisplayForTest();
                    LOGI("[TEST][EC11] scanext t=%lums GPIO%d=%d", millis() - start, pins[i], v);
                    lastVals[i] = v;
                }
            }
            refreshDisplayForTest();
            delay(20);
        }
        LOGI("[TEST][EC11] scanext end");
#else
        LOGI("[TEST][EC11] ENABLE_EC11=0");
#endif
        return;
    }

    if (strcmp(line, "test pin scanall") == 0) {
        // 仅扫描常见且相对安全的可用脚位，避开 PSRAM/USB/UART0/LOG 等保留脚，
        // 防止误配置导致系统复位。
        const int pins[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
            21, 38, 39, 40, 41, 42, 47, 48
        };
        const int pinCount = (int)(sizeof(pins) / sizeof(pins[0]));
        int lastVals[27];

        for (int i = 0; i < pinCount; ++i) {
            pinMode(pins[i], INPUT_PULLUP);
            lastVals[i] = digitalRead(pins[i]);
        }

        display_showMessage("PIN scanall 12s", "touch/operate now");
        refreshDisplayForTest();
        LOGI("[TEST][PIN] scanall start 12s");

        unsigned long start = millis();
        while (millis() - start < 12000UL) {
            for (int i = 0; i < pinCount; ++i) {
                int v = digitalRead(pins[i]);
                if (v != lastVals[i]) {
                    char l2[22];
                    snprintf(l2, sizeof(l2), "GPIO%d=%d", pins[i], v);
                    display_showMessage("PIN scanall change", l2);
                    refreshDisplayForTest();
                    LOGI("[TEST][PIN] scanall t=%lums GPIO%d=%d", millis() - start, pins[i], v);
                    lastVals[i] = v;
                }
            }
            refreshDisplayForTest();
            yield();
            delay(20);
        }

        LOGI("[TEST][PIN] scanall end");
        return;
    }

    if (strncmp(line, "test pin watch ", 15) == 0) {
        long pin = strtol(line + 15, NULL, 10);
        if (pin < 0 || pin > 48) {
            LOGW("[TEST][PIN] 无效GPIO: %ld", pin);
            return;
        }
        pinMode((int)pin, INPUT_PULLUP);
        int last = digitalRead((int)pin);
        unsigned long start = millis();
        char l1[22];
        char l2[22];
        snprintf(l1, sizeof(l1), "watch GPIO%ld", pin);
        snprintf(l2, sizeof(l2), "init=%d", last);
        display_showMessage(l1, l2);
        refreshDisplayForTest();
        LOGI("[TEST][PIN] watch GPIO%ld start 5s, 初始=%d", pin, last);
        while (millis() - start < 5000UL) {
            int v = digitalRead((int)pin);
            if (v != last) {
                snprintf(l1, sizeof(l1), "GPIO%ld=%d", pin, v);
                display_showMessage("PIN change", l1);
                refreshDisplayForTest();
                LOGI("[TEST][PIN] t=%lums GPIO%ld=%d", millis() - start, pin, v);
                last = v;
            }
            refreshDisplayForTest();
            delay(20);
        }
        LOGI("[TEST][PIN] watch GPIO%ld end", pin);
        return;
    }

    if (strncmp(line, "test pin ", 9) == 0) {
        long pin = strtol(line + 9, NULL, 10);
        if (pin < 0 || pin > 48) {
            LOGW("[TEST][PIN] 无效GPIO: %ld", pin);
            return;
        }
        pinMode((int)pin, INPUT_PULLUP);
        int v = digitalRead((int)pin);
        char l1[22];
        snprintf(l1, sizeof(l1), "GPIO%ld=%d", pin, v);
        display_showMessage("PIN snapshot", l1);
        refreshDisplayForTest();
        LOGI("[TEST][PIN] GPIO%ld=%d (INPUT_PULLUP)", pin, v);
        return;
    }

    LOGW("[TEST] 未知命令: %s", line);
}

/**
 * @brief 根据 EC11 按键事件处理模式相关动作。
 * @details 同一个按键在不同模式有不同语义，因此先判模式再判短按/长按，避免误操作。
 */
void handleModeAndTimerInput() {
    // 事件是一次性消费模型：本轮不处理就会丢失，因此先取事件再判断模式。
    ModeClickEvent click = mode_takeClickEvent();
    if (click == MODE_CLICK_NONE) {
        return;
    }

    LOGI("[EC11] click=%s mode=%s",
         click == MODE_CLICK_LONG ? "long" : "short",
         mode_getName());

    // 模式0（坐姿模式）：短按切检测开关，长按轮换提醒输出组合。
    if (mode_getCurrent() == MODE_POSTURE) {
        if (click == MODE_CLICK_SHORT) {
            monitoringEnabled = !monitoringEnabled;
            voice_speak(monitoringEnabled ? "坐姿检测已开启" : "坐姿检测已关闭");
            display_showMessage(monitoringEnabled ? "Monitor ON" : "Monitor OFF", NULL);
            LOGI("[EC11] monitor=%s", monitoringEnabled ? "ON" : "OFF");
            requestImmediatePublish();
        } else if (click == MODE_CLICK_LONG) {
            // 轮换顺序：LED -> LED+蜂鸣器 -> LED+蜂鸣器+语音 -> LED。
            // 这样设计是让用户从“最轻提醒”逐步增强，避免一上来就语音打扰。
            uint8_t modeMask = timer_getConfig()->alertModeMask;
            if (modeMask == ALERT_MODE_LED) {
                modeMask = ALERT_MODE_LED | ALERT_MODE_BUZZER;
            } else if (modeMask == (ALERT_MODE_LED | ALERT_MODE_BUZZER)) {
                modeMask = ALERT_MODE_LED | ALERT_MODE_BUZZER | ALERT_MODE_VOICE;
            } else {
                modeMask = ALERT_MODE_LED;
            }
            timer_setAlertModeMask(modeMask);
            timer_applyConfig();
            display_showMessage("Alert mode changed", NULL);
            LOGI("[EC11] alertModeMask=%u", timer_getConfig()->alertModeMask);
            requestImmediatePublish();
            return;
        }
    }

    // 模式2（定时器模式）：
    // - 长按：进入/退出调时模式（旋钮改时长）；
    // - 短按：在 开始/暂停/恢复 之间切换。
    if (mode_getCurrent() == MODE_TIMER) {
        if (click == MODE_CLICK_LONG) {
            bool newAdjustMode = !timer_isAdjustMode();
            timer_setAdjustMode(newAdjustMode);
            mode_setRotationLocked(newAdjustMode);
            display_showMessage(newAdjustMode ? "Timer adjust ON" : "Timer adjust OFF", NULL);
            LOGI("[EC11] timerAdjust=%s", newAdjustMode ? "ON" : "OFF");
            requestImmediatePublish();
            return;
        }

        TimerState tState = timer_getState();
        if (tState == TIMER_IDLE || tState == TIMER_DONE) {
            timer_start();
            display_showMessage("Timer started", NULL);
            LOGI("[EC11] timer=START total=%lus", timer_getConfig()->timerDurationSec);
            requestImmediatePublish();
        } else if (tState == TIMER_RUNNING) {
            timer_pause();
            display_showMessage("Timer paused", NULL);
            LOGI("[EC11] timer=PAUSE remain=%lus", timer_getRemainSec());
            requestImmediatePublish();
        } else if (tState == TIMER_PAUSED) {
            timer_resume();
            display_showMessage("Timer resumed", NULL);
            LOGI("[EC11] timer=RESUME remain=%lus", timer_getRemainSec());
            requestImmediatePublish();
        }
    }
}

/**
 * @brief 在测试命令执行期间主动刷新显示。
 * @details 测试流程里存在短时阻塞，手动刷新可让操作者即时看到引脚/状态变化。
 */
void refreshDisplayForTest() {
    const K230Data* data = k230_getData();
    const char* posture = (data != NULL) ? data->postureType : NULL;
    bool abnormal = (data != NULL) ? data->isAbnormal : false;
    display_update(mode_getCurrent(), posture, abnormal);
}


// =============================================================================
// MQTT 回调函数
// =============================================================================

/**
 * @brief MQTT 消息统一入口，根据主题路由到对应处理函数。
 * @details 先做 topic 级分流再解析 payload，可减少无关消息的解析开销，并避免回执主题被误当命令。
 * @param topic MQTT 主题字符串
 * @param payload MQTT 原始载荷字节流
 * @param length 载荷长度
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    LOGI("[MQTT 收到消息]");
    LOGI("  主题: %s", topic);
    
    // 将 payload 转为字符串
    char message[512];
    unsigned int copyLen = length < sizeof(message) - 1 ? length : sizeof(message) - 1;
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';
    LOGI("  内容: %s", message);
    
    // 分发顺序说明：
    // 1) 先排除 *_reply，避免回执被当成下行命令造成“设备自己处理自己的回包”；
    // 2) 再匹配 set/get，下发控制与查询走不同处理路径。
    if (strstr(topic, "property/set") != NULL && strstr(topic, "set_reply") == NULL) {
        // 云端属性设置命令：会改本地状态并回 set_reply。
        LOGI("  → 处理 property/set 命令");
        handlePropertySet(message);
    } else if (strstr(topic, "property/get") != NULL && strstr(topic, "get_reply") == NULL) {
        // 云端属性查询请求：只读取当前状态并回 get_reply。
        LOGI("  → 处理 property/get 请求");
        handlePropertyGet(message);
    }
    // post_reply 等其他主题仅打印日志，不需要处理
}

// =============================================================================
// 云端命令处理
// =============================================================================

/**
 * @brief 处理云端下发的 property/set 命令
 * 
 * 预期收到的 JSON 格式：
 * {"id":"123","version":"1.0","params":{"isPosture":{"value":true}}}
 * 
 * 处理逻辑：
 * 1. 提取消息 id
 * 2. 解析 monitoringEnabled/currentMode 等属性值并更新本地状态
 * 3. 发送 set_reply 回复确认
 * 
 * @param message 收到的 JSON 消息字符串
 */
void handlePropertySet(const char* message) {
    LOGI("[handlePropertySet] 开始解析命令...");

    // 解析流程总览：
    // - 先拿到消息 id（用于回复同一事务）；
    // - 再按属性逐项尝试解析（允许一次下发多个字段）；
    // - 最后统一 applyConfig + set_reply，确保行为与回执一致。
    
    // 步骤 1：提取消息 id
    char msgId[32] = "0";
    mqtt_extractId(message, msgId, sizeof(msgId));
    LOGI("  消息 ID: %s", msgId);
    
    const char* p = NULL;
    bool boolValue = false;
    unsigned long numValue = 0;
    bool hasMonitoringCommand = false;

    // 优先解析新字段 monitoringEnabled；若存在则标记 hasMonitoringCommand。
    if (jsonFindParamValueStart(message, PROP_ID_MONITORING_ENABLED, &p) && jsonParseBool(p, &boolValue)) {
        bool oldValue = monitoringEnabled;
        monitoringEnabled = boolValue;
        hasMonitoringCommand = true;
        if (oldValue != monitoringEnabled) {
            voice_speak(monitoringEnabled ? "坐姿检测已开启" : "坐姿检测已关闭");
        }
        LOGI("  %s: %s", PROP_ID_MONITORING_ENABLED, monitoringEnabled ? "true" : "false");
        requestImmediatePublish();
    }

    // 兼容旧字段 isPosture：仅在未收到 monitoringEnabled 时生效，
    // 这样可避免同一报文同时带新旧字段时互相覆盖。
    if (!hasMonitoringCommand && jsonFindParamValueStart(message, PROP_ID_IS_POSTURE, &p) && jsonParseBool(p, &boolValue)) {
        bool oldValue = monitoringEnabled;
        monitoringEnabled = boolValue;
        if (oldValue != monitoringEnabled) {
            voice_speak(monitoringEnabled ? "坐姿检测已开启" : "坐姿检测已关闭");
        }
        LOGI("  %s(legacy): %s", PROP_ID_IS_POSTURE, monitoringEnabled ? "true" : "false");
        requestImmediatePublish();
    }

    // 模式切换需要先做越界保护；切离定时器模式时要同步退出“调时锁定”，
    // 否则旋钮可能仍被锁在旧语义下。
    if (jsonFindParamValueStart(message, PROP_ID_CURRENT_MODE, &p) && jsonParseULong(p, &numValue)) {
        if (numValue < MODE_COUNT) {
            if (mode_setCurrent((SystemMode)numValue)) {
                if (mode_getCurrent() != MODE_TIMER) {
                    timer_setAdjustMode(false);
                    mode_setRotationLocked(false);
                }
            }
            LOGI("  %s: %lu", PROP_ID_CURRENT_MODE, numValue);
            requestImmediatePublish();
        } else {
            LOGW("  %s 越界: %lu (有效范围 0..%d)", PROP_ID_CURRENT_MODE, numValue, MODE_COUNT - 1);
        }
    }

    // 提醒输出策略：LED/蜂鸣器/语音位掩码。
    if (jsonFindParamValueStart(message, PROP_ID_ALERT_MODE_MASK, &p) && jsonParseULong(p, &numValue)) {
        timer_setAlertModeMask((uint8_t)numValue);
        LOGI("  %s: %u", PROP_ID_ALERT_MODE_MASK, timer_getConfig()->alertModeMask);
        requestImmediatePublish();
    }

    // 冷却参数直接影响重复提醒频率，是行为策略核心参数。
    if (jsonFindParamValueStart(message, PROP_ID_COOLDOWN_MS, &p) && jsonParseULong(p, &numValue)) {
        timer_setCooldownMs(numValue);
        LOGI("  %s: %lu", PROP_ID_COOLDOWN_MS, timer_getConfig()->cooldownMs);
        requestImmediatePublish();
    }

    // 兼容历史字段 remindIntervalMs，并映射到当前 cooldownMs。
    if (jsonFindParamValueStart(message, "\"remindIntervalMs\"", &p) && jsonParseULong(p, &numValue)) {
        timer_setCooldownMs(numValue);
        LOGI("  remindIntervalMs(legacy->cooldownMs): %lu", timer_getConfig()->cooldownMs);
        requestImmediatePublish();
    }

    // 定时器时长配置：内部会做上下限钳制，避免异常值。
    if (jsonFindParamValueStart(message, PROP_ID_TIMER_DURATION_SEC, &p) && jsonParseULong(p, &numValue)) {
        timer_setTimerDurationSec(numValue);
        LOGI("  %s: %lu", PROP_ID_TIMER_DURATION_SEC, timer_getConfig()->timerDurationSec);
        requestImmediatePublish();
    }

    // 运行态控制：true 表示开始/继续，false 表示暂停。
    if (jsonFindParamValueStart(message, PROP_ID_TIMER_RUNNING, &p) && jsonParseBool(p, &boolValue)) {
        timer_setTimerRunning(boolValue);
        LOGI("  %s: %s", PROP_ID_TIMER_RUNNING, boolValue ? "true" : "false");
        requestImmediatePublish();
    }

    // 自检触发字段只看“出现与否”，数值本身用于日志追踪来源。
    if (jsonFindParamValueStart(message, PROP_ID_SELF_TEST, &p) && jsonParseULong(p, &numValue)) {
        runActuatorSelfTest();
        LOGI("  %s: %lu", PROP_ID_SELF_TEST, numValue);
    }

    timer_applyConfig();
    
    // 步骤 3：发送 set_reply 回复
    mqtt_sendSetReply(msgId);
    
    blinkLED(2, 50);
}

/**
 * @brief 处理云端下发的 property/get 请求
 * 
 * 预期收到的 JSON 格式：
 * {"id":"456","version":"1.0","params":["isPosture"]}
 * 
 * 处理逻辑：
 * 1. 提取消息 id
 * 2. 构建当前属性值 JSON
 * 3. 发送 get_reply 回复
 * 
 * @param message 收到的 JSON 消息字符串
 */
void handlePropertyGet(const char* message) {
    LOGI("[handlePropertyGet] 开始处理查询...");

    // 数据回报思路：
    // - 返回当前“行为状态”（是否正常、当前模式、定时器运行态）；
    // - 同时返回“策略配置”（提醒掩码、冷却、时长、配置版本）。
    // 云端据此既能展示实时状态，也能判断配置是否已下发成功。
    
    // 步骤 1：提取消息 id
    char msgId[32] = "0";
    mqtt_extractId(message, msgId, sizeof(msgId));
    LOGI("  消息 ID: %s", msgId);
    
    // 步骤 2：构建当前属性值
    const K230Data* data = k230_getData();
    unsigned long now = millis();
    bool noPerson = (strcmp(data->postureType, "no_person") == 0) || (sensors_isPirReady() && !sensors_isPersonPresent());
    PublishDecision decision = evaluatePeriodicPublishDecision(data, now, noPerson);
    char params[512];
    sprintf(
        params,
        "{"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%u},"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%d},"
        "\"%s\":{\"value\":%u},"
        "\"%s\":{\"value\":%lu},"
        "\"%s\":{\"value\":%lu},"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%lu}"
        "}",
        PROP_ID_IS_POSTURE, decision.isPostureOk ? "true" : "false",
        PROP_ID_MONITORING_ENABLED, monitoringEnabled ? "true" : "false",
        PROP_ID_CURRENT_MODE, (unsigned int)mode_getCurrent(),
        PROP_ID_PERSON_PRESENT, noPerson ? "false" : "true",
        PROP_ID_AMBIENT_LUX, sensors_getLightLevel(),
        PROP_ID_ALERT_MODE_MASK, timer_getConfig()->alertModeMask,
        PROP_ID_COOLDOWN_MS, timer_getConfig()->cooldownMs,
        PROP_ID_TIMER_DURATION_SEC, timer_getConfig()->timerDurationSec,
        PROP_ID_TIMER_RUNNING, timer_getState() == TIMER_RUNNING ? "true" : "false",
        PROP_ID_CFG_VERSION, timer_getConfig()->cfgVersion
    );
    
    LOGI("  回复内容: %s", params);
    
    // 步骤 3：发送 get_reply
    mqtt_sendGetReply(msgId, params);
    
    blinkLED(1, 50);
}

// =============================================================================
// 定时属性上报
// =============================================================================

/**
 * @brief 评估本轮周期上报应使用的坐姿结果及原因。
 * @details 把上报判定集中到单函数，保证日志、云端数据、异常兜底三者一致。
 * @param data 当前 K230 数据
 * @param now 当前毫秒时间戳
 * @param noPerson 当前是否可判定为无人
 * @return PublishDecision 包含最终布尔值、决策原因与数据时效信息
 */
PublishDecision evaluatePeriodicPublishDecision(const K230Data* data, unsigned long now, bool noPerson) {
    PublishDecision decision = {true, DECISION_FALLBACK_TRUE, 0};

    // 分支顺序有意设计为“先总开关，再启动态，再链路健康，再实时帧”：
    // 越靠前的是越高优先级的全局约束。

    // 1) 检测功能被关闭：按产品策略上报 true，避免在停用期间制造异常告警。
    if (!monitoringEnabled) {
        decision.reason = DECISION_MONITOR_OFF;
        return decision;
    }

    if (noPerson) {
        decision.reason = DECISION_NO_PERSON;
        decision.isPostureOk = true;
        return decision;
    }

    // 2) 系统刚启动且还没拿到首帧：进入等待态，不立刻依据空数据判断异常。
    if (k230_getFrameCount() == 0) {
        decision.reason = DECISION_STARTUP_WAIT;
        return decision;
    }

    // 3) 当前帧无效或链路超时：先尝试“保鲜窗口”策略。
    if (!data->valid || k230_isTimeout(K230_TIMEOUT_MS)) {
        if (data->valid) {
            decision.dataAgeMs = now - data->lastUpdateTime;
            // 3.1) 在保鲜窗口内，沿用最近一次有效姿态，减少短抖动导致的云端翻转。
            if (decision.dataAgeMs <= K230_STALE_HOLD_MS) {
                decision.reason = DECISION_STALE_HOLD;
                decision.isPostureOk = !data->isAbnormal;
                return decision;
            }
        }
        // 3.2) 超出保鲜窗口或无可用历史帧：回退到安全默认 true。
        decision.reason = DECISION_FALLBACK_TRUE;
        decision.isPostureOk = true;
        return decision;
    }

    // 4) 链路与数据都健康：使用实时帧判定。
    decision.reason = DECISION_LIVE_FRAME;
    decision.isPostureOk = !data->isAbnormal;
    return decision;
}

/**
 * @brief 将上报决策原因枚举转换为日志可读文本。
 * @details 统一映射可避免各处手写字符串不一致，便于排查现场问题。
 * @param reason 决策原因枚举
 * @return const char* 对应文本标签
 */
const char* publishDecisionReasonText(PublishDecisionReason reason) {
    switch (reason) {
        case DECISION_MONITOR_OFF:
            return "MONITOR_OFF";
        case DECISION_NO_PERSON:
            return "NO_PERSON";
        case DECISION_STARTUP_WAIT:
            return "STARTUP_WAIT";
        case DECISION_STALE_HOLD:
            return "STALE_HOLD";
        case DECISION_FALLBACK_TRUE:
            return "FALLBACK_TRUE";
        case DECISION_LIVE_FRAME:
            return "LIVE_FRAME";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 定时发布设备属性到 OneNET。
 * @details 先用 evaluatePeriodicPublishDecision 统一得出上报值，再拆分为状态包与配置包发布，降低单包过长失败风险。
 */
void handlePeriodicPublish() {
    // 检查 MQTT 是否连接
    if (!mqttClient.connected()) {
        LOGD("[定时上报] MQTT 未连接，跳过");
        return;
    }
    
    char statusParams[192];
    char configParams[224];
    char sensorParams[96];
    PublishDecision decision;
    buildPropertyPayloads(statusParams, sizeof(statusParams), configParams, sizeof(configParams), sensorParams, sizeof(sensorParams), &decision);
    const K230Data* data = k230_getData();
    unsigned long now = millis();

    // 先打印“为何这样上报”的原因，便于现场从日志反推判定链路。
    switch (decision.reason) {
        case DECISION_MONITOR_OFF:
            LOGD("[定时上报][%s] 检测已关闭，上报 isPosture=true", publishDecisionReasonText(decision.reason));
            break;
        case DECISION_NO_PERSON:
            LOGI("[定时上报][%s] 当前无人，上报 isPosture=true, personPresent=false",
                 publishDecisionReasonText(decision.reason));
            break;
        case DECISION_STARTUP_WAIT:
            LOGW("[定时上报][%s] 尚未收到K230有效首帧: now=%lums, grace=%dms, seenBytes=%s",
                 publishDecisionReasonText(decision.reason),
                 now,
                 K230_STARTUP_GRACE_MS,
                 k230_hasSeenAnyByte() ? "true" : "false");
            break;
        case DECISION_STALE_HOLD:
            LOGW("[定时上报][%s] K230 超时但在保鲜窗口(%lums<=%dms)，沿用最后状态 isPosture=%s",
                 publishDecisionReasonText(decision.reason),
                 decision.dataAgeMs,
                 K230_STALE_HOLD_MS,
                 decision.isPostureOk ? "true" : "false");
            break;
        case DECISION_FALLBACK_TRUE:
            LOGD("[定时上报][%s] K230 数据无效/超时且无可用保鲜状态，上报 isPosture=true",
                 publishDecisionReasonText(decision.reason));
            break;
        case DECISION_LIVE_FRAME:
            LOGI("[定时上报][%s] K230: %s, 异常=%s → isPosture=%s",
                 publishDecisionReasonText(decision.reason),
                 data->postureType,
                 data->isAbnormal ? "是" : "否",
                 decision.isPostureOk ? "true" : "false");
            break;
        default:
            LOGW("[定时上报][UNKNOWN] 未知决策原因，按安全默认值处理");
            break;
    }
    
    // PubSubClient 默认报文上限较小，拆分上报避免单包过长导致 publish() 失败
    // 第一包：高频状态（姿态、检测开关、当前模式、定时器运行态）。
    // 分包发布比单大包更稳，且云端可分别追踪状态变化与配置变化。
    mqtt_publishProperty(statusParams);
    mqtt_publishProperty(configParams);
    mqtt_publishProperty(sensorParams);
}

void buildPropertyPayloads(char* statusParams, size_t statusLen, char* configParams, size_t configLen, char* sensorParams, size_t sensorLen, PublishDecision* decisionOut) {
    const K230Data* data = k230_getData();
    unsigned long now = millis();
    bool noPerson = (strcmp(data->postureType, "no_person") == 0) || (sensors_isPirReady() && !sensors_isPersonPresent());
    PublishDecision decision = evaluatePeriodicPublishDecision(data, now, noPerson);

    snprintf(
        statusParams,
        statusLen,
        "{"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%u},"
        "\"%s\":{\"value\":%s},"
        "\"%s\":{\"value\":%s}"
        "}",
        PROP_ID_IS_POSTURE, decision.isPostureOk ? "true" : "false",
        PROP_ID_MONITORING_ENABLED, monitoringEnabled ? "true" : "false",
        PROP_ID_CURRENT_MODE, (unsigned int)mode_getCurrent(),
        PROP_ID_PERSON_PRESENT, noPerson ? "false" : "true",
        PROP_ID_TIMER_RUNNING, timer_getState() == TIMER_RUNNING ? "true" : "false"
    );

    snprintf(
        configParams,
        configLen,
        "{"
        "\"%s\":{\"value\":%u},"
        "\"%s\":{\"value\":%lu},"
        "\"%s\":{\"value\":%lu},"
        "\"%s\":{\"value\":%lu}"
        "}",
        PROP_ID_ALERT_MODE_MASK, timer_getConfig()->alertModeMask,
        PROP_ID_COOLDOWN_MS, timer_getConfig()->cooldownMs,
        PROP_ID_TIMER_DURATION_SEC, timer_getConfig()->timerDurationSec,
        PROP_ID_CFG_VERSION, timer_getConfig()->cfgVersion
    );

    snprintf(
        sensorParams,
        sensorLen,
        "{"
        "\"%s\":{\"value\":%d}"
        "}",
        PROP_ID_AMBIENT_LUX, sensors_getLightLevel()
    );

    if (decisionOut != NULL) {
        *decisionOut = decision;
    }
}

void requestImmediatePublish() {
    pendingImmediatePublish = true;
}

void syncImmediatePublishState() {
    if (lastSyncedMonitoringEnabled != monitoringEnabled) {
        lastSyncedMonitoringEnabled = monitoringEnabled;
        requestImmediatePublish();
    }

    int currentMode = (int)mode_getCurrent();
    if (lastSyncedMode != currentMode) {
        lastSyncedMode = currentMode;
        requestImmediatePublish();
    }

    int currentTimerState = (int)timer_getState();
    if (lastSyncedTimerState != currentTimerState) {
        lastSyncedTimerState = currentTimerState;
        requestImmediatePublish();
    }

    unsigned long currentCfgVersion = timer_getConfig()->cfgVersion;
    if (lastSyncedCfgVersion != currentCfgVersion) {
        lastSyncedCfgVersion = currentCfgVersion;
        requestImmediatePublish();
    }
}

bool flushPendingPropertyPublish(unsigned long now) {
    if (!pendingImmediatePublish || !mqttClient.connected()) {
        return false;
    }
    pendingImmediatePublish = false;
    handlePeriodicPublish();
    lastPublishTime = now;
    return true;
}

void syncTimerAdjustInputLock() {
    if (mode_getCurrent() == MODE_TIMER) {
        bool shouldLock = timer_isAdjustMode();
        if (mode_isRotationLocked() != shouldLock) {
            mode_setRotationLocked(shouldLock);
        }
        return;
    }

    if (timer_isAdjustMode()) {
        timer_setAdjustMode(false);
        requestImmediatePublish();
    }
    if (mode_isRotationLocked()) {
        mode_setRotationLocked(false);
    }
}

// =============================================================================
// 工具函数
// =============================================================================

/**
 * @brief 板载 LED 闪烁
 * 
 * @param times 闪烁次数
 * @param duration 每次亮/灭的时长（毫秒）
 */
void blinkLED(int times, int duration) {
    #if STATUS_LED_PIN >= 0
        for (int i = 0; i < times; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(duration);
            digitalWrite(STATUS_LED_PIN, LOW);
            if (i < times - 1) {
                delay(duration / 2);
            }
        }
    #endif
}
