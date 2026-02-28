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

/**
 * @brief K230 串口解析模块（视觉结果接入层）
 *
 * 模块职责：
 * - 从 UART 非阻塞接收 K230 输出的逐帧文本/JSON 数据。
 * - 使用轻量字符串策略解析 posture_type、is_abnormal 等关键字段。
 * - 维护最近一帧有效姿态数据与通信健康状态（超时、静默等）。
 *
 * 对外提供的核心 API：
 * - k230_init() / k230_read()
 * - k230_getData() / k230_isTimeout()
 * - k230_getFrameCount() / k230_hasSeenAnyByte() / k230_getSilenceMs()
 *
 * 依赖关系：
 * - Arduino Serial1 / millis
 * - config.h（串口引脚、恢复阈值等参数）
 * - utils.h（日志输出）
 *
 * 是否可选：
 * - 非可选核心模块；若 K230 链路不可用，主姿态检测业务会降级。
 */

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
 * 同时会重置解析统计和链路状态，进入冷启动监听阶段。
 */
void k230_init();

/**
 * @brief 非阻塞读取 K230 UART 数据
 *
 * 在 loop() 中调用。按 \n 分帧，完整一行后解析 JSON。
 * 不会阻塞——如果没有数据立即返回。
 * 内部同时维护 JSON 流式捕获，增强抗噪声能力。
 */
void k230_read();

/**
 * @brief 获取最新的 K230 数据
 *
 * 返回内部静态结构体地址，调用方应按“只读快照”使用。
 * @return K230Data 结构体的指针
 */
const K230Data* k230_getData();

/**
 * @brief 检测 K230 是否超时（未收到数据）
 * @param timeoutMs 超时阈值（毫秒）
 *
 * 常用于主循环判断是否进入通信异常分支。
 * @return true K230 数据已超时
 */
bool k230_isTimeout(unsigned long timeoutMs);

/**
 * @brief 获取接收帧计数
 *
 * 仅统计解析成功并写入 _k230Data 的有效帧。
 */
unsigned long k230_getFrameCount();

bool k230_hasSeenAnyByte();

unsigned long k230_getSilenceMs();

#endif // K230_PARSER_H
