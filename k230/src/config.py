"""
K230 视觉模块配置中心。

模块职责：
- 集中维护模型路径、推理阈值、摄像头参数、UART 参数与姿态判定阈值。
- 将“算法逻辑”和“部署调参”解耦，便于毕业设计答辩时解释参数影响。

与其他模块的关系：
- `main.py` 读取 `APP_CONFIG` 控制主循环、UART 与性能统计行为。
- `pose_detector.py` 读取 `MODEL_CONFIG`、`PERSON_FILTER`、`POSTURE_THRESHOLDS` 完成检测与姿态判定。
- `camera.py` 读取摄像头/预览相关配置初始化硬件。
"""

# 模型相关配置（影响“是否能检出人”与关键点稳定性）
MODEL_CONFIG = {
    # kmodel 文件路径（建议使用绝对路径）
    "model_path": "/sdcard/examples/kmodel/yolov8n-pose.kmodel",
    # 模型输入尺寸（需与模型训练输入一致）
    "input_width": 320,
    "input_height": 320,
    # 候选框置信度阈值（第一道过滤）
    # 含义：postprocess 阶段保留候选框的最低置信度。
    # 调大 -> 误检更少但可能漏检；调小 -> 更容易出框但噪声框更多。
    # 建议步长：0.02；建议范围：0.10 ~ 0.35。
    "conf_threshold": 0.20,
    # NMS 阈值（去重强度）
    # 含义：重叠框的抑制阈值。
    # 调大 -> 保留更多相近框；调小 -> 去重更激进。
    # 建议步长：0.05；建议范围：0.30 ~ 0.60。
    "nms_threshold": 0.45,
    # 关键点置信度阈值（用于统计有效关键点）
    # 含义：关键点 conf >= 此值才计入有效点数量。
    # 调大 -> 关键点更可靠但数量可能减少；调小 -> 数量增加但噪声点更多。
    # 建议步长：0.05；建议范围：0.10 ~ 0.40。
    "kpt_conf_threshold": 0.25,
}

# 系统运行配置
APP_CONFIG = {
    # 摄像头 ID（LCKFB 常见为 2）
    "camera_sensor_id": 2,
    # AI 推理通道（建议 chn2）
    "camera_channel": 2,
    # 采集分辨率：QQVGA/QVGA/VGA/HD/FHD
    "camera_framesize": "QVGA",
    # AI 通道像素格式（官方 AI 例程常用 RGBP888）
    "camera_pixformat": "RGBP888",
    # 预览通道（绑定显示层）
    "camera_preview_channel": 0,
    # 预览通道像素格式（绑定显示必须 YUV420SP）
    "camera_preview_pixformat": "YUV420SP",
    # 检测间隔 ms（越小刷新越快，CPU/NPU负载越高）
    "detection_interval": 120,
    # UART 发送间隔 ms（仅限制发往 ESP32 的频率，不影响识别本身持续运行）
    # 调大 -> 串口压力更小，但云端/ESP32 看到的状态刷新更慢。
    # 建议范围：300 ~ 500；建议先保持识别连续，仅调这个参数。
    "uart_send_interval_ms": 400,
    "camera_flush_frames": 1,
    # 是否启用 ESP32 UART 输出
    # 当前联调默认开启，使 K230 在识别运行时持续向 ESP32 输出最小姿态 JSON。
    "enable_esp32_uart": True,
    # 测试模式开关：用于在 CanMV IDE 中验证算法效果。
    # 开启后会强制打开 IDE 预览，并在 IDE 串口逐帧输出状态；是否发往 ESP32 仅由 enable_esp32_uart 控制。
    "test_mode": False,
    # UART 控制器编号（对应 machine.UART 的设备号）
    "uart_id": 1,
    # UART 波特率（需与 ESP32 端完全一致）
    "uart_baudrate": 9600,
    # UART TX 引脚号（K230 发往 ESP32 RX）
    "uart_tx_pin": 3,
    # UART RX 引脚号（K230 接收 ESP32 TX）
    "uart_rx_pin": 4,
    # 是否保存采集图像（调试用，开启会增加存储与IO开销）
    "save_images": False,
    # 全局调试开关（影响 logger 输出级别）
    "debug": True,
    # IDE 预览开关与参数
    "ide_preview": True,
    "ide_preview_fps": 15,
    # LT9611/ST7701/VIRT
    "ide_preview_mode": "LT9611",
    # 运行期详细日志开关（调阈值时建议关闭，仅保留JSON与错误）
    "runtime_verbose_logs": False,
    # 运行期性能指标日志开关（仅输出时延指标）
    "metrics_enabled": False,
    # 时延统计窗口大小（帧）
    "metrics_window_size": 50,
    # 时延指标输出间隔（帧）
    "metrics_log_interval_frames": 30,
    # 连续 no_person 达到该帧数后，重置异常判定状态机
    "no_person_reset_frames": 5,
    # 定期内存回收间隔（帧）
    "gc_collect_interval_frames": 80,
}

# 人体存在过滤参数（只影响是否输出 no_person，不影响姿态算法本体）
# 数据流位置：YOLO 输出 -> 人体存在过滤 -> 通过后才进入姿态类型判定。
PERSON_FILTER = {
    # 人体存在置信度阈值
    # 含义：最佳人体框 confidence 低于该值则判 no_person。
    # 调大 -> no_person 更容易触发（更保守）；调小 -> 更容易通过到姿态判断。
    # 建议步长：0.02；建议范围：0.10 ~ 0.35。
    "person_present_threshold": 0.15,
    # 最小人体框面积占比（归一化面积）
    # 含义：bbox_area 小于该值则判 no_person。
    # 调大 -> 近景优先、远景更易被过滤；调小 -> 远处/小人更容易被接受。
    # 建议步长：0.001；建议范围：0.001 ~ 0.03。
    "person_min_bbox_area_ratio": 0.02,
    # 最少有效关键点数量
    # 含义：有效关键点数量不足则判 no_person。
    # 调大 -> 对关键点完整性要求更高；调小 -> 更容易通过但鲁棒性下降。
    # 建议步长：1；建议范围：1 ~ 6。
    "person_min_keypoints": 2,
}

# 姿态判定参数（5关键点几何规则）
POSTURE_THRESHOLDS = {
    # torso_tilt_threshold_deg: 躯干线相对图像竖直方向的容忍角度（度）
    # 调大 -> 更不敏感；调小 -> 更容易判驼背。
    # 建议步长：1~2；建议范围：12 ~ 30。
    "torso_tilt_threshold_deg": 12,
    # head_tilt_threshold_deg: 鼻子到肩中点连线相对图像竖直方向的容忍角度（度）
    # 调大 -> 更不敏感；调小 -> 更容易判低头。
    # 建议步长：1~2；建议范围：15 ~ 35。
    "head_tilt_threshold_deg": 60,
    # required_keypoint_confidence: 5 个必需关键点的最低置信度要求
    # 只有鼻子、左右肩、左右髋都达到该阈值时，才进入姿态判定。
    "required_keypoint_confidence": 0.25,
    # stable_frame_count: 同一类别连续出现多少帧后才确认输出
    # 单张图片测试默认保持 1，实时视频可调整为 2。
    "stable_frame_count": 3,
}
