"""
Project configuration for K230Vision.

This module centralizes runtime parameters to keep main logic clean and
make deployment tuning easier.
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
    "camera_flush_frames": 1,
    # 连续异常帧阈值（达到后触发提醒）
    "abnormal_threshold": 2,
    # 是否启用 ESP32 UART 输出
    "enable_esp32_uart": True,
    "uart_id": 1,
    "uart_baudrate": 115200,
    "uart_tx_pin": 3,
    "uart_rx_pin": 4,
    "save_images": False,
    "debug": False,
    # IDE 预览开关与参数
    "ide_preview": False,
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

# 姿态判定参数（关键点几何特征 + 风险分数 + 迟滞）
POSTURE_THRESHOLDS = {
    "posture_mode": "legacy",
    "posture_profile": "legacy_20260221222815",
    # 几何特征上限（超过即增加异常风险）
    # torso_tilt_max_deg: 躯干偏离竖直方向的容忍角度（度）
    # 调大 -> 更不敏感（不容易判驼背）；调小 -> 更敏感。
    # 建议步长：1~2；建议范围：12 ~ 30。
    "torso_tilt_max_deg": 18.0,
    # neck_tilt_max_deg: 颈部/头部相对竖直方向的容忍角度（度）
    # 调大 -> 更不敏感（不容易判低头）；调小 -> 更敏感。
    # 建议步长：1~2；建议范围：15 ~ 35。
    "neck_tilt_max_deg": 24.0,
    "neck_relative_tilt_max_deg": 24.0,
    # head_forward_ratio_max: 头部前探比例上限（|nose_x-shoulder_center_x| / shoulder_width）
    # 调大 -> 前探容忍更高；调小 -> 更容易判头前伸/低头。
    # 建议步长：0.02；建议范围：0.20 ~ 0.60。
    "head_forward_ratio_max": 0.45,
    # shoulder_roll_ratio_max: 肩膀高低差比例上限（|yL-yR| / shoulder_width）
    # 调大 -> 对侧倾更宽松；调小 -> 更容易判侧倾。
    # 建议步长：0.02；建议范围：0.10 ~ 0.35。
    "shoulder_roll_ratio_max": 0.17,
    # 风险特征权重（总和建议接近1）
    # 含义：各异常特征在综合风险中的占比，哪个权重大，哪个更影响最终异常结论。
    # 调参建议：某类误判多，就降低该类对应权重。
    "w_torso": 0.34,
    "w_neck": 0.30,
    "w_head_forward": 0.08,
    "w_roll": 0.14,
    "w_side_face": 0.14,
    "w_side_risk_factor": 1.00,
    "side_face_min_ratio": 0.70,
    "head_dominance_margin": 1.00,
    "torso_dominance_margin": 1.00,
    "tilt_dominance_margin": 1.00,
    # 风险平滑 + 迟滞阈值（降低抖动误判）
    # risk_ema_alpha: EMA平滑系数，越大越跟随当前帧，越小越平滑。
    # 建议步长：0.05；建议范围：0.20 ~ 0.70。
    "risk_ema_alpha": 0.50,
    # risk_enter: 从 normal 进入 abnormal 的阈值（应高于 risk_exit）。
    # 调大 -> 更难进入异常（降低敏感度）；调小 -> 更容易报警。
    # 建议步长：0.03；建议范围：0.45 ~ 0.80。
    "risk_enter": 0.60,
    "risk_instant_enter": 0.56,
    "risk_soft_enter": 0.50,
    "dominant_score_gap_enter": 0.030,
    # risk_exit: 从 abnormal 恢复 normal 的阈值。
    # 调大 -> 更容易恢复正常；调小 -> 异常状态保持更久。
    # 建议步长：0.03；建议范围：0.20 ~ 0.55。
    # 推荐关系：risk_enter > risk_exit，二者至少相差 0.10。
    "risk_exit": 0.46,
    "risk_instant_exit": 0.42,
    "baseline_warmup_frames": 20,
    "neck_delta_max_deg": 12.0,
    "torso_delta_max_deg": 10.0,
    "roll_delta_max": 0.16,
    "head_forward_delta_max": 0.20,
    "class_score_alpha": 0.50,
    "class_enter": 0.56,
    "class_exit": 0.42,
    "class_instant_enter": 0.62,
    "class_instant_exit": 0.36,
    "view_side_ratio": 0.14,
    "view_semiside_ratio": 0.24,
}
