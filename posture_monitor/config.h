/**
 * @file config.h
 * @brief 智能语音坐姿提醒器 - 全局配置文件
 * 
 * 所有可配置参数集中在此文件管理：
 * - GPIO 引脚分配（基于 Freenove ESP32-S3 WROOM 引脚图）
 * - WiFi / MQTT / OneNET 连接参数
 * - 各模块定时间隔与阈值
 * - 外设启用开关（ENABLE 宏）
 * 
 * @note 不要将包含真实凭据的此文件提交到版本控制！
 * @date 2026
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <pgmspace.h>

// =============================================================================
// 外设启用开关（0=禁用/stub, 1=启用）
// -----------------------------------------------------------------------------
// 这一组宏决定“是否编译真实驱动逻辑”。
// 设计成开关的原因是：硬件未接入时仍要保证整机可编译、可联调（通过 stub 维持接口一致）。
// 调试建议：先最小化开启模块，确认主链路稳定后再逐个打开外设。
// =============================================================================

#define ENABLE_OLED         1   // OLED 显示屏
#define ENABLE_EC11         1   // EC11 旋转编码器
#define ENABLE_PIR          1   // HC-SR501 已完成独立 Demo 验证，接入整机
#define ENABLE_LIGHT_SENSOR 1   // GY-302 BH1750 已完成独立 Demo 验证，接入整机
#define ENABLE_VOICE        1   // SYN-6288 语音模块
#define ENABLE_BUZZER       1   // 蜂鸣器
#define ENABLE_WS2812       1   // 使用板载 WS2812（GPIO48）作为状态灯

// =============================================================================
// GPIO 引脚分配（Freenove ESP32-S3 WROOM Board）
// -----------------------------------------------------------------------------
// 本节控制所有外设硬件连线映射，是“软件逻辑”和“物理接线”之间的桥梁。
// 修改这里通常不需要改业务代码，但必须保证：
// 1) 不占用启动/日志/PSRAM/USB 等关键脚位；
// 2) 同一引脚不被多个外设冲突复用。
// 避免使用：GPIO35/36/37（PSRAM）、GPIO19/20（USB）、
//          GPIO0（Boot）、GPIO46（LOG）、GPIO43/44（UART0 调试串口）
// =============================================================================

// --- K230 视觉模块 UART（硬件 UART1）---
// 说明：以 K230 工程文档定义为准，ESP32 侧仅固定本端引脚。
#define K230_UART_TX_PIN    15  // ESP32 GPIO15 TX -> K230 UART1_RX GPIO4
#define K230_UART_RX_PIN    16  // ESP32 GPIO16 RX <- K230 UART1_TX GPIO3
#define K230_UART_BAUD      9600

// --- 板载指示灯 ---
#define STATUS_LED_PIN      -1  // 本板无可用单色板载LED，使用 WS2812 指示
#define WS2812_PIN          48  // 板载 WS2812 RGB LED
#define WS2812_NUM_LEDS     1   // WS2812 LED 数量

// --- OLED 显示屏 I2C ---
#define OLED_SDA_PIN        1   // I2C SDA（GPIO1）
#define OLED_SCL_PIN        2   // I2C SCL（GPIO2）
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_I2C_ADDR       0x3C

// --- EC11 旋转编码器 ---
#define EC11_S1_PIN         9   // 模块 S1（编码器 A 相）
#define EC11_S2_PIN         10  // 模块 S2（编码器 B 相）
#define EC11_KEY_PIN        21  // 模块 KEY（按键）

// --- 人体存在传感器（支持 PIR / 对射式红外） ---
#define PERSON_SENSOR_PIR         1
#define PERSON_SENSOR_BEAM_BREAK  2

#define PERSON_SENSOR_TYPE        PERSON_SENSOR_BEAM_BREAK
#define PERSON_SENSOR_PIN         4    // 当前对射式人体存在传感器输入引脚
#define PERSON_SENSOR_ACTIVE_LEVEL LOW // 挡住光路/有人时的有效电平
#define PERSON_SENSOR_WARMUP_MS   0UL  // 对射式无需预热；PIR 可改回非 0
#define PERSON_SENSOR_HOLD_MS     0UL  // 对射式按实时状态判定；PIR 可配置保持时间

// 旧宏名保留到主逻辑全面切换完成。
#define PIR_PIN             PERSON_SENSOR_PIN
#define PIR_WARMUP_MS       PERSON_SENSOR_WARMUP_MS

// --- GY-302 BH1750 光照传感器 ---
#define BH1750_SDA_PIN      OLED_SDA_PIN
#define BH1750_SCL_PIN      OLED_SCL_PIN
#define BH1750_I2C_ADDR     0x23
#define BH1750_MEASUREMENT_DELAY_MS 180UL
#define LIGHT_SENSOR_PIN    5   // 旧模拟光敏占位引脚，保留以兼容旧调试说明

// --- 补光 LED ---
#define ENABLE_FILL_LIGHT   1   // 0=禁用补光控制, 1=启用 GPIO 开关控制
#define FILL_LIGHT_PIN      13  // 白光 LED 控制引脚（需串联限流电阻）
#define FILL_LIGHT_LUX_THRESHOLD 20  // 低于该 lux 且人体在位时打开补光

// --- SYN-6288 语音合成模块 ---
#define VOICE_TX_PIN        41  // 语音模块 TX（ESP32 TX -> SYN6288 RX）
#define VOICE_RX_PIN        -1  // 默认不接回传，若要看回传改为实际 RX GPIO（如16）
#define VOICE_BAUD          9600
#define VOICE_MS_PER_CHAR   120
#define VOICE_FRAME_PARAM   0x00

// --- 蜂鸣器 ---
#define BUZZER_PIN          6   // PWM 输出（GPIO6）

// =============================================================================
// WiFi 配置
// -----------------------------------------------------------------------------
// 这一组参数控制网络接入稳定性：重试次数与重试间隔共同决定“首次上电等待多久”。
// 若现场网络较差，可适当增大重试上限；若希望快速失败回退，可减小重试窗口。
// =============================================================================

#define WIFI_SSID               "Mi"
#define WIFI_PASSWORD           ""
#define WIFI_MAX_RETRY_ATTEMPTS 20
#define WIFI_RETRY_DELAY_MS     500

// =============================================================================
// SNTP 时钟配置（ESP32 内置 SNTP，用于时钟模式）
// -----------------------------------------------------------------------------
// 本组参数决定设备从网络校时的来源与时区换算。
// 采用“双服务器”是为了提高可用性：主 NTP 失败时可自动回退到备用源。
// =============================================================================

#define SNTP_SERVER1        "ntp.aliyun.com"
#define SNTP_SERVER2        "pool.ntp.org"
#define SNTP_GMT_OFFSET     28800       // UTC+8 北京时间（秒）
#define SNTP_DAYLIGHT       0           // 无夏令时

// =============================================================================
// OneNET MQTT 配置
// -----------------------------------------------------------------------------
// 本组控制云平台连接端点与认证参数。
// 其中 Product/Device/Token 三项必须与云端设备完全一致，否则会出现“能连 WiFi 但 MQTT 鉴权失败”。
// =============================================================================

#define MQTT_HOST           "mqtts.heclouds.com"
#define MQTT_PORT           1883

// MQTT 连接参数（不显式调用 setKeepAlive/setBufferSize/setSocketTimeout）
#define MQTT_KEEPALIVE_SEC      120     // 仅供参考，使用库默认值
#define MQTT_CONNECT_TIMEOUT_MS 10000
#define MQTT_RECONNECT_DELAY_MS 5000

// OneNET 设备认证信息
#define ONENET_PRODUCT_ID   "DX0LtPPJQ9"
#define ONENET_DEVICE_NAME  "main"
#define ONENET_DEVICE_TOKEN "version=2018-10-31&res=products%2FDX0LtPPJQ9%2Fdevices%2Fmain&et=1780484779&method=md5&sign=9US70lu1tmN87SJkRNs9RQ%3D%3D"

// =============================================================================
// MQTT 主题定义
// -----------------------------------------------------------------------------
// 主题是云端协议路由键：
// - post/post_reply 对应设备上报与平台回执
// - set/set_reply、get/get_reply 对应平台下发与设备应答
// 建议保持常量集中定义，避免在业务代码里硬编码主题字符串。
// =============================================================================

// --- 发布主题 ---
#define TOPIC_PROPERTY_POST         "$sys/DX0LtPPJQ9/main/thing/property/post"
#define TOPIC_PROPERTY_SET_REPLY    "$sys/DX0LtPPJQ9/main/thing/property/set_reply"
#define TOPIC_PROPERTY_GET_REPLY    "$sys/DX0LtPPJQ9/main/thing/property/get_reply"
#define TOPIC_EVENT_POST            "$sys/DX0LtPPJQ9/main/thing/event/post"

// --- 订阅主题 ---
#define TOPIC_PROPERTY_POST_REPLY   "$sys/DX0LtPPJQ9/main/thing/property/post/reply"
#define TOPIC_PROPERTY_SET          "$sys/DX0LtPPJQ9/main/thing/property/set"
#define TOPIC_PROPERTY_GET          "$sys/DX0LtPPJQ9/main/thing/property/get"
#define TOPIC_EVENT_POST_REPLY      "$sys/DX0LtPPJQ9/main/thing/event/post/reply"

// =============================================================================
// 定时间隔配置（毫秒）
// -----------------------------------------------------------------------------
// 本节控制主循环中各任务的触发频率，直接影响功耗、实时性与网络负载。
// 调参原则：
// - 太短：CPU/串口/MQTT 压力上升；
// - 太长：状态更新滞后。
// 因此需要在“响应速度”和“系统稳定”之间折中。
// =============================================================================

#define PUBLISH_INTERVAL_MS         10000   // MQTT 属性上报间隔（10 秒）
#define K230_TIMEOUT_MS             5000    // K230 数据超时阈值（5 秒无数据视为离线）
#define K230_STARTUP_GRACE_MS       15000   // 启动宽限期（15 秒内允许等待首帧）
#define K230_STALE_HOLD_MS          15000   // 超时后短时间内沿用最后有效状态
#define K230_UART_RECOVER_SILENCE_MS 12000  // 串口静默达到该阈值时尝试重建Serial1
#define K230_UART_RECOVER_COOLDOWN_MS 8000  // 串口重建最小间隔，避免频繁抖动
#define DISPLAY_UPDATE_INTERVAL_MS  500     // OLED 刷新间隔
#define SENSOR_READ_INTERVAL_MS     200     // 传感器读取间隔，补光需更紧跟环境变化
#define ALERT_CHECK_INTERVAL_MS     200     // 报警检查间隔

// =============================================================================
// 坐姿检测参数
// -----------------------------------------------------------------------------
// 本节定义坐姿提醒策略与定时器边界：
// - 异常判定阈值（连续帧）
// - 提醒冷却时间
// - 定时器默认/最小/最大时长
// - EC11 按键与蜂鸣器行为阈值
// 这些参数共同决定“提醒是否灵敏”与“是否扰民”。
// =============================================================================

// 连续异常帧数阈值（K230 约 200ms 一帧，15 帧 ≈ 3 秒）
#define ABNORMAL_THRESHOLD_FRAMES   15

// 坐姿提醒冷却时间（毫秒），避免重复提醒
#define ALERT_COOLDOWN_MS           5000    // 5 秒

#define TIMER_DEFAULT_DURATION_SEC  1500    // 25 分钟

#define EC11_LONG_PRESS_MS          700

#define EC11_DEBOUNCE_MS            20

#define BUZZER_PULSE_MS             120
#define TIMER_DONE_REMINDER_INTERVAL_MS 3000
#define TIMER_DONE_LED_PULSE_MS     700

// 蜂鸣器旋律参数（模式切换提示音）
#define BUZZER_MELODY_NOTE_MS       180     // 单个音符脉冲时长
#define BUZZER_MELODY_GAP_MS        220     // 音符之间间隔

// 报警模式掩码（bit0=LED, bit1=蜂鸣器, bit2=语音）
#define ALERT_MODE_MASK_ALL         0x07    // 全部启用

// 冷却时间与定时器边界值
#define COOLDOWN_MIN_MS             1000    // 冷却最小值（1 秒）
#define COOLDOWN_MAX_MS             600000  // 冷却最大值（10 分钟）
#define TIMER_MIN_DURATION_SEC      1       // 定时器下限（1 秒）
#define TIMER_MAX_DURATION_SEC      7200    // 定时器上限（2 小时）

// 主循环延迟
#define LOOP_YIELD_DELAY_MS         10      // loop() 末尾让出 CPU

// MQTT 属性标识（与 shared/protocol/constants.h 保持同步）
// 这些 key 是设备与云端的数据契约，重命名会直接导致属性读写失配。
#define POSTURE_CODE_NO_PERSON      0
#define POSTURE_CODE_NORMAL         1
#define POSTURE_CODE_HEAD_DOWN      2
#define POSTURE_CODE_HUNCHBACK      3

#define PROP_ID_POSTURE_TYPE        "postureType"
#define PROP_ID_MONITORING_ENABLED  "monitoringEnabled"
#define PROP_ID_CURRENT_MODE        "currentMode"
#define PROP_ID_PERSON_PRESENT      "personPresent"
#define PROP_ID_AMBIENT_LUX         "ambientLux"
#define PROP_ID_FILL_LIGHT_ON       "fillLightOn"
#define PROP_ID_ALERT_MODE_MASK     "alertModeMask"
#define PROP_ID_COOLDOWN_MS         "cooldownMs"
#define PROP_ID_TIMER_DURATION_SEC  "timerDurationSec"
#define PROP_ID_TIMER_RUNNING       "timerRunning"
#define PROP_ID_CFG_VERSION         "cfgVersion"
#define PROP_ID_SELF_TEST           "selfTest"

// 旧云端字段，迁移期间保留到主逻辑切换完成。
#define PROP_ID_IS_POSTURE          "isPosture"

// =============================================================================
// 模式定义
// -----------------------------------------------------------------------------
// 运行模式由 EC11 旋钮切换，不同模式复用同一套硬件输入但含义不同。
// 把模式集中定义可保证 UI、输入处理、云端上报三方语义一致。
// =============================================================================

/**
 * @brief 系统运行模式（EC11 旋转切换）
 */
enum SystemMode {
    MODE_POSTURE = 0,   // 坐姿检测模式（默认）
    MODE_CLOCK   = 1,   // 时钟模式
    MODE_TIMER   = 2,   // 定时器模式
    MODE_COUNT   = 3    // 模式总数（用于循环切换）
};

// =============================================================================
// 调试与开发
// -----------------------------------------------------------------------------
// 控制串口日志与调试输出等级。
// 项目答辩或联调阶段建议开启日志；量产/长期运行可关闭以减少串口噪声。
// =============================================================================

#define DEBUG_ENABLED       false
#define SERIAL_BAUD_RATE    115200

#endif // CONFIG_H
