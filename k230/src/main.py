"""
K230视觉模块主程序 - 坐姿检测系统
文件路径：K230Vision/src/main.py

功能说明：
    基于YOLOv8n-pose人体姿态检测的坐姿监测系统
    通过K230 AI模块进行实时人体关键点检测和姿态分析
    并通过UART与ESP32通信，实现智能坐姿提醒

系统架构：
    K230 Vision Module (本程序)
        ↓ 摄像头采集 (QVGA 320x240)
        ↓ YOLOv8n-pose推理 (NPU加速)
        ↓ 姿态分析 (正常/低头/驼背/未知)
        ↓ UART发送 (JSON格式)

    ESP32 Main Controller
        ↓ 接收数据并解析
        ↓ MQTT上传至OneNET云平台
        ↓ 控制RGB LED显示状态
        ↓ 触发语音/蜂鸣器提醒

依赖模块：
    - pose_detector: YOLOv8n-pose姿态检测器
    - camera: K230摄像头初始化

运行方式：
    1. 使用CanMV IDE打开此文件
    2. 连接到K230开发板（USB串口）
    3. 确保yolov8n_pose.kmodel已上传到/sdcard/models/
    4. 点击"运行"按钮
    5. 按Ctrl+C停止程序

硬件连接：
    K230          ESP32
    UART1_TX(GPIO3)  ->  GPIO18 (RX)
    UART1_RX(GPIO4)  <-  GPIO17 (TX)
    GND       ->  GND

    波特率：115200
    数据位：8
    停止位：1
    校验：无
    流控制：无

JSON输出格式：
    {
        "posture_type": "normal"          // 姿态类型: normal/head_down/hunchback/unknown/no_person
    }

日期: 2026-02
版本: 2.0
"""

import time
import gc
import machine
from machine import UART

# 导入自定义模块
try:
    from config import APP_CONFIG, MODEL_CONFIG
    from logger import get_logger
    from camera import CameraModule
    from pose_detector import PoseDetector
    from canmv_api import ensure_dir
except ImportError as e:
    print("[-] 错误：无法导入模块")
    print(f"    详细错误: {str(e)}")
    print("[*] 请确保以下文件在同一目录:")
    print("    - camera.py")
    print("    - pose_detector.py")
    raise


