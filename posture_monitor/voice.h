#ifndef VOICE_H
#define VOICE_H

/**
 * @brief 语音播报模块（SYN-6288，可选）
 *
 * 模块职责：
 * - 封装 SYN-6288 串口协议帧发送。
 * - 提供文本播报、GBK 原始字节播报、原始包发送与忙碌状态管理。
 * - 支持常用短语的固定包直发，降低编码差异导致的播报失败风险。
 *
 * 对外提供的核心 API：
 * - voice_init() / voice_update() / voice_isBusy() / voice_stop()
 * - voice_speak() / voice_speakWithParam() / voice_speakGbk()
 * - voice_sendPacket() / voice_sendDemoPacket() / voice_speakRaw()
 * - voice_setFeedbackLogEnabled()
 *
 * 依赖关系：
 * - ENABLE_VOICE=1 时依赖 Serial2 与 config.h 中语音串口参数。
 * - utils.h 用于运行日志与诊断信息输出。
 *
 * 是否可选：
 * - 受 ENABLE_VOICE 控制。
 * - ENABLE_VOICE=0 时接口保留为 stub，不驱动语音硬件。
 */

#include <Arduino.h>
#include "config.h"
#include "utils.h"

// 初始化语音串口并重置忙碌状态。
void voice_init();
// 发送普通文本播报（内部会构建 SYN-6288 帧）。
void voice_speak(const char* text);
// 以指定 frameParam 发送文本播报。
void voice_speakWithParam(const char* text, uint8_t frameParam);
// 直接发送 GBK 字节内容，适用于已完成外部编码的文本。
void voice_speakGbk(const uint8_t* gbkBytes, size_t byteLen, uint8_t frameParam);
// 发送内置演示语音包。
void voice_sendDemoPacket();
// 直接下发完整协议包。
void voice_sendPacket(const uint8_t* packet, size_t len);
// 以原始串口文本方式发送（可选追加 CRLF）。
void voice_speakRaw(const char* text, bool appendCrlf);
// 开关语音模块回传字节日志。
void voice_setFeedbackLogEnabled(bool enabled);
// 轮询语音模块回传并维护忙碌状态。
void voice_update();
// 查询当前是否处于播报忙碌态。
bool voice_isBusy();
// 立即清除忙碌状态（逻辑停止）。
void voice_stop();

#endif // VOICE_H
