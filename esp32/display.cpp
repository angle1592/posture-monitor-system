#include "display.h"

/**
 * @brief display.cpp 实现说明
 *
 * 实现要点：
 * - 通过缓存状态变量（连接状态、定时器状态、短消息窗口）与渲染函数解耦。
 * - display_update() 按 mode 分页面渲染：坐姿页、时钟页、定时器页。
 * - 短消息窗口优先级高于主页面，保障提示信息可完整显示。
 *
 * 内部状态变量含义：
 * - _displayReady：OLED 是否已初始化。
 * - _displayWifiConnected/_displayMqttConnected：连接状态缓存。
 * - _displayTimerRemainSec/_displayTimerTotalSec/_displayTimerState：定时器显示缓存。
 * - _displayMsg1/_displayMsg2/_displayMsgUntil：短消息文本与过期时间。
 */

// Static variables
static bool _displayReady = false;
static bool _displayWifiConnected = false;
static bool _displayMqttConnected = false;
static int _displayTimerRemainSec = TIMER_DEFAULT_DURATION_SEC;
static int _displayTimerTotalSec = TIMER_DEFAULT_DURATION_SEC;
static int _displayTimerState = DISPLAY_TIMER_IDLE;
static char _displayMsg1[22] = {0};
static char _displayMsg2[22] = {0};
static unsigned long _displayMsgUntil = 0;

#if ENABLE_OLED && DISPLAY_U8G2_AVAILABLE
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2(U8G2_R0, U8X8_PIN_NONE);

inline void _displayFmtMMSS(int sec, char* out, size_t outLen) {
    // 显示层统一时间格式化入口，避免各页面重复写 mm:ss 转换。
    if (sec < 0) sec = 0;
    int mm = sec / 60;
    int ss = sec % 60;
    snprintf(out, outLen, "%02d:%02d", mm, ss);
}
#endif

void display_setConnectivity(bool wifiConnected, bool mqttConnected) {
    _displayWifiConnected = wifiConnected;
    _displayMqttConnected = mqttConnected;
}

void display_setTimerStatus(int remainSec, int totalSec, int timerState) {
    _displayTimerRemainSec = remainSec;
    _displayTimerTotalSec = totalSec;
    _displayTimerState = timerState;
}

void display_init() {
#if ENABLE_OLED
    // 启用时初始化 I2C 与 U8g2；禁用时仅输出日志并保留空实现。
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    _u8g2.begin();
    _u8g2.setContrast(180);
    _displayReady = true;
    LOGI("OLED 初始化完成");
#else
    LOGI("OLED 未启用（ENABLE_OLED=0）");
#endif
}

void display_update(int mode, const char* postureType, bool isAbnormal) {
#if ENABLE_OLED && DISPLAY_U8G2_AVAILABLE
    if (!_displayReady) {
        return;
    }

    _u8g2.clearBuffer();

    if (_displayMsgUntil > millis()) {
        // 短消息窗口期间冻结主界面，确保提示信息至少完整显示一轮。
        _u8g2.setFont(u8g2_font_6x10_tf);
        _u8g2.drawStr(0, 20, _displayMsg1);
        _u8g2.drawStr(0, 40, _displayMsg2);
        _u8g2.sendBuffer();
        return;
    }

    char line[32];
    _u8g2.setFont(u8g2_font_6x10_tf);
    snprintf(line, sizeof(line), "WiFi:%s MQTT:%s", _displayWifiConnected ? "OK" : "--", _displayMqttConnected ? "OK" : "--");
    _u8g2.drawStr(0, 10, line);

    if (mode == MODE_POSTURE) {
        // 姿态页：展示姿态类型 + 是否异常，供用户快速感知当前识别结果。
        _u8g2.setFont(u8g2_font_6x13_tf);
        if (postureType == NULL || postureType[0] == '\0') {
            _u8g2.drawStr(0, 34, "Posture: unknown");
        } else {
            snprintf(line, sizeof(line), "Posture:%s", postureType);
            _u8g2.drawStr(0, 34, line);
        }
        _u8g2.drawStr(0, 54, isAbnormal ? "State: abnormal" : "State: normal");
    } else if (mode == MODE_CLOCK) {
        // 时钟页：优先显示本地时间；SNTP 未同步完成时显示提示文案。
        struct tm t;
        if (getLocalTime(&t, 10)) {
            char hhmmss[16];
            char ymd[24];
            snprintf(hhmmss, sizeof(hhmmss), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
            snprintf(ymd, sizeof(ymd), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            _u8g2.setFont(u8g2_font_logisoso24_tn);
            _u8g2.drawStr(0, 40, hhmmss);
            _u8g2.setFont(u8g2_font_6x10_tf);
            _u8g2.drawStr(0, 60, ymd);
        } else {
            _u8g2.drawStr(0, 34, "Syncing time...");
        }
    } else {
        // 非 posture/clock 统一落到 timer 渲染，和主状态机 mode 枚举保持同口径。
        char mmss[12];
        _displayFmtMMSS(_displayTimerRemainSec, mmss, sizeof(mmss));
        _u8g2.setFont(u8g2_font_logisoso24_tn);
        _u8g2.drawStr(0, 40, mmss);
        _u8g2.setFont(u8g2_font_6x10_tf);
        if (_displayTimerState == DISPLAY_TIMER_RUNNING) {
            _u8g2.drawStr(0, 60, "Timer: running");
        } else if (_displayTimerState == DISPLAY_TIMER_PAUSED) {
            _u8g2.drawStr(0, 60, "Timer: paused");
        } else if (_displayTimerState == DISPLAY_TIMER_DONE) {
            _u8g2.drawStr(0, 60, "Timer: done");
        } else {
            _u8g2.drawStr(0, 60, "Timer: idle");
        }
    }

    _u8g2.sendBuffer();
#else
    // OLED 关闭时显式吞掉参数，避免未使用告警并保持调用侧接口一致。
    (void)mode;
    (void)postureType;
    (void)isAbnormal;
#endif
}

void display_showSplash() {
#if ENABLE_OLED && DISPLAY_U8G2_AVAILABLE
    if (!_displayReady) {
        return;
    }

    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.drawStr(0, 24, "Smart Posture");
    _u8g2.drawStr(0, 42, "Reminder v1.0");
    _u8g2.sendBuffer();
#endif
}

void display_showMessage(const char* line1, const char* line2) {
#if ENABLE_OLED
    if (line1 == NULL) {
        _displayMsg1[0] = '\0';
    } else {
        strncpy(_displayMsg1, line1, sizeof(_displayMsg1) - 1);
        _displayMsg1[sizeof(_displayMsg1) - 1] = '\0';
    }

    if (line2 == NULL) {
        _displayMsg2[0] = '\0';
    } else {
        strncpy(_displayMsg2, line2, sizeof(_displayMsg2) - 1);
        _displayMsg2[sizeof(_displayMsg2) - 1] = '\0';
    }

    // 统一 2 秒提示窗；窗口期内 display_update() 会优先渲染该消息。
    _displayMsgUntil = millis() + 2000;
#else
    (void)line1;
    (void)line2;
#endif
}
