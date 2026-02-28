#include "k230_parser.h"

/**
 * @brief k230_parser.cpp 实现说明
 *
 * 实现要点：
 * - 同时支持“按行分隔解析”和“JSON 流状态机解析”两条路径，提升串口抗噪与恢复能力。
 * - 核心 JSON 解析不依赖 ArduinoJson，而是使用 strstr/strchr/strtol/strtod 做字段定位与严格校验。
 * - 对 posture_type、is_abnormal、consecutive_abnormal、confidence 逐项校验，拒绝脏数据写入。
 * - 提供静默恢复机制：长时间无字节时重建 Serial1，避免链路偶发卡死。
 *
 * 内部状态变量含义（节选）：
 * - _k230Data：最近一次有效解析结果。
 * - _k230LineBuf/_k230BufPos：按行接收缓冲与写入位置。
 * - _k230Json*：流式 JSON 捕获状态（深度、字符串态、转义态）。
 * - _k230Reject*：各类解析失败计数，用于诊断输入格式问题。
 */

// =============================================================================
// 内部状态
// =============================================================================

static K230Data _k230Data = {"unknown", false, 0, 0.0, 0, false};
static char _k230LineBuf[512];
static int _k230BufPos = 0;
static unsigned long _k230FrameCount = 0;
static unsigned long _k230RxByteCount = 0;
static unsigned long _k230LastByteTime = 0;
static unsigned long _k230LastDiagLogTime = 0;
static unsigned long _k230LastRecoverTime = 0;
static unsigned long _k230RecoverCount = 0;
static bool _k230SeenAnyByte = false;
static bool _k230HasSyncedFrame = false;
static bool _k230LineTruncated = false;
static unsigned long _k230LineCount = 0;
static unsigned long _k230JsonLineCount = 0;
static unsigned long _k230NonJsonLineCount = 0;
static unsigned long _k230TruncatedLineCount = 0;
static unsigned long _k230ParseAttemptCount = 0;
static unsigned long _k230ParseSuccessCount = 0;
static unsigned long _k230RejectMissingPostureType = 0;
static unsigned long _k230RejectUnknownPostureType = 0;
static unsigned long _k230RejectIsAbnormal = 0;
static unsigned long _k230RejectConsecutive = 0;
static unsigned long _k230RejectConfidence = 0;
static unsigned long _k230RejectConfidenceRange = 0;
static unsigned long _k230RejectControlChar = 0;
static unsigned long _k230LastRawHintLogTime = 0;
static uint8_t _k230RecentBytes[24];
static int _k230RecentByteCount = 0;
static bool _k230JsonCapturing = false;
static int _k230JsonDepth = 0;
static bool _k230JsonInString = false;
static bool _k230JsonEscape = false;
static int _k230JsonPos = 0;
static char _k230JsonBuf[512];
static bool _k230PreferJsonStream = false;
static bool _k230ParsedSinceLastDelimiter = false;

// =============================================================================
// 内部辅助函数声明
// =============================================================================

static bool _k230Parse(const char* json);
static void _k230MarkFrameSynced();
static const char* _k230SkipSpaces(const char* p);
static const char* _k230FindKeyValueStart(const char* json, const char* keyPattern);
static bool _k230IsKnownPostureType(const char* postureType);
static bool _k230IsValueTerminator(char c);
static bool _k230ParseIntStrict(const char* valueStart, int* outValue);
static bool _k230ParseFloatStrict(const char* valueStart, float* outValue);
static void _k230ApplyTextFrame(const char* postureType, bool isAbnormal, int consecutiveAbnormal, float confidence);
static bool _k230ParseTextLine(const char* line);
static void _k230ResetJsonCapture();
static void _k230FeedJsonStream(char c);
static void _k230RecoverUart(const char* reason);
static void _k230OnRxByte(char c);
static void _k230HandleDelimiter();
static void _k230AppendLineByte(char c);
static void _k230LogDiagnostics(unsigned long now);

// =============================================================================
// 内部辅助函数实现
// =============================================================================

