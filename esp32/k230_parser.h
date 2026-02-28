/**
 * @file k230_parser.h
 * @brief K230 视觉模块 UART 数据接收与解析
 * 
 * K230 通过 UART1 以 115200 波特率发送 \n 结尾的 JSON 数据，约每 200ms 一帧。
 * 
 * K230 发送的 JSON 格式：
 * {"frame_id":123,"posture_type":"normal","is_abnormal":false,
 *  "consecutive_abnormal":0,"confidence":0.95,"timestamp":123456}
 * 
 * posture_type 取值：
 * - "normal"    — 坐姿正常
 * - "head_down" — 低头
 * - "hunchback" — 驼背
 * - "tilt"      — 歪头/侧倾
 * - "no_person" — 无人
 * - "unknown"   — 未知
 * 
 * 解析方式：strstr，与 MQTT 模块一致，不引入 ArduinoJson。
 * 
 * @date 2026
 */

#ifndef K230_PARSER_H
#define K230_PARSER_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

// =============================================================================
// K230 数据结构
// =============================================================================

/**
 * @brief K230 坐姿数据
 */
typedef struct {
    char postureType[16];       // "normal", "head_down", "hunchback", "tilt", "no_person", "unknown"
    bool isAbnormal;            // 是否异常
    int consecutiveAbnormal;    // 连续异常帧数
    float confidence;           // 置信度 0.0~1.0
    unsigned long lastUpdateTime;  // 最后更新时间（millis）
    bool valid;                 // 是否有有效数据
} K230Data;

// =============================================================================
// 公共函数声明
// =============================================================================

/**
 * @brief 初始化 K230 UART 通信
 * 
 * 使用硬件 UART1（Serial1），引脚 GPIO17(TX) / GPIO18(RX)。
 */
void k230_init();

/**
 * @brief 非阻塞读取 K230 UART 数据
 * 
 * 在 loop() 中调用。按 \n 分帧，完整一行后解析 JSON。
 * 不会阻塞——如果没有数据立即返回。
 */
void k230_read();

/**
 * @brief 获取最新的 K230 数据
 * @return K230Data 结构体的指针
 */
const K230Data* k230_getData();

/**
 * @brief 检测 K230 是否超时（未收到数据）
 * @param timeoutMs 超时阈值（毫秒）
 * @return true K230 数据已超时
 */
bool k230_isTimeout(unsigned long timeoutMs);

/**
 * @brief 获取接收帧计数
 */
unsigned long k230_getFrameCount();

bool k230_hasSeenAnyByte();

unsigned long k230_getSilenceMs();

#endif // K230_PARSER_H