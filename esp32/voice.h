#ifndef VOICE_H
#define VOICE_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

static bool _voiceBusy = false;
static unsigned long _voiceBusyUntil = 0;
static bool _voiceFeedbackLogEnabled = false;

#if ENABLE_VOICE
static HardwareSerial& _voiceSerial = Serial2;

inline void _voiceSendFrame(const uint8_t* payload, uint16_t len, uint8_t frameParam) {
    uint8_t head[5];
    head[0] = 0xFD;
    head[1] = (uint8_t)((len + 2) >> 8);
    head[2] = (uint8_t)((len + 2) & 0xFF);
    head[3] = 0x01;
    head[4] = frameParam;

    _voiceSerial.write(head, sizeof(head));
    _voiceSerial.write(payload, len);
}

inline void _voiceSendRaw(const char* text, bool appendCrlf) {
    if (text == NULL) {
        return;
    }
    _voiceSerial.write((const uint8_t*)text, strlen(text));
    if (appendCrlf) {
        _voiceSerial.write('\r');
        _voiceSerial.write('\n');
    }
}

inline bool _voiceSendKnownPacketByText(const char* text) {
    if (text == NULL) {
        return false;
    }

    // 已验证可用的固定中文语音包（按当前SYN6288模组实测协议整理）。
    // 优先走固定包，避免不同编译环境下中文字符串编码不一致导致“有日志无声音”。
    static const uint8_t PACKET_WELCOME[] = {
        0xFD, 0x00, 0x13, 0x01, 0x00,
        0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
        0xBB, 0xB6, 0xD3, 0xAD, 0xCA, 0xB9, 0xD3, 0xC3, 0xF6
    };
    static const uint8_t PACKET_ADJUST_POSTURE[] = {
        0xFD, 0x00, 0x15, 0x01, 0x00,
        0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
        0xC7, 0xEB, 0xB5, 0xF7, 0xD5, 0xFB, 0xD7, 0xF8, 0xD7, 0xCB, 0x93
    };
    static const uint8_t PACKET_TIMER_DONE[] = {
        0xFD, 0x00, 0x15, 0x01, 0x00,
        0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
        0xB6, 0xA8, 0xCA, 0xB1, 0xC6, 0xF7, 0xBD, 0xE1, 0xCA, 0xF8, 0xDA
    };
    static const uint8_t PACKET_MONITOR_ON[] = {
        0xFD, 0x00, 0x19, 0x01, 0x00,
        0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
        0xD7, 0xF8, 0xD7, 0xCB, 0xBC, 0xEC, 0xB2, 0xE2, 0xD2, 0xD1, 0xBF, 0xAA, 0xC6, 0xF4, 0xFB
    };
    static const uint8_t PACKET_MONITOR_OFF[] = {
        0xFD, 0x00, 0x19, 0x01, 0x00,
        0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
        0xD7, 0xF8, 0xD7, 0xCB, 0xBC, 0xEC, 0xB2, 0xE2, 0xD2, 0xD1, 0xB9, 0xD8, 0xB1, 0xD5, 0xD9
    };

    const uint8_t* packet = NULL;
    size_t packetLen = 0;

    if (strcmp(text, "欢迎使用") == 0) {
        packet = PACKET_WELCOME;
        packetLen = sizeof(PACKET_WELCOME);
    } else if (strcmp(text, "请调整坐姿") == 0) {
        packet = PACKET_ADJUST_POSTURE;
        packetLen = sizeof(PACKET_ADJUST_POSTURE);
    } else if (strcmp(text, "定时器结束") == 0) {
        packet = PACKET_TIMER_DONE;
        packetLen = sizeof(PACKET_TIMER_DONE);
    } else if (strcmp(text, "坐姿检测已开启") == 0) {
        packet = PACKET_MONITOR_ON;
        packetLen = sizeof(PACKET_MONITOR_ON);
    } else if (strcmp(text, "坐姿检测已关闭") == 0) {
        packet = PACKET_MONITOR_OFF;
        packetLen = sizeof(PACKET_MONITOR_OFF);
    }

    if (packet == NULL) {
        return false;
    }

    _voiceSerial.write(packet, packetLen);
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 2500;
    LOGI("[语音][known] %s", text);
    return true;
}
#endif

inline void voice_init() {
#if ENABLE_VOICE
    _voiceSerial.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);
    _voiceBusy = false;
    _voiceBusyUntil = 0;
    _voiceFeedbackLogEnabled = false;
    LOGI("语音模块初始化完成: TX=GPIO%d, RX=GPIO%d, baud=%d", VOICE_TX_PIN, VOICE_RX_PIN, VOICE_BAUD);
#else
    LOGI("语音模块未启用（ENABLE_VOICE=0）");
#endif
}