static void _k230MarkFrameSynced() {
    if (_k230HasSyncedFrame) {
        return;
    }

    _k230HasSyncedFrame = true;
    _k230BufPos = 0;
    _k230LineTruncated = false;
    _k230LineCount = 0;
    _k230JsonLineCount = 0;
    _k230NonJsonLineCount = 0;
    _k230TruncatedLineCount = 0;
    LOGI("[K230] 首个有效帧已同步，启动期噪声统计已清零");
}

static const char* _k230SkipSpaces(const char* p) {
    while (p != NULL && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    return p;
}

static const char* _k230FindKeyValueStart(const char* json, const char* keyPattern) {
    // 策略：先用 strstr 定位 key，再跳过空白和冒号，返回 value 的起始位置。
    // 该策略开销低、可控，适合 MCU 场景的已知结构 JSON。
    const char* key = strstr(json, keyPattern);
    if (key == NULL) return NULL;

    const char* p = key + strlen(keyPattern);
    p = _k230SkipSpaces(p);
    if (p == NULL || *p != ':') return NULL;

    p++;
    return _k230SkipSpaces(p);
}

static bool _k230IsKnownPostureType(const char* postureType) {
    if (strcmp(postureType, "normal") == 0) return true;
    if (strcmp(postureType, "bad") == 0) return true;
    if (strcmp(postureType, "no_person") == 0) return true;
    if (strcmp(postureType, "head_down") == 0) return true;
    if (strcmp(postureType, "hunchback") == 0) return true;
    if (strcmp(postureType, "tilt") == 0) return true;
    if (strcmp(postureType, "unknown") == 0) return true;
    return false;
}

static bool _k230IsValueTerminator(char c) {
    return c == ',' || c == '}' || c == '\0' || c == '\r' || c == '\n' || c == ' ' || c == '\t';
}

static bool _k230ParseIntStrict(const char* valueStart, int* outValue) {
    if (valueStart == NULL || outValue == NULL) return false;
    char* endPtr = NULL;
    long parsed = strtol(valueStart, &endPtr, 10);
    if (endPtr == valueStart) return false;
    endPtr = (char*)_k230SkipSpaces(endPtr);
    if (endPtr == NULL || !_k230IsValueTerminator(*endPtr)) return false;
    *outValue = (int)parsed;
    return true;
}

static bool _k230ParseFloatStrict(const char* valueStart, float* outValue) {
    if (valueStart == NULL || outValue == NULL) return false;
    char* endPtr = NULL;
    double parsed = strtod(valueStart, &endPtr);
    if (endPtr == valueStart) return false;
    endPtr = (char*)_k230SkipSpaces(endPtr);
    if (endPtr == NULL || !_k230IsValueTerminator(*endPtr)) return false;
    *outValue = (float)parsed;
    return true;
}

static void _k230ApplyTextFrame(const char* postureType, bool isAbnormal, int consecutiveAbnormal, float confidence) {
    strncpy(_k230Data.postureType, postureType, sizeof(_k230Data.postureType) - 1);
    _k230Data.postureType[sizeof(_k230Data.postureType) - 1] = '\0';
    _k230Data.isAbnormal = isAbnormal;
    _k230Data.consecutiveAbnormal = consecutiveAbnormal;
    _k230Data.confidence = confidence;
    _k230Data.lastUpdateTime = millis();
    _k230Data.valid = true;
    _k230MarkFrameSynced();
    _k230FrameCount++;
}

static bool _k230ParseTextLine(const char* line) {
    if (line == NULL || line[0] == '\0') {
        return false;
    }

    // 若行内已含 posture_type，说明更可能是 JSON 数据，交给 JSON 路径处理。
    if (strstr(line, "\"posture_type\"") != NULL) {
        return false;
    }

    if (strstr(line, "no_person") != NULL) {
        _k230ApplyTextFrame("no_person", false, 0, 0.0f);
        return true;
    }
    if (strstr(line, "head_down") != NULL) {
        _k230ApplyTextFrame("head_down", true, 1, 0.60f);
        return true;
    }
    if (strstr(line, "hunchback") != NULL) {
        _k230ApplyTextFrame("hunchback", true, 1, 0.60f);
        return true;
    }
    if (strstr(line, "tilt") != NULL) {
        _k230ApplyTextFrame("tilt", true, 1, 0.60f);
        return true;
    }
    if (strstr(line, "bad") != NULL) {
        _k230ApplyTextFrame("bad", true, 1, 0.60f);
        return true;
    }
    if (strstr(line, "normal") != NULL) {
        _k230ApplyTextFrame("normal", false, 0, 0.60f);
        return true;
    }

    return false;
}

static void _k230ResetJsonCapture() {
    _k230JsonCapturing = false;
    _k230JsonDepth = 0;
    _k230JsonInString = false;
    _k230JsonEscape = false;
    _k230JsonPos = 0;
}

static void _k230FeedJsonStream(char c) {
    if (!_k230JsonCapturing) {
        if (c == '{') {
            // 仅在遇到对象起点时进入捕获，降低串口噪声对解析状态机的干扰。
            _k230JsonCapturing = true;
            _k230JsonDepth = 1;
            _k230JsonInString = false;
            _k230JsonEscape = false;
            _k230JsonPos = 0;
            _k230JsonBuf[_k230JsonPos++] = c;
        }
        return;
    }

    // 持续写入 JSON 缓冲；若超长则尝试“最后一次抢救解析”后丢弃本对象。
    if (_k230JsonPos < (int)sizeof(_k230JsonBuf) - 1) {
        _k230JsonBuf[_k230JsonPos++] = c;
    } else {
        _k230JsonBuf[_k230JsonPos] = '\0';
        if (strstr(_k230JsonBuf, "\"posture_type\"") != NULL && _k230Parse(_k230JsonBuf)) {
            _k230PreferJsonStream = true;
            _k230JsonLineCount++;
            _k230ParsedSinceLastDelimiter = true;
            if (_k230HasSyncedFrame) {
                LOGW("[K230] JSON流超长前已恢复1帧解析");
            }
        }
        if (_k230HasSyncedFrame) {
            _k230TruncatedLineCount++;
            LOGW("[K230] JSON流缓冲区溢出，丢弃当前对象");
        }
        _k230ResetJsonCapture();
        return;
    }

    if (_k230JsonEscape) {
        _k230JsonEscape = false;
        return;
    }

    if (_k230JsonInString) {
        if (c == '\\') {
            _k230JsonEscape = true;
        } else if (c == '"') {
            _k230JsonInString = false;
        }
        return;
    }

    if (c >= 0 && c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
        _k230RejectControlChar++;
        _k230ResetJsonCapture();
        return;
    }

    if (c == '"') {
        _k230JsonInString = true;
        return;
    }

    if (c == '{') {
        // depth 跟踪支持对象内嵌套，避免中途 '}' 误判为帧结束。
        _k230JsonDepth++;
        return;
    }

    if (c == '}') {
        _k230JsonDepth--;
        if (_k230JsonDepth <= 0) {
            _k230JsonBuf[_k230JsonPos] = '\0';
            if (strstr(_k230JsonBuf, "\"posture_type\"") != NULL && _k230Parse(_k230JsonBuf)) {
                _k230PreferJsonStream = true;
                _k230JsonLineCount++;
                _k230ParsedSinceLastDelimiter = true;
                LOGD("[K230] 帧 #%lu: %s (异常=%s, 连续=%d, 置信=%.2f)",
                     _k230FrameCount,
                     _k230Data.postureType,
                     _k230Data.isAbnormal ? "是" : "否",
                     _k230Data.consecutiveAbnormal,
                     _k230Data.confidence);
            }
            _k230ResetJsonCapture();
        }
    }
}

static void _k230RecoverUart(const char* reason) {
    LOGW("[K230] UART接收静默，重建Serial1: %s", reason);
    Serial1.end();
    delay(5);
    Serial1.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
    _k230BufPos = 0;
    _k230LineTruncated = false;
    _k230ParsedSinceLastDelimiter = false;
    _k230ResetJsonCapture();
    _k230LastRecoverTime = millis();
    _k230RecoverCount++;
}

/**
 * @brief 解析 K230 JSON 数据
 * 
 * 从 JSON 字符串中提取各字段值。
 * 
 * @param json 以 \0 结尾的 JSON 字符串
 * @return true 解析成功
 */
static bool _k230Parse(const char* json) {
    _k230ParseAttemptCount++;

    // 解析策略说明：
    // 1) 先通过 key 锚点定位字段起点（strstr）。
    // 2) 对数值字段使用 strtol/strtod 并验证终止符，避免把 "12abc" 当作合法值。
    // 3) 全部字段通过后再一次性写入 _k230Data，避免半解析状态污染。

    char postureType[sizeof(_k230Data.postureType)];
    bool isAbnormal = false;
    int consecutiveAbnormal = 0;
    float confidence = 0.0f;

    // posture_type 是帧有效性的主锚点，缺失则整帧丢弃。
    const char* ptValue = _k230FindKeyValueStart(json, "\"posture_type\"");
    if (ptValue == NULL || *ptValue != '"') {
        _k230RejectMissingPostureType++;
        return false;
    }

    const char* ptStart = ptValue + 1;
    const char* ptEnd = strchr(ptStart, '"');
    if (ptEnd == NULL || ptEnd <= ptStart || (ptEnd - ptStart) >= (int)sizeof(_k230Data.postureType)) {
        _k230RejectMissingPostureType++;
        return false;
    }

    strncpy(postureType, ptStart, ptEnd - ptStart);
    postureType[ptEnd - ptStart] = '\0';
    if (!_k230IsKnownPostureType(postureType)) {
        _k230RejectUnknownPostureType++;
        return false;
    }

    // --- 解析 is_abnormal ---
    // 仅接受 true/false 字面量，拒绝 0/1 或其他变体，保持协议一致性。
    const char* abValue = _k230FindKeyValueStart(json, "\"is_abnormal\"");
    if (abValue == NULL) {
        _k230RejectIsAbnormal++;
        return false;
    }
    if (strncmp(abValue, "true", 4) == 0) {
        isAbnormal = true;
    } else if (strncmp(abValue, "false", 5) == 0) {
        isAbnormal = false;
    } else {
        _k230RejectIsAbnormal++;
        return false;
    }

    // --- 解析 consecutive_abnormal ---
    // 连续异常帧数必须是严格整型，且后续字符必须为 JSON 分隔符。
    const char* caValue = _k230FindKeyValueStart(json, "\"consecutive_abnormal\"");
    if (!_k230ParseIntStrict(caValue, &consecutiveAbnormal)) {
        _k230RejectConsecutive++;
        return false;
    }

    // --- 解析 confidence ---
    // 置信度按浮点读取，并额外校验范围 [0,1]。
    const char* cfValue = _k230FindKeyValueStart(json, "\"confidence\"");
    if (!_k230ParseFloatStrict(cfValue, &confidence)) {
        _k230RejectConfidence++;
        return false;
    }

    if (confidence < 0.0f || confidence > 1.0f) {
        _k230RejectConfidenceRange++;
        return false;
    }

    if (strcmp(postureType, "no_person") == 0) {
        // 协议约定：无人时不视为异常，连续异常数清零。
        isAbnormal = false;
        consecutiveAbnormal = 0;
    }

    strncpy(_k230Data.postureType, postureType, sizeof(_k230Data.postureType) - 1);
    _k230Data.postureType[sizeof(_k230Data.postureType) - 1] = '\0';
    _k230Data.isAbnormal = isAbnormal;
    _k230Data.consecutiveAbnormal = consecutiveAbnormal;
    _k230Data.confidence = confidence;
    
    // 更新时间戳和有效标志
    _k230Data.lastUpdateTime = millis();
    _k230Data.valid = true;
    _k230MarkFrameSynced();
    _k230FrameCount++;
    _k230ParseSuccessCount++;
    
    return true;
}

static void _k230OnRxByte(char c) {
    if (_k230RecentByteCount < (int)sizeof(_k230RecentBytes)) {
        _k230RecentBytes[_k230RecentByteCount++] = (uint8_t)c;
    } else {
        memmove(_k230RecentBytes, _k230RecentBytes + 1, sizeof(_k230RecentBytes) - 1);
        _k230RecentBytes[sizeof(_k230RecentBytes) - 1] = (uint8_t)c;
    }

    _k230RxByteCount++;
    _k230LastByteTime = millis();
    if (!_k230SeenAnyByte) {
        _k230SeenAnyByte = true;
        LOGI("[K230] 检测到串口字节流，链路已激活");
    }
    _k230FeedJsonStream(c);
}

static void _k230HandleDelimiter() {
    if (_k230JsonCapturing && _k230JsonPos > 20) {
        _k230JsonBuf[_k230JsonPos] = '\0';
        if (strstr(_k230JsonBuf, "\"posture_type\"") != NULL && _k230Parse(_k230JsonBuf)) {
            _k230PreferJsonStream = true;
            _k230JsonLineCount++;
            _k230ParsedSinceLastDelimiter = true;
            LOGW("[K230] 分隔符处恢复了未闭合JSON帧");
        }
        _k230ResetJsonCapture();
    }

    if (_k230BufPos <= 0) {
        return;
    }

    _k230LineBuf[_k230BufPos] = '\0';
    _k230LineCount++;

    if (_k230LineTruncated) {
        _k230TruncatedLineCount++;
        LOGW("[K230] 行长度超过缓冲区，已截断解析(长度上限=%d)",
             (int)sizeof(_k230LineBuf) - 1);
    }

    // 行兜底策略：即使前面是噪声，只要本行包含 '{'，仍尝试从 JSON 起点恢复解析。
    const char* jsonStart = strchr(_k230LineBuf, '{');
    if (jsonStart != NULL) {
        // 若 JSON 流模式本轮未成功解析，仍尝试按"行分隔"兜底恢复一帧。
        if (!_k230ParsedSinceLastDelimiter && _k230Parse(jsonStart)) {
            _k230JsonLineCount++;
            if (_k230PreferJsonStream) {
                LOGW("[K230] JSON流模式下通过分隔符恢复了1帧");
            }
        }
    } else {
        _k230NonJsonLineCount++;
        if (_k230ParseTextLine(_k230LineBuf)) {
            LOGD("[K230] 文本帧 #%lu: %s", _k230FrameCount, _k230Data.postureType);
        }
        if ((_k230NonJsonLineCount % 20) == 1) {
            LOGW("[K230] 非JSON行样本 #%lu: %.120s", _k230NonJsonLineCount, _k230LineBuf);
        }
    }

    _k230BufPos = 0;
    _k230LineTruncated = false;
    _k230ParsedSinceLastDelimiter = false;
}

static void _k230AppendLineByte(char c) {
    if (_k230BufPos < (int)sizeof(_k230LineBuf) - 1) {
        _k230LineBuf[_k230BufPos++] = c;
        return;
    }

    if (!_k230LineTruncated) {
        LOGW("[K230] 行缓冲区溢出，等待换行后进行截断解析");
        _k230LineTruncated = true;
    }
    // 溢出后清空当前行，等待下一个分隔符重新同步，避免持续写越界。
    _k230BufPos = 0;
}

static void _k230LogDiagnostics(unsigned long now) {
    if (now - _k230LastDiagLogTime < 3000) {
        return;
    }

    _k230LastDiagLogTime = now;
    if (!_k230SeenAnyByte) {
        // 启动后长期无字节，优先提示硬件链路问题（供电/接线/串口配置）。
        LOGW("[K230] 3秒内未收到任何串口字节，请检查K230是否启用UART输出、TX/RX接线和GND共地");
        return;
    }

    unsigned long silenceMs = now - _k230LastByteTime;
    LOGI("[K230] 链路状态: rxBytes=%lu, frames=%lu, silence=%lums, valid=%s",
         _k230RxByteCount,
         _k230FrameCount,
         silenceMs,
         _k230Data.valid ? "true" : "false");
    LOGI("[K230] 行统计: lines=%lu, json=%lu, nonJson=%lu, truncated=%lu",
         _k230LineCount,
         _k230JsonLineCount,
         _k230NonJsonLineCount,
         _k230TruncatedLineCount);
    LOGI("[K230] 解析统计: attempts=%lu, success=%lu, reject(pt=%lu, type=%lu, abnormal=%lu, consecutive=%lu, confidence=%lu, range=%lu, ctrl=%lu)",
         _k230ParseAttemptCount,
         _k230ParseSuccessCount,
         _k230RejectMissingPostureType,
         _k230RejectUnknownPostureType,
         _k230RejectIsAbnormal,
         _k230RejectConsecutive,
         _k230RejectConfidence,
         _k230RejectConfidenceRange,
         _k230RejectControlChar);

    if (_k230ParseAttemptCount == 0 && _k230RxByteCount >= 200 &&
        (now - _k230LastRawHintLogTime >= 9000)) {
        _k230LastRawHintLogTime = now;
        char hexBuf[3 * (int)sizeof(_k230RecentBytes) + 1];
        int w = 0;
        for (int i = 0; i < _k230RecentByteCount && w < (int)sizeof(hexBuf) - 4; i++) {
            w += sprintf(hexBuf + w, "%02X ", _k230RecentBytes[i]);
        }
        hexBuf[w] = '\0';
        LOGW("[K230] 长时间无解析尝试，最近字节HEX样本: %s", hexBuf);
    }

    if (silenceMs >= K230_UART_RECOVER_SILENCE_MS &&
        (now - _k230LastRecoverTime >= K230_UART_RECOVER_COOLDOWN_MS)) {
        // 恢复动作带冷却，避免在线路不稳时反复重建串口导致雪崩。
        _k230RecoverUart("silence threshold reached");
    }
}

// =============================================================================
// 公共函数实现
// =============================================================================

void k230_init() {
    LOGI("K230 UART 初始化...");
    LOGI("  TX(ESP->K230): GPIO%d", K230_UART_TX_PIN);
    LOGI("  RX(K230->ESP): GPIO%d", K230_UART_RX_PIN);
    LOGI("  波特率: %d", K230_UART_BAUD);
    
    // ESP32-S3 的 Serial1 可以映射到任意引脚
    Serial1.begin(K230_UART_BAUD, SERIAL_8N1, K230_UART_RX_PIN, K230_UART_TX_PIN);
    
    _k230BufPos = 0;
    _k230FrameCount = 0;
    _k230RxByteCount = 0;
    _k230Data.valid = false;
    _k230LastByteTime = 0;
    _k230LastDiagLogTime = millis();
    _k230SeenAnyByte = false;
    _k230HasSyncedFrame = false;
    _k230LastRecoverTime = 0;
    _k230RecoverCount = 0;
    _k230LineTruncated = false;
    _k230LineCount = 0;
    _k230JsonLineCount = 0;
    _k230NonJsonLineCount = 0;
    _k230TruncatedLineCount = 0;
    _k230ParseAttemptCount = 0;
    _k230ParseSuccessCount = 0;
    _k230RejectMissingPostureType = 0;
    _k230RejectUnknownPostureType = 0;
    _k230RejectIsAbnormal = 0;
    _k230RejectConsecutive = 0;
    _k230RejectConfidence = 0;
    _k230RejectConfidenceRange = 0;
    _k230RejectControlChar = 0;
    _k230LastRawHintLogTime = 0;
    _k230RecentByteCount = 0;
    _k230PreferJsonStream = false;
    _k230ParsedSinceLastDelimiter = false;
    _k230ResetJsonCapture();
    
    LOGI("K230 UART 初始化完成");
}

void k230_read() {
    while (Serial1.available()) {
        char c = Serial1.read();
        _k230OnRxByte(c);
        
        if (c == '\n' || c == '\r') {
            _k230HandleDelimiter();
        } else {
            _k230AppendLineByte(c);
        }
    }

    _k230LogDiagnostics(millis());
}

const K230Data* k230_getData() {
    return &_k230Data;
}

bool k230_isTimeout(unsigned long timeoutMs) {
    if (!_k230Data.valid) return true;
    return (millis() - _k230Data.lastUpdateTime) > timeoutMs;
}

unsigned long k230_getFrameCount() {
    return _k230FrameCount;
}

bool k230_hasSeenAnyByte() {
    return _k230SeenAnyByte;
}

unsigned long k230_getSilenceMs() {
    if (!_k230SeenAnyByte) {
        return 0;
    }
    return millis() - _k230LastByteTime;
}