class SittingPostureMonitor:
    """
    坐姿检测监控系统

    主要功能：
        1. 初始化K230摄像头和AI姿态检测器
        2. 采集图像并进行YOLOv8n-pose姿态检测
        3. 分析坐姿状态（正常/低头/驼背/未知）
        4. 通过UART与ESP32通信，发送检测结果
        5. 连续监测并统计异常坐姿

    关键属性含义：
        - camera: 摄像头模块实例，负责图像采集
        - detector: 姿态检测器实例，负责关键点推理与姿态判定
        - uart: 与 ESP32 通信的串口对象
        - frame_count: 已处理帧总数
        - recog_latency_ms: 推理时延滑动窗口

    使用示例：
        >>> monitor = SittingPostureMonitor()
        >>> if monitor.initialize():
        ...     monitor.run(duration_seconds=600)  # 运行10分钟
    """

    def __init__(self):
        """
        初始化系统

        设置默认配置参数，但不初始化硬件。
        硬件初始化需要在initialize()方法中显式调用。
        """
        print("\n" + "=" * 60)
        print("智能语音坐姿提醒器 - K230视觉模块")
        print("=" * 60)

        # 系统组件（延迟初始化）
        self.camera = None  # 摄像头模块
        self.detector = None  # 姿态检测器
        self.uart = None  # UART通信（用于ESP32）

        # 系统状态
        self.is_running = False  # 是否正在运行
        self.frame_count = 0  # 总帧计数
        self.abnormal_count = 0  # 异常帧计数
        self.posture_history = []  # 姿态历史记录
        self._warned_detector_uninitialized = False

        # 配置参数
        self.config = APP_CONFIG.copy()
        self.test_mode = bool(self.config.get("test_mode", False))
        self.enable_esp32_uart = bool(self.config.get("enable_esp32_uart", False))
        self.ide_preview_enabled = bool(self.config.get("ide_preview", False))
        self.runtime_verbose_logs = bool(self.config.get("runtime_verbose_logs", False))
        self.metrics_enabled = bool(self.config.get("metrics_enabled", True))
        if self.test_mode:
            self.ide_preview_enabled = True
            self.runtime_verbose_logs = False
            self.metrics_enabled = False
        self.config["enable_esp32_uart"] = self.enable_esp32_uart
        self.config["ide_preview"] = self.ide_preview_enabled
        APP_CONFIG["ide_preview"] = self.ide_preview_enabled
        self.metrics_window_size = int(self.config.get("metrics_window_size", 50))
        self.metrics_log_interval_frames = int(
            self.config.get("metrics_log_interval_frames", 30)
        )
        self.recog_latency_ms = []
        self.no_person_reset_frames = int(self.config.get("no_person_reset_frames", 5))
        self.gc_collect_interval_frames = int(
            self.config.get("gc_collect_interval_frames", 80)
        )
        self.logger = get_logger("main", self.config.get("debug", False))
        self.uart_send_ok_count = 0
        self.uart_send_fail_count = 0
        self.last_uart_send_error = ""
        self.last_uart_send_ms = None
        self._uart_none_warned = False
        self.uart_send_interval_ms = int(self.config.get("uart_send_interval_ms", 400))
        self.detection_enabled = True
        self._uart_rx_buffer = bytearray()
        self._reported_detection_enabled = True

    def _now_ms(self):
        """
        获取当前毫秒时间戳。

        为什么这样做：
        - 在 CanMV/MicroPython 优先使用 `ticks_ms`，无该接口时回退到 `time.time`。

        返回：
        - 当前时间（毫秒，int）。
        """
        if hasattr(time, "ticks_ms"):
            return int(time.ticks_ms())
        return int(time.time() * 1000)

    def _diff_ms(self, end_ms, start_ms):
        """
        计算两个毫秒时间戳差值。

        为什么这样做：
        - `ticks_diff` 能处理计数器回绕，避免长时间运行后出现负值误差。

        返回：
        - 时间差（毫秒，int）。
        """
        if hasattr(time, "ticks_diff"):
            return int(time.ticks_diff(end_ms, start_ms))
        return int(end_ms - start_ms)

    def _push_window(self, buffer, value):
        """
        向滑动窗口追加样本并维持固定长度。

        为什么这样做：
        - 性能指标只关注最近窗口，避免历史长尾数据稀释当前状态。

        返回：
        - 无返回值。
        """
        buffer.append(value)
        if len(buffer) > self.metrics_window_size:
            buffer.pop(0)

    def _window_avg(self, buffer):
        """
        计算窗口均值。

        返回：
        - 缓冲区为空时返回 `0.0`，否则返回平均值。
        """
        if not buffer:
            return 0.0
        return float(sum(buffer)) / float(len(buffer))

    def _window_p95(self, buffer):
        """
        计算窗口内 P95 延迟。

        为什么这样做：
        - P95 能反映偶发卡顿，比均值更适合评估实时性体验。

        返回：
        - 缓冲区为空时返回 `0.0`，否则返回 P95 值。
        """
        if not buffer:
            return 0.0
        data = sorted(buffer)
        idx = int((len(data) - 1) * 0.95)
        return float(data[idx])

    def _resolve_fpioa_func(self, fpioa_obj, *names):
        """
        在 FPIOA 对象与其类型上查找可用 UART 功能常量。

        为什么这样做：
        - 不同固件版本常量挂载位置可能不同，需要做兼容查找。

        返回：
        - 找到时返回常量值，找不到返回 `None`。
        """
        for name in names:
            value = getattr(fpioa_obj, name, None)
            if value is not None:
                return value
            value = getattr(type(fpioa_obj), name, None)
            if value is not None:
                return value
        return None

    def _configure_uart_fpioa(self, uart_id):
        """
        配置 UART 引脚到 FPIOA 功能映射。

        为什么这样做：
        - K230 UART 是否能正常通信取决于引脚复用映射是否正确。

        返回：
        - `(True, "ok")` 表示映射成功。
        - `(False, 错误原因)` 表示映射失败（调用方可选择降级继续运行）。
        """
        fpioa_cls = getattr(machine, "FPIOA", None)
        if fpioa_cls is None:
            return False, "machine.FPIOA unavailable"

        tx_pin = int(self.config.get("uart_tx_pin", 3))
        rx_pin = int(self.config.get("uart_rx_pin", 4))
        fpioa = fpioa_cls()

        tx_func = self._resolve_fpioa_func(
            fpioa, "UART{}_TXD".format(uart_id), "UART_TXD"
        )
        rx_func = self._resolve_fpioa_func(
            fpioa, "UART{}_RXD".format(uart_id), "UART_RXD"
        )

        if tx_func is None or rx_func is None:
            return False, "missing FPIOA UART funcs tx={}, rx={}".format(
                tx_func, rx_func
            )

        fpioa.set_function(tx_pin, tx_func)
        fpioa.set_function(rx_pin, rx_func)
        print(
            "[*] FPIOA映射完成: TX pin {}, RX pin {}, UART{}".format(
                tx_pin, rx_pin, uart_id
            )
        )
        return True, "ok"

    def _init_uart(self):
        """
        初始化UART通信（简化版）

        尝试多种 UART 初始化方式以兼容不同固件版本。
        成功返回 UART 对象，失败返回 None。
        """
        try:
            uart_id = int(self.config.get("uart_id", 1) or 1)
            uart_baud = int(self.config.get("uart_baudrate", 115200) or 115200)
            tx_pin = int(self.config.get("uart_tx_pin", 3) or 3)
            rx_pin = int(self.config.get("uart_rx_pin", 4) or 4)
            print("[*] UART参数: id={}, baud={}".format(uart_id, uart_baud))
            print("[*] UART引脚: tx_pin={}, rx_pin={}".format(tx_pin, rx_pin))

            # FPIOA 引脚映射（失败不阻塞）
            fpioa_ok, fpioa_msg = self._configure_uart_fpioa(uart_id)
            if not fpioa_ok:
                print("[!] FPIOA配置失败: {}".format(fpioa_msg))

            # 构建候选 UART ID 列表（兼容不同固件常量命名）
            candidates = [uart_id]
            for name in ("UART1", "UART_1"):
                val = getattr(UART, name, None)
                if val is not None and val not in candidates:
                    candidates.append(val)

            # 按候选顺序尝试初始化
            for uid in candidates:
                for kwargs in ({"baudrate": uart_baud}, {}):
                    try:
                        uart = (
                            UART(uid, uart_baud, **kwargs)
                            if not kwargs
                            else UART(uid, **{"baudrate": uart_baud})
                        )
                        print(
                            "[+] UART初始化成功 (id={}, baud={})".format(uid, uart_baud)
                        )
                        return uart
                    except Exception:
                        pass
                # 也尝试位置参数形式
                try:
                    uart = UART(uid, uart_baud)
                    print(
                        "[+] UART初始化成功 (id={}, baud={}, positional)".format(
                            uid, uart_baud
                        )
                    )
                    return uart
                except Exception:
                    pass

            print("[-] 所有UART初始化方式均失败")
            return None
        except Exception as e:
            print("[-] UART初始化异常: {}".format(str(e)))
            return None

    def _emit_metrics(self):
        """
        输出当前性能指标快照。

        为什么这样做：
        - 统一在主循环中定期打印 KPI，便于调参与验收时核对时延目标。

        返回：
        - 无返回值。
        """
        avg_ms = self._window_avg(self.recog_latency_ms)
        p95_ms = self._window_p95(self.recog_latency_ms)
        frame_ok = avg_ms <= 100.0
        print(
            "[KPI] frame_avg_ms={:.1f} frame_p95_ms={:.1f} frame_le_100ms={}".format(
                avg_ms, p95_ms, frame_ok
            )
        )

    def initialize(self):
        """
        初始化所有硬件组件

        初始化顺序：
            1. 摄像头模块（QVGA 320x240）
            2. 姿态检测器（YOLOv8n-pose模型）
            3. UART通信（用于ESP32）
            4. 检查存储目录

        返回:
            bool: 初始化是否成功

        注意:
            即使某些组件初始化失败，系统也会尝试继续运行，
            但功能可能受限。
        """
        print("\n[*] 正在初始化系统...")
        if self.test_mode:
            print(
                "[*] 当前运行于测试模式：IDE预览=开，ESP32 UART跟随开关={}, 逐帧日志=开".format(
                    self.enable_esp32_uart
                )
            )

        # 1. 初始化摄像头
        print("\n[*] 步骤1/4: 初始化摄像头...")
        self.camera = CameraModule(
            sensor_id=self.config.get("camera_sensor_id", 2),
            channel=self.config.get("camera_channel", 0),
            framesize=self.config["camera_framesize"],
            pixformat=self.config["camera_pixformat"],
        )

        if not self.camera.initialize():
            print("[-] 摄像头初始化失败，系统退出")
            return False
        print("[+] 摄像头初始化成功")

        # 2. 初始化姿态检测器（YOLOv8n-pose）
        print("\n[*] 步骤2/4: 初始化姿态检测器（YOLOv8n-pose）...")
        self.detector = PoseDetector(
            model_path=MODEL_CONFIG["model_path"],
            debug=self.config.get("debug", False),
        )

        # 加载模型
        if not self.detector.initialize():
            print("[-] 姿态检测器初始化失败")
            print("[*] 提示: 请确保 yolov8n_pose.kmodel 已上传到 /sdcard/models/")
            # 继续运行但不进行姿态检测（使用模拟数据）
            print("[*] 将以模拟模式继续运行")
        else:
            print("[+] YOLOv8n-pose 模型加载成功！")
            self.startup_self_check()

        # 3. 初始化UART通信（与ESP32通信）
        print("\n[*] 步骤3/4: 初始化UART通信...")
        print(
            "[*] UART开关: enable_esp32_uart={}, uart_id={}, tx_pin={}, rx_pin={}, baud={}".format(
                self.enable_esp32_uart,
                self.config.get("uart_id", 1),
                self.config.get("uart_tx_pin", 3),
                self.config.get("uart_rx_pin", 4),
                self.config.get("uart_baudrate", 115200),
            )
        )
        self.uart = self._init_uart() if self.enable_esp32_uart else None
        if self.uart is None and self.enable_esp32_uart:
            print("[*] 继续运行，但无法与ESP32通信")
        elif self.uart is None:
            print("[*] 已禁用ESP32 UART，当前仅输出到K230 IDE串口")
        else:
            print("[+] ESP32 UART链路已启用，后续将直接发送JSON到ESP32")
        # 4. 检查存储目录
        print("\n[*] 步骤4/4: 检查存储目录...")
        try:
            ensure_dir("/sdcard/images")
            print("[+] 图像存储目录已就绪: /sdcard/images")
        except Exception as e:
            print(f"[-] 创建目录失败: {str(e)}")

        print("\n" + "=" * 60)
        print("[+] 系统初始化完成！")
        print("=" * 60)

        return True

    def startup_self_check(self):
        """
        启动自检：输出模型路径与KPU输入输出描述信息。
        """
        if self.detector is None:
            self.logger.warn("startup self-check skipped: detector is None")
            return

        info = self.detector.get_runtime_info()
        self.logger.info("Self-check model: {}".format(info.get("model_path")))
        self.logger.info("Self-check initialized: {}".format(info.get("initialized")))
        self.logger.info(
            "Self-check io counts: inputs={}, outputs={}".format(
                info.get("inputs_size"), info.get("outputs_size")
            )
        )

        input_desc = info.get("input_desc", [])
        output_desc = info.get("output_desc", [])
        if input_desc:
            self.logger.info("Input desc[0]: {}".format(input_desc[0]))
        if output_desc:
            self.logger.info("Output desc[0]: {}".format(output_desc[0]))

    def process_frame(self, img):
        """
        处理单帧图像 - 使用YOLOv8n-pose模型

        完整的处理流程：
            1. 运行YOLOv8n-pose检测人体姿态
            2. 提取关键点并分析坐姿
            3. 构建JSON输出结果

        参数:
            img: CanMV image对象，从摄像头采集的图像

        返回:
            dict: 处理结果，包含以下字段:
                - posture_type: str 姿态类型 ('normal', 'head_down', 'hunchback', 'unknown', 'no_person')
                - person_debug: dict 人体存在过滤中间量
                - posture_debug: dict 姿态角度与几何中间量

            如果检测失败，返回None
        """
        result = {
            "posture_type": "unknown",
            "person_debug": {},
            "posture_debug": {},
        }

        try:
            # 使用YOLOv8n-pose进行姿态检测
            if self.detector and self.detector.is_initialized:
                # 调用检测器的detect_pose方法
                detection_result = self.detector.detect_pose(img)

                if detection_result:
                    # 提取检测结果
                    pose_type = detection_result.get("pose_type", "unknown")
                    result["posture_type"] = pose_type
                    result["person_debug"] = detection_result.get("person_debug", {})
                    result["posture_debug"] = detection_result.get("angles", {})

                else:
                    # 未检测到人体
                    result["posture_type"] = "no_person"
                    result["person_debug"] = getattr(
                        self.detector, "last_person_debug", {}
                    )
                    result["posture_debug"] = {}

            else:
                # 检测器未初始化，使用模拟数据
                if not self._warned_detector_uninitialized:
                    print("[!] 警告: 检测器未初始化，使用模拟数据")
                    self._warned_detector_uninitialized = True
                result["posture_type"] = "normal"
                result["person_debug"] = {"reason": "detector_uninitialized_mock"}
                result["posture_debug"] = {}

        except Exception as e:
            print(f"[-] 图像处理失败: {str(e)}")
            import sys

            sys.print_exception(e)

        return result

    def send_to_esp32(self, data):
        """
        发送数据到ESP32

        通过UART将姿态检测结果发送给ESP32主控制器。
        ESP32将数据上传到OneNET云平台，并控制RGB LED显示状态。

        参数:
            data: dict 要发送的数据字典，包含基础信息
                - posture_type: str 姿态类型

        返回:
            bool: 是否发送成功

        UART通信协议：
            - 波特率: 115200
            - 数据位: 8
            - 停止位: 1
            - 校验: 无
            - 流控制: 无

        JSON格式示例：
            {
                "posture_type": "head_down"
            }
        """
        if self.uart is None:
            if not self._uart_none_warned:
                print("[!] send_to_esp32 skipped: UART is None")
                self._uart_none_warned = True
            self.uart_send_fail_count += 1
            self.last_uart_send_error = "uart_none"
            return False

        try:
            import json

            output = {
                "posture_type": data.get("posture_type", "unknown"),
            }
            json_str = json.dumps(output) + "\n"

            # 发送数据
            written = self.uart.write(json_str.encode())
            if written is None:
                written = len(json_str)
            self.uart_send_ok_count += 1
            self.last_uart_send_error = ""
            self.last_uart_send_ms = self._now_ms()
            if self.runtime_verbose_logs and (self.uart_send_ok_count % 30 == 0):
                print(
                    "[*] UART发送统计: ok={}, fail={}, last_bytes={}".format(
                        self.uart_send_ok_count,
                        self.uart_send_fail_count,
                        written,
                    )
                )
            return True

        except Exception as e:
            self.uart_send_fail_count += 1
            self.last_uart_send_error = str(e)
            print(f"[-] 发送数据到ESP32失败: {str(e)}")
            return False

    def _should_send_uart(self, now_ms):
        if self.last_uart_send_ms is None:
            return True
        if self.uart_send_interval_ms <= 0:
            return True
        return (
            self._diff_ms(now_ms, self.last_uart_send_ms) >= self.uart_send_interval_ms
        )

    def _apply_uart_command(self, raw_line):
        """
        解析来自 ESP32 的单行 UART 控制命令。

        当前仅支持：
            {"cmd":"set_detection","enabled":true/false}
        """
        if not raw_line:
            return

        try:
            text = raw_line.decode().strip()
        except Exception:
            return

        if not text:
            return

        try:
            import json

            payload = json.loads(text)
        except Exception as e:
            print("[!] 忽略无法解析的UART命令: {} ({})".format(text, str(e)))
            return

        if payload.get("cmd") != "set_detection":
            return

        enabled = bool(payload.get("enabled", True))
        if self.detection_enabled != enabled:
            self.detection_enabled = enabled
            self._reported_detection_enabled = False
            print("[*] 检测状态已切换: {}".format("开启" if enabled else "暂停"))

    def poll_uart_commands(self):
        """
        非阻塞轮询 ESP32 下发命令。

        即使检测被暂停，也必须保留这个入口以便后续恢复。
        """
        if self.uart is None:
            return

        try:
            while self.uart.any():
                chunk = self.uart.read(1)
                if not chunk:
                    break
                if chunk == b"\n":
                    self._apply_uart_command(bytes(self._uart_rx_buffer))
                    self._uart_rx_buffer = bytearray()
                elif chunk != b"\r":
                    if len(self._uart_rx_buffer) < 128:
                        self._uart_rx_buffer.extend(chunk)
                    else:
                        self._uart_rx_buffer = bytearray()
        except Exception as e:
            print("[!] UART命令轮询失败: {}".format(str(e)))

    def output_to_ide(self, data):
        """
        将结果输出到 CanMV IDE 串口。

        为什么这样做：
        - 测试模式下需要保留逐帧可观测性，即使同时启用了 ESP32 UART。
        - 非测试模式下，当未启用 ESP32 UART 时，IDE 串口是最直接的调试观察通道。

        返回：
        - 无返回值。
        """
        try:
            if self.test_mode:
                print(
                    "frame={} posture={} latency_ms={} fps={:.1f}".format(
                        data.get("frame_id", 0),
                        data.get("posture_type", "unknown"),
                        int(data.get("infer_ms", 0)),
                        float(data.get("fps", 0.0)),
                    )
                )
                return

            if not self.runtime_verbose_logs:
                print(data.get("posture_type", "unknown"))
                return

            import json

            output = {
                "frame_id": data.get("frame_id", 0),
                "posture_type": data.get("posture_type", "unknown"),
                "person_debug": data.get("person_debug", {}),
                "posture_debug": data.get("posture_debug", {}),
                "timestamp": time.ticks_ms(),
            }
            print(json.dumps(output))
        except Exception as e:
            print(f"[-] IDE串口输出失败: {str(e)}")

    def run(self, duration_seconds=None, max_frames=None):
        """
        运行主循环

        启动坐姿检测主循环，持续采集图像、检测姿态、分析结果并发送给ESP32。
        支持按时间或帧数限制运行时长。

        参数:
            duration_seconds: int/None 运行时长（秒），None表示无限运行
            max_frames: int/None 最大帧数，None表示无限制

        返回:
            None

        运行逻辑：
            1. 启动摄像头采集
            2. 每帧运行YOLOv8n-pose检测
            3. 分析坐姿状态（normal/head_down/hunchback/unknown/no_person）
            4. 必要时重置姿态稳定状态
            5. 测试模式下始终输出 IDE 串口日志；启用 UART 时并行发送最小 UART JSON
            6. 重复直到达到时长/帧数限制或用户中断

        异常处理：
            - KeyboardInterrupt (Ctrl+C): 正常停止，关闭资源
            - 摄像头采集失败: 跳过该帧，继续运行
            - 检测器错误: 记录错误，继续运行
            - UART发送失败: 记录错误，继续运行

        示例:
            >>> monitor = SittingPostureMonitor()
            >>> if monitor.initialize():
            ...     # 运行60秒
            ...     monitor.run(duration_seconds=60)
            ...     # 或者运行1000帧
            ...     monitor.run(max_frames=1000)
            ...     # 或者无限运行（按Ctrl+C停止）
            ...     # monitor.run()
        """
        if not self.is_running:
            print("\n[*] 启动主循环...")
            print("[*] 按Ctrl+C停止\n")
            self.is_running = True

        start_time = time.time()
        no_person_streak = 0

        try:
            while self.is_running:
                self.poll_uart_commands()

                # 检查时长限制
                if duration_seconds and (time.time() - start_time) >= duration_seconds:
                    print("\n[*] 达到运行时长限制，停止")
                    break

                # 检查帧数限制
                if max_frames and self.frame_count >= max_frames:
                    print(f"\n[*] 达到最大帧数限制 ({max_frames})")
                    break

                if not self.detection_enabled:
                    if not self._reported_detection_enabled:
                        print("[*] 检测已暂停，等待 ESP32 恢复命令")
                        self._reported_detection_enabled = True
                    time.sleep(0.05)
                    continue

                if not self._reported_detection_enabled:
                    print("[*] 检测已恢复")
                    self._reported_detection_enabled = True

                # 采集图像
                frame_start = time.time()
                if self.camera is None:
                    print("[-] 摄像头对象不存在，停止运行")
                    break
                img = self.camera.capture_frame()

                if img is None:
                    print("[-] 图像采集失败，跳过")
                    continue

                self.frame_count += 1

                # 处理图像（姿态检测）
                infer_start_ms = self._now_ms()
                result = self.process_frame(img)
                infer_end_ms = self._now_ms()
                infer_ms = self._diff_ms(infer_end_ms, infer_start_ms)
                if self.metrics_enabled:
                    self._push_window(self.recog_latency_ms, infer_ms)

                if result.get("posture_type") == "no_person":
                    no_person_streak += 1
                else:
                    no_person_streak = 0

                if (
                    self.no_person_reset_frames > 0
                    and no_person_streak == self.no_person_reset_frames
                ):
                    # 连续无人时清空姿态稳定状态，避免人员离开后回到画面立刻沿用旧类别。
                    if self.detector and hasattr(self.detector, "reset_posture_state"):
                        self.detector.reset_posture_state()

                is_abnormal_posture = result["posture_type"] in (
                    "hunchback",
                    "head_down",
                )
                if is_abnormal_posture and self.runtime_verbose_logs:
                    print("[!] 检测到异常姿势: {}".format(result["posture_type"]))

                # 计算帧率
                frame_time = time.time() - frame_start
                fps = 1.0 / frame_time if frame_time > 0 else 0

                output_payload = {
                    "frame_id": self.frame_count,
                    "posture_type": result["posture_type"],
                    "infer_ms": infer_ms,
                    "fps": fps,
                    "person_debug": result.get("person_debug", {}),
                    "posture_debug": result.get("posture_debug", {}),
                }

                if self.test_mode:
                    self.output_to_ide(output_payload)

                if self.enable_esp32_uart:
                    now_ms = self._now_ms()
                    if self._should_send_uart(now_ms):
                        self.send_to_esp32(output_payload)
                elif not self.test_mode:
                    self.output_to_ide(output_payload)

                if self.enable_esp32_uart and self.frame_count % 30 == 0:
                    print(
                        "[*] UART状态: ok={}, fail={}, uart_ready={}, last_error={}".format(
                            self.uart_send_ok_count,
                            self.uart_send_fail_count,
                            self.uart is not None,
                            self.last_uart_send_error or "none",
                        )
                    )

                # 显示状态（每30帧显示一次）
                if self.runtime_verbose_logs and self.frame_count % 30 == 0:
                    print(
                        "[*] 帧 {}, FPS: {:.1f}, 当前状态: {}".format(
                            self.frame_count, fps, result["posture_type"]
                        )
                    )

                if (
                    self.metrics_enabled
                    and self.metrics_log_interval_frames > 0
                    and self.frame_count % self.metrics_log_interval_frames == 0
                ):
                    self._emit_metrics()

                # 延时控制（根据配置的检测间隔）
                sleep_time = self.config["detection_interval"] / 1000.0 - frame_time
                if sleep_time > 0:
                    time.sleep(sleep_time)

                if (
                    self.gc_collect_interval_frames > 0
                    and self.frame_count % self.gc_collect_interval_frames == 0
                ):
                    gc.collect()

        except KeyboardInterrupt:
            print("\n[*] 用户中断")
        except Exception as e:
            print(f"\n[-] 运行时错误: {str(e)}")
        finally:
            self.stop()

    def stop(self):
        """
        停止系统

        关闭所有硬件资源：
            - 停止摄像头采集
            - 关闭UART通信
            - 释放内存
        """
        print("\n[*] 正在停止系统...")
        self.is_running = False

        # 关闭摄像头
        if self.camera:
            self.camera.deinit()

        # 关闭UART
        if self.uart:
            try:
                self.uart.deinit()
            except:
                pass

        print("[+] 系统已停止")
        print(f"[*] 共处理 {self.frame_count} 帧")


# ============ 主程序入口 ============
if __name__ == "__main__":
    print("\n" + "=" * 70)
    print("    智能语音坐姿提醒器 - K230视觉模块")
    print("    基于YOLOv8n-pose人体姿态识别")
    print("=" * 70 + "\n")

    # 创建系统实例
    monitor = SittingPostureMonitor()

    # 初始化系统
    if not monitor.initialize():
        print("[-] 系统初始化失败，程序退出")
        raise SystemExit(1)

    # 运行主循环
    # 参数说明:
    #   duration_seconds: 运行时长（秒），None表示无限运行
    #   max_frames: 最大帧数，None表示无限制
    #
    # 示例：
    #   monitor.run(duration_seconds=60)  # 运行60秒
    #   monitor.run(max_frames=1000)      # 采集1000帧
    #   monitor.run()                       # 无限运行（按Ctrl+C停止）

    try:
        # 默认运行10分钟或采集5000帧（先测试用）
        monitor.run(duration_seconds=600, max_frames=5000)

        # 正式运行时使用（无限运行）
        # monitor.run()

    except Exception as e:
        print(f"\n[-] 程序异常: {str(e)}")
        monitor.stop()
        raise
