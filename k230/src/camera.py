"""
摄像头模块封装。

模块职责：
- 完成 K230 摄像头、预览显示层与媒体管线初始化。
- 提供单帧采集与连续采集接口，供主循环调用。

与其他模块的关系：
- `main.py` 通过 `CameraModule` 获取图像帧，再交给 `pose_detector.py` 推理。
- `config.py` 中的摄像头参数会直接影响本模块初始化行为与帧质量。
"""

import time

from media.sensor import Sensor
from media.sensor import CAM_CHN_ID_0
from media.sensor import CAM_CHN_ID_1
from media.sensor import CAM_CHN_ID_2
from media.media import MediaManager
from media.display import Display

from config import APP_CONFIG
from logger import get_logger
from canmv_api import resolve_channel
from canmv_api import resolve_framesize
from canmv_api import resolve_pixformat
from canmv_api import ensure_dir


class CameraModule:
    """
    K230 摄像头管理类。

    类职责：
    - 管理传感器对象生命周期（初始化、采集、反初始化）。
    - 统一处理采集通道与预览通道配置。

    关键属性含义：
    - `sensor_device`：底层 Sensor 实例。
    - `channel`：AI 推理取帧通道。
    - `preview_channel`：IDE/显示预览通道。
    - `is_initialized`：摄像头是否可安全采帧。
    """

    def __init__(
        self, framesize=None, pixformat=None, sensor_id=2, channel=CAM_CHN_ID_0
    ):
        """
        初始化摄像头模块对象。

        为什么这样做：
        - 构造阶段只保存配置，不立刻访问硬件，便于主程序统一控制初始化时机。

        返回：
        - 无返回值。
        """
        self.framesize = framesize
        self.pixformat = pixformat
        self.sensor_id = sensor_id
        self.channel = channel
        self.is_initialized = False
        self.frame_count = 0
        self.sensor_device = None
        self.media_initialized = False
        self.display_initialized = False
        self.preview_channel = APP_CONFIG.get("camera_preview_channel", CAM_CHN_ID_0)
        self.preview_pixformat = APP_CONFIG.get("camera_preview_pixformat", "RGB565")
        self.preview_mode = str(APP_CONFIG.get("ide_preview_mode", "LT9611")).upper()
        self.preview_fps = int(APP_CONFIG.get("ide_preview_fps", 30))
        self.flush_frames = int(APP_CONFIG.get("camera_flush_frames", 0))
        self.logger = get_logger("camera", APP_CONFIG.get("debug", False))

    def initialize(self):
        """
        初始化摄像头、显示层和媒体管理器。

        为什么这样做：
        - 将硬件启动步骤集中在一个入口，出现问题时更容易定位。

        返回：
        - `True` 表示初始化成功。
        - `False` 表示初始化失败。
        """
        try:
            print("[*] 正在初始化K230摄像头...")

            self.channel = resolve_channel(
                self.channel, CAM_CHN_ID_0, CAM_CHN_ID_1, CAM_CHN_ID_2
            )
            self.preview_channel = resolve_channel(
                self.preview_channel, CAM_CHN_ID_0, CAM_CHN_ID_1, CAM_CHN_ID_2
            )

            self.sensor_device = Sensor(id=self.sensor_id)
            self.sensor_device.reset()

            framesize = resolve_framesize(self.framesize, Sensor)
            pixformat = resolve_pixformat(self.pixformat, Sensor)
            preview_pixformat = resolve_pixformat(self.preview_pixformat, Sensor)

            self.sensor_device.set_framesize(framesize, chn=self.preview_channel)
            self.sensor_device.set_pixformat(
                preview_pixformat, chn=self.preview_channel
            )

            self.sensor_device.set_framesize(framesize, chn=self.channel)
            self.sensor_device.set_pixformat(pixformat, chn=self.channel)
            self.logger.info("分辨率与像素格式已设置")

            if APP_CONFIG.get("ide_preview", True):
                if self.preview_mode == "ST7701":
                    Display.init(Display.ST7701, to_ide=True, osd_num=1)
                elif self.preview_mode == "VIRT":
                    # VIRT 预览需要显式宽高，这里按 framesize 做一层映射兜底。
                    size_map = {
                        "QQVGA": (160, 120),
                        "QVGA": (320, 240),
                        "VGA": (640, 480),
                        "HD": (1280, 720),
                        "FHD": (1920, 1080),
                    }
                    width, height = size_map.get(
                        str(self.framesize).upper(), (320, 240)
                    )
                    Display.init(
                        Display.VIRT,
                        width=width,
                        height=height,
                        fps=self.preview_fps,
                        to_ide=True,
                    )
                else:
                    Display.init(Display.LT9611, to_ide=True, osd_num=1)

                bind_info = self.sensor_device.bind_info(chn=self.preview_channel)
                Display.bind_layer(**bind_info, layer=Display.LAYER_VIDEO1)
                self.display_initialized = True

            MediaManager.init()
            self.media_initialized = True

            self.sensor_device.run()
            print("[+] 摄像头已启动")

            self.is_initialized = True
            self.frame_count = 0
            self.print_info()
            return True

        except Exception as e:
            print("[-] 摄像头初始化失败: {}".format(str(e)))
            self.is_initialized = False
            return False

    def capture_frame(self, save_path=None):
        """
        采集单帧图像，可选保存到文件。

        为什么这样做：
        - 主循环按帧调用该接口，保持“采集-推理-发送”数据流清晰。

        返回：
        - 成功时返回图像对象。
        - 失败时返回 `None`。
        """
        if not self.is_initialized:
            print("[-] 摄像头未初始化，请先调用initialize()")
            return None

        try:
            if self.sensor_device is None:
                return None
            img = None
            flush_count = self.flush_frames if self.flush_frames > 0 else 0
            # 某些传感器切换后首帧不稳定，允许丢弃前 N 帧来提升推理一致性。
            for _ in range(flush_count + 1):
                img = self.sensor_device.snapshot(chn=self.channel)
            if img is None:
                return None
            self.frame_count += 1

            if save_path:
                dir_path = "/".join(save_path.split("/")[:-1])
                if dir_path:
                    try:
                        ensure_dir(dir_path)
                    except Exception:
                        pass
                img.save(save_path)
                print("[+] 图像已保存: {}".format(save_path))

            return img

        except Exception as e:
            print("[-] 图像采集失败: {}".format(str(e)))
            return None

    def capture_preview_frame(self):
        """
        预留的预览帧接口。

        为什么这样做：
        - 为后续扩展预览抓帧能力保留方法签名，避免影响调用方。

        返回：
        - 当前固定返回 `None`。
        """
        return None

    def capture_continuous(self, callback, interval_ms=100, max_frames=None):
        """
        按固定间隔连续采集并回调处理。

        为什么这样做：
        - 适合离线采集或调试，不影响主程序常规单帧流程。

        返回：
        - 无返回值，采集结果通过 `callback` 回传。
        """
        if not self.is_initialized:
            print("[-] 摄像头未初始化")
            return

        frame_count = 0
        print("[*] 开始连续采集，间隔 {}ms".format(interval_ms))

        try:
            while max_frames is None or frame_count < max_frames:
                img = self.capture_frame()
                if img is None:
                    break

                callback(img, frame_count)
                frame_count += 1

                if interval_ms > 0:
                    time.sleep(interval_ms / 1000.0)

        except KeyboardInterrupt:
            print("\n[*] 用户停止采集")

        print("[+] 共采集 {} 帧".format(frame_count))

    def set_windowing(self, roi):
        """
        设置传感器采集窗口（ROI）。

        为什么这样做：
        - 可将采集焦点限制在画面局部，减少无关区域干扰。

        返回：
        - 无返回值。
        """
        try:
            if self.sensor_device is None:
                return
            self.sensor_device.set_windowing(roi)
            print("[+] 采集窗口设置: {}".format(roi))
        except Exception as e:
            print("[-] 设置窗口失败: {}".format(str(e)))

    def print_info(self):
        """
        打印当前摄像头配置与状态信息。

        返回：
        - 无返回值。
        """
        print("\n" + "=" * 50)
        print("K230摄像头信息")
        print("=" * 50)
        print("分辨率配置: {}".format(self.framesize if self.framesize else "QVGA"))
        print("像素格式配置: {}".format(self.pixformat if self.pixformat else "RGB565"))
        print("初始化状态: {}".format("成功" if self.is_initialized else "失败"))
        print("已采集帧数: {}".format(self.frame_count))
        print("=" * 50 + "\n")

    def deinit(self):
        """
        关闭摄像头及相关显示/媒体资源。

        为什么这样做：
        - 主程序退出时需要显式释放硬件资源，避免下次启动冲突。

        返回：
        - 无返回值。
        """
        try:
            if self.sensor_device is None:
                return

            self.sensor_device.stop()
            if self.display_initialized:
                Display.deinit()
                self.display_initialized = False
            if self.media_initialized:
                MediaManager.deinit()
                self.media_initialized = False

            print("[+] 摄像头已关闭")
            self.is_initialized = False

        except Exception as e:
            print("[-] 关闭摄像头失败: {}".format(str(e)))