inline void voice_speak(const char* text) {
#if ENABLE_VOICE
    if (text == NULL || text[0] == '\0') {
        return;
    }

    unsigned long now = millis();
    if (_voiceBusy && now < _voiceBusyUntil) {
        // 忙碌窗内直接丢弃，优先防止语音堆积导致延迟提醒。
        LOGW("[语音] 忙，跳过: %s", text);
        return;
    }

    // 对常用中文提示词优先使用固定包，其他文本再走通用字符串帧发送。
    if (_voiceSendKnownPacketByText(text)) {
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "[v8][t3]%s", text);

    size_t len = strlen(payload);
    if (len > 240) {
        len = 240;
    }

    _voiceSendFrame((const uint8_t*)payload, (uint16_t)len, VOICE_FRAME_PARAM);

    _voiceBusy = true;
    unsigned long estimate = (unsigned long)len * VOICE_MS_PER_CHAR;
    // 估时加上下限/上限，避免短文本误清忙碌或长文本长期锁死。
    if (estimate < 1200) {
        estimate = 1200;
    }
    if (estimate > 6000) {
        estimate = 6000;
    }
    _voiceBusyUntil = now + estimate;
    LOGI("[语音] 播报: %s", text);
#else
    LOGD("[语音 stub] 播报: %s", text);
#endif
}

inline void voice_speakWithParam(const char* text, uint8_t frameParam) {
#if ENABLE_VOICE
    if (text == NULL || text[0] == '\0') {
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "[v8][t3]%s", text);
    size_t len = strlen(payload);
    if (len > 240) {
        len = 240;
    }

    _voiceSendFrame((const uint8_t*)payload, (uint16_t)len, frameParam);
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 2000;
    LOGI("[语音][param=0x%02X] %s", frameParam, text);
#else
    (void)frameParam;
    LOGD("[语音 stub] 播报: %s", text);
#endif
}

inline void voice_speakGbk(const uint8_t* gbkBytes, size_t byteLen, uint8_t frameParam) {
#if ENABLE_VOICE
    if (gbkBytes == NULL || byteLen == 0) {
        return;
    }
    if (byteLen > 240) {
        byteLen = 240;
    }

    _voiceSendFrame(gbkBytes, (uint16_t)byteLen, frameParam);
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 2500;
    LOGI("[语音][gbk][param=0x%02X] %u bytes", frameParam, (unsigned int)byteLen);
#else
    (void)gbkBytes;
    (void)byteLen;
    (void)frameParam;
#endif
}

inline void voice_sendDemoPacket() {
#if ENABLE_VOICE
    static const uint8_t demo[] = {
        0xFD, 0x00, 0x35, 0x01, 0x00,
        0x5B, 0x6D, 0x34, 0x5D, 0x5B, 0x76, 0x31, 0x36, 0x5D, 0x5B, 0x74, 0x35, 0x5D,
        0xBB, 0xB6, 0xD3, 0xAD, 0xCA, 0xB9, 0xD3, 0xC3, 0xC2, 0xCC, 0xC9, 0xEE,
        0xC6, 0xEC, 0xBD, 0xA2, 0xB5, 0xEA, 0x53, 0x59, 0x4E, 0x36, 0x32, 0x38,
        0x38, 0xD3, 0xEF, 0xD2, 0xF4, 0xBA, 0xCF, 0xB3, 0xC9, 0xC4, 0xA3, 0xBF,
        0xE9, 0x91
    };
    _voiceSerial.write(demo, sizeof(demo));
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 2500;
    LOGI("[语音][demo] sent %u bytes", (unsigned int)sizeof(demo));
#endif
}

inline void voice_sendPacket(const uint8_t* packet, size_t len) {
#if ENABLE_VOICE
    if (packet == NULL || len == 0) {
        return;
    }
    _voiceSerial.write(packet, len);
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 2500;
    LOGI("[语音][packet] sent %u bytes", (unsigned int)len);
#else
    (void)packet;
    (void)len;
#endif
}

inline void voice_speakRaw(const char* text, bool appendCrlf) {
#if ENABLE_VOICE
    if (text == NULL || text[0] == '\0') {
        return;
    }
    _voiceSendRaw(text, appendCrlf);
    _voiceBusy = true;
    _voiceBusyUntil = millis() + 1500;
    LOGI("[语音][raw%s] %s", appendCrlf ? "+CRLF" : "", text);
#else
    (void)appendCrlf;
    LOGD("[语音 raw stub] 播报: %s", text);
#endif
}

inline void voice_setFeedbackLogEnabled(bool enabled) {
#if ENABLE_VOICE
    _voiceFeedbackLogEnabled = enabled;
#else
    (void)enabled;
#endif
}

inline void voice_update() {
#if ENABLE_VOICE
    while (_voiceSerial.available() > 0) {
        int b = _voiceSerial.read();
        if (b < 0) {
            break;
        }

        uint8_t code = (uint8_t)b;
        if (code == 0x4F) {
            _voiceBusy = false;
        }

        if (_voiceFeedbackLogEnabled) {
            LOGI("[语音回传] 0x%02X", code);
        }
    }

    if (_voiceBusy && millis() >= _voiceBusyUntil) {
        _voiceBusy = false;
    }
#endif
}

inline bool voice_isBusy() {
#if ENABLE_VOICE
    voice_update();
    if (_voiceBusy && millis() >= _voiceBusyUntil) {
        _voiceBusy = false;
    }
    return _voiceBusy;
#else
    return false;
#endif
}

inline void voice_stop() {
#if ENABLE_VOICE
    _voiceBusy = false;
    _voiceBusyUntil = 0;
#endif
}

#endif // VOICE_H
