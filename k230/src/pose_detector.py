"""
坐姿检测核心算法模块。

模块职责：
- 负责 YOLOv8n-pose 模型加载、预处理、推理与后处理。
- 从关键点中提取几何特征并判定姿态类型（normal/head_down/hunchback/unknown）。
- 输出姿态风险中间量，供主程序打包为 UART JSON 发送给 ESP32。

与其他模块的关系：
- `main.py` 调用 `detect_pose` 获取姿态结果并进行异常计数。
- `config.py` 提供检测阈值、人体过滤参数与姿态判定参数。
- `logger.py` 提供调试日志输出能力。
"""

import math
import sys

import nncase_runtime as nn
import ulab.numpy as np

from logger import get_logger
from config import MODEL_CONFIG, POSTURE_THRESHOLDS, PERSON_FILTER
from canmv_api import path_exists


def _hwc_to_chw(arr):
    """
    将图像张量从 HWC 排列转换为 CHW 排列。

    为什么这样做：
    - KPU/AI2D 输入通常为 CHW，本函数用于统一输入格式。

    返回：
    - 转换后的 CHW 数组。
    """
    shape = arr.shape
    flat = arr.reshape((shape[0] * shape[1], shape[2]))
    transposed = flat.transpose()
    copied = transposed.copy()
    return copied.reshape((shape[2], shape[0], shape[1]))


def _center_pad_param(input_size, output_size):
    """
    计算按比例缩放后的中心填充参数。

    为什么这样做：
    - 保持宽高比缩放可减少关键点几何失真，居中填充便于后处理对齐。

    返回：
    - `(top, bottom, left, right)` 填充像素值。
    """
    ratio_w = output_size[0] / input_size[0]
    ratio_h = output_size[1] / input_size[1]
    ratio = min(ratio_w, ratio_h)
    new_w = int(ratio * input_size[0])
    new_h = int(ratio * input_size[1])
    dw = (output_size[0] - new_w) / 2
    dh = (output_size[1] - new_h) / 2
    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw - 0.1))
    return top, bottom, left, right


class PoseDetector:
    """
    YOLOv8n-pose 姿态检测器。

    类职责：
    - 管理模型与 AI2D 资源生命周期。
    - 执行人体检测、关键点提取与姿态分类。

    关键属性含义：
    - `kpu`：模型推理对象。
    - `ai2d_builder`：预处理图构建器，随输入尺寸变化重建。
    - `stable_posture_type`：当前稳定输出的姿态类别。
    - `pending_posture_type`：等待确认的新类别。
    - `last_person_debug`：最近一次人体存在过滤结果。
    """

    KEYPOINT_INDICES = {
        "nose": 0,
        "left_shoulder": 5,
        "right_shoulder": 6,
        "left_hip": 11,
        "right_hip": 12,
    }

    def __init__(self, model_path=None, debug=False):
        """
        初始化姿态检测器参数与状态。

        为什么这样做：
        - 在构造阶段完成阈值读取和状态变量初始化，推理阶段可直接使用。

        返回：
        - 无返回值。
        """
        cfg_path = MODEL_CONFIG.get(
            "model_path", "/sdcard/examples/kmodel/yolov8n-pose.kmodel"
        )
        self.model_path = model_path or cfg_path
        self.debug = debug
        self.logger = get_logger("pose", debug)

        self.input_size = (
            int(MODEL_CONFIG.get("input_width", 320)),
            int(MODEL_CONFIG.get("input_height", 320)),
        )
        self.num_keypoints = 17
        self.output_dims = 56

        self.conf_threshold = float(MODEL_CONFIG.get("conf_threshold", 0.25))
        self.nms_threshold = float(MODEL_CONFIG.get("nms_threshold", 0.45))
        self.kpt_conf_threshold = float(MODEL_CONFIG.get("kpt_conf_threshold", 0.30))
        self.posture_thresholds = POSTURE_THRESHOLDS.copy()
        self.person_present_threshold = float(
            PERSON_FILTER.get("person_present_threshold", 0.45)
        )
        self.person_min_bbox_area_ratio = float(
            PERSON_FILTER.get("person_min_bbox_area_ratio", 0.03)
        )
        self.person_min_keypoints = int(PERSON_FILTER.get("person_min_keypoints", 4))
        self.torso_tilt_threshold_deg = float(
            self.posture_thresholds.get("torso_tilt_threshold_deg", 18.0)
        )
        self.head_tilt_threshold_deg = float(
            self.posture_thresholds.get("head_tilt_threshold_deg", 24.0)
        )
        self.required_keypoint_confidence = float(
            self.posture_thresholds.get("required_keypoint_confidence", 0.25)
        )
        self.stable_frame_count = int(
            self.posture_thresholds.get("stable_frame_count", 1)
        )
        self.stable_posture_type = "normal"
        self.pending_posture_type = None
        self.pending_posture_count = 0

        self.kpu = None
        self.ai2d = None
        self.ai2d_builder = None
        self.ai2d_out_tensor = None
        self.is_initialized = False
        self.last_input_shape = None
        self.last_person_debug = {}

    def _log(self, message):
        """输出调试日志，返回无。"""
        self.logger.debug(message)

    def reset_posture_state(self):
        """
        重置姿态稳定状态。

        为什么这样做：
        - 连续无人后重新入镜时，避免沿用旧状态导致误报。

        返回：
        - 无返回值。
        """
        self.stable_posture_type = "normal"
        self.pending_posture_type = None
        self.pending_posture_count = 0

    def initialize(self):
        """
        加载 kmodel 并初始化 AI2D 预处理资源。

        返回：
        - `True` 表示可进入推理。
        - `False` 表示初始化失败。
        """
        try:
            if not path_exists(self.model_path):
                print("[-] 模型文件不存在:", self.model_path)
                return False

            self.kpu = nn.kpu()
            self.kpu.load_kmodel(self.model_path)
            self.ai2d = nn.ai2d()
            self.ai2d.set_dtype(
                nn.ai2d_format.NCHW_FMT,
                nn.ai2d_format.NCHW_FMT,
                np.uint8,
                np.uint8,
            )

            out_h = self.input_size[1]
            out_w = self.input_size[0]
            out_data = np.ones((1, 3, out_h, out_w), dtype=np.uint8)
            self.ai2d_out_tensor = nn.from_numpy(out_data)

            self.is_initialized = True
            return True
        except Exception as e:
            print("[-] 姿态检测器初始化失败:", str(e))
            try:
                sys.print_exception(e)
            except Exception:
                pass
            return False

    def get_runtime_info(self):
        """
        获取模型运行时信息，用于启动自检与调试展示。

        返回：
        - 包含模型路径、输入输出数量与描述的字典。
        """
        info = {
            "model_path": self.model_path,
            "initialized": self.is_initialized,
            "input_size": self.input_size,
            "inputs_size": None,
            "outputs_size": None,
            "input_desc": [],
            "output_desc": [],
        }
        if not self.is_initialized or self.kpu is None:
            return info
        try:
            info["inputs_size"] = int(self.kpu.inputs_size())
            info["outputs_size"] = int(self.kpu.outputs_size())
            for i in range(info["inputs_size"]):
                try:
                    info["input_desc"].append(self.kpu.inputs_desc(i))
                except Exception:
                    info["input_desc"].append("<unavailable>")
            for i in range(info["outputs_size"]):
                try:
                    info["output_desc"].append(self.kpu.outputs_desc(i))
                except Exception:
                    info["output_desc"].append("<unavailable>")
        except Exception as e:
            self._log("get_runtime_info failed: {}".format(str(e)))
        return info

    def _ensure_ai2d_builder(self, in_w, in_h):
        """
        按输入尺寸确保 AI2D builder 可用。

        为什么这样做：
        - 输入尺寸变化时必须重建预处理图，否则推理输入会错位。

        返回：
        - 无返回值。
        """
        if self.last_input_shape == (in_w, in_h) and self.ai2d_builder is not None:
            return
        top, bottom, left, right = _center_pad_param(
            [in_w, in_h], [self.input_size[0], self.input_size[1]]
        )
        self.ai2d.set_pad_param(
            True, [0, 0, 0, 0, top, bottom, left, right], 0, [0, 0, 0]
        )
        self.ai2d.set_resize_param(
            True, nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel
        )
        self.ai2d_builder = self.ai2d.build(
            [1, 3, in_h, in_w],
            [1, 3, self.input_size[1], self.input_size[0]],
        )
        self.last_input_shape = (in_w, in_h)

    def preprocess(self, input_np):
        """
        预处理输入图像到模型输入张量。

        数据流：
        - 图像对象/数组 -> CHW 规范化 -> 居中缩放填充 -> AI2D 输出张量。

        返回：
        - 成功返回模型输入 tensor，失败返回 `None`。
        """
        try:
            if hasattr(input_np, "to_numpy_ref"):
                input_np = input_np.to_numpy_ref()

            shape = input_np.shape
            if len(shape) != 3:
                return None

            if shape[0] == 3:
                chw = input_np
                in_h = shape[1]
                in_w = shape[2]
            elif shape[2] == 3:
                chw = _hwc_to_chw(input_np)
                in_h = shape[0]
                in_w = shape[1]
            else:
                return None

            self._ensure_ai2d_builder(in_w, in_h)
            ai2d_in = nn.from_numpy(chw)
            self.ai2d_builder.run(ai2d_in, self.ai2d_out_tensor)
            return self.ai2d_out_tensor
        except Exception as e:
            self._log("[-] preprocess failed: {}".format(str(e)))
            return None

    def _select_pose_output(self, outputs):
        """
        从多输出中选择姿态主输出。

        为什么这样做：
        - 不同导出模型输出顺序可能不同，需按维度特征自动识别。

        返回：
        - 匹配的输出数组；无可用输出时返回 `None`。
        """
        if not outputs:
            return None
        if len(outputs) == 1:
            return outputs[0]
        for out in outputs:
            try:
                shape = out.shape
                if len(shape) == 3 and (
                    shape[1] == self.output_dims or shape[2] == self.output_dims
                ):
                    return out
                if len(shape) == 2 and (
                    shape[0] == self.output_dims or shape[1] == self.output_dims
                ):
                    return out
            except Exception:
                pass
        return outputs[0]

    def _infer(self, input_tensor):
        """
        执行一次 KPU 推理并提取输出。

        返回：
        - 选中的姿态输出数组。
        """
        self.kpu.set_input_tensor(0, input_tensor)
        self.kpu.run()
        outputs = []
        for i in range(int(self.kpu.outputs_size())):
            out_tensor = self.kpu.get_output_tensor(i)
            outputs.append(out_tensor.to_numpy())
            del out_tensor
        return self._select_pose_output(outputs)

    def postprocess(self, output_data, conf_threshold=None, nms_threshold=None):
        """
        对模型输出做解码、阈值过滤与 NMS 去重。

        为什么这样做：
        - 将原始输出转换为可解释的人体框+关键点结构，供姿态分析使用。

        返回：
        - 检测结果列表（按置信度与 NMS 过滤后）。
        """
        if output_data is None:
            return []
        if conf_threshold is None:
            conf_threshold = self.conf_threshold
        if nms_threshold is None:
            nms_threshold = self.nms_threshold

        try:
            shape = output_data.shape
            if len(shape) == 3 and shape[0] == 1:
                output_data = output_data[0]
                shape = output_data.shape
            if len(shape) != 2:
                return []

            if shape[0] == self.output_dims:
                dims_first = True
                num_anchors = shape[1]
            elif shape[1] == self.output_dims:
                dims_first = False
                num_anchors = shape[0]
            else:
                return []

            detections = []
            for i in range(num_anchors):
                if dims_first:
                    cx = output_data[0, i]
                    cy = output_data[1, i]
                    w = output_data[2, i]
                    h = output_data[3, i]
                    conf = output_data[4, i]
                else:
                    cx = output_data[i, 0]
                    cy = output_data[i, 1]
                    w = output_data[i, 2]
                    h = output_data[i, 3]
                    conf = output_data[i, 4]
                if conf < conf_threshold:
                    continue

                x1 = max(0.0, min(cx - w / 2.0, 1.0))
                y1 = max(0.0, min(cy - h / 2.0, 1.0))
                x2 = max(0.0, min(cx + w / 2.0, 1.0))
                y2 = max(0.0, min(cy + h / 2.0, 1.0))

                keypoints = []
                for k in range(self.num_keypoints):
                    base = 5 + k * 3
                    if dims_first:
                        kx = output_data[base, i]
                        ky = output_data[base + 1, i]
                        kc = output_data[base + 2, i]
                    else:
                        kx = output_data[i, base]
                        ky = output_data[i, base + 1]
                        kc = output_data[i, base + 2]
                    keypoints.append((float(kx), float(ky), float(kc)))

                detections.append(
                    {
                        "bbox": (float(x1), float(y1), float(x2), float(y2)),
                        "raw_bbox": (
                            float(cx - w / 2.0),
                            float(cy - h / 2.0),
                            float(cx + w / 2.0),
                            float(cy + h / 2.0),
                        ),
                        "raw_wh": (float(w), float(h)),
                        "confidence": float(conf),
                        "keypoints": keypoints,
                    }
                )
            return self._nms(detections, nms_threshold)
        except Exception as e:
            self._log("[-] postprocess failed: {}".format(str(e)))
            return []

    def _nms(self, detections, threshold):
        """
        执行非极大值抑制。

        返回：
        - 去除重叠框后的检测列表。
        """
        if not detections:
            return []
        candidates = sorted(detections, key=lambda x: x["confidence"], reverse=True)
        keep = []
        while candidates:
            current = candidates.pop(0)
            keep.append(current)
            remained = []
            for det in candidates:
                if self._calculate_iou(current["bbox"], det["bbox"]) < threshold:
                    remained.append(det)
            candidates = remained
        return keep

    def _calculate_iou(self, box1, box2):
        """
        计算两个边界框的 IoU。

        返回：
        - IoU 数值（0~1）。
        """
        x1_min, y1_min, x1_max, y1_max = box1
        x2_min, y2_min, x2_max, y2_max = box2
        xi_min = max(x1_min, x2_min)
        yi_min = max(y1_min, y2_min)
        xi_max = min(x1_max, x2_max)
        yi_max = min(y1_max, y2_max)
        if xi_max <= xi_min or yi_max <= yi_min:
            return 0.0
        inter = (xi_max - xi_min) * (yi_max - yi_min)
        area1 = (x1_max - x1_min) * (y1_max - y1_min)
        area2 = (x2_max - x2_min) * (y2_max - y2_min)
        union = area1 + area2 - inter
        if union <= 0:
            return 0.0
        return inter / union

    def detect_pose(self, input_np):
        """
        对单帧输入执行完整姿态检测流程。

        数据流：
        - 预处理 -> 推理 -> 后处理 -> 人体存在过滤 -> 姿态判定。

        返回：
        - 成功时返回包含 `pose_type/confidence/keypoints/angles/person_debug` 的字典。
        - 失败或无人时返回 `None`。
        """
        if not self.is_initialized:
            self.last_person_debug = {"reason": "detector_not_initialized"}
            return None
        input_tensor = self.preprocess(input_np)
        if input_tensor is None:
            self.last_person_debug = {"reason": "preprocess_failed"}
            return None
        try:
            output_data = self._infer(input_tensor)
            detections = self.postprocess(output_data)
            if not detections:
                self.last_person_debug = {
                    "reason": "no_det",
                    "confidence": 0.0,
                    "person_present_threshold": self.person_present_threshold,
                    "bbox_area": 0.0,
                    "person_min_bbox_area_ratio": self.person_min_bbox_area_ratio,
                    "valid_kpt": 0,
                    "person_min_keypoints": self.person_min_keypoints,
                }
                return None
            best = detections[0]

            bbox = best.get("bbox", (0.0, 0.0, 0.0, 0.0))
            bbox_area_clipped = max(0.0, (bbox[2] - bbox[0]) * (bbox[3] - bbox[1]))
            raw_wh = best.get("raw_wh", (0.0, 0.0))
            raw_w = abs(float(raw_wh[0]))
            raw_h = abs(float(raw_wh[1]))

            # 部分导出的 kmodel 会把 bbox 宽高输出为像素尺度（接近输入尺寸），
            # 而不是 0~1 归一化尺度；因此这里先做归一化修正。
            # 当裁剪后面积退化为 0 时，回退使用 raw_wh 归一化面积，避免误判 small_bbox。
            if raw_w > 1.5:
                raw_w = raw_w / float(self.input_size[0])
            if raw_h > 1.5:
                raw_h = raw_h / float(self.input_size[1])
            bbox_area_raw = max(0.0, min(1.0, raw_w * raw_h))
            bbox_area = max(bbox_area_clipped, bbox_area_raw)
            keypoints = best.get("keypoints", [])
            valid_kpt = 0
            for kp in keypoints:
                if len(kp) >= 3 and kp[2] >= self.kpt_conf_threshold:
                    valid_kpt += 1

            confidence = float(best.get("confidence", 0.0))
            person_debug = {
                "reason": "pass",
                "confidence": confidence,
                "person_present_threshold": self.person_present_threshold,
                "bbox_area": bbox_area,
                "bbox_area_clipped": bbox_area_clipped,
                "bbox_area_raw": bbox_area_raw,
                "raw_w_norm": raw_w,
                "raw_h_norm": raw_h,
                "person_min_bbox_area_ratio": self.person_min_bbox_area_ratio,
                "valid_kpt": valid_kpt,
                "person_min_keypoints": self.person_min_keypoints,
            }

            if confidence < self.person_present_threshold:
                person_debug["reason"] = "low_conf"
                self.last_person_debug = person_debug
                return None
            if bbox_area < self.person_min_bbox_area_ratio:
                person_debug["reason"] = "small_bbox"
                self.last_person_debug = person_debug
                return None
            if valid_kpt < self.person_min_keypoints:
                person_debug["reason"] = "low_kpt"
                self.last_person_debug = person_debug
                return None

            pose_type, angles = self._analyze_posture(best["keypoints"])
            best["pose_type"] = pose_type
            best["angles"] = angles
            best["person_debug"] = person_debug
            self.last_person_debug = person_debug
            return best
        except Exception as e:
            self._log("[-] detect_pose failed: {}".format(str(e)))
            self.last_person_debug = {"reason": "detect_exception", "error": str(e)}
            return None

    def _analyze_posture(self, keypoints):
        """
        基于 5 个关键点的几何规则判定姿态类别。

        为什么这样做：
        - 直接使用鼻子、双肩、双髋的几何关系，便于论文解释。
        - 仅保留驼背与低头两类异常，并输出最小必要调试量。

        返回：
        - `(posture_type, angles_dict)`。
        """
        required_keypoints = self._extract_required_keypoints(keypoints)
        if required_keypoints is None:
            return "unknown", {
                "required_keypoints_ready": False,
                "torso_tilt_deg": 0.0,
                "head_tilt_deg": 0.0,
                "torso_excess_ratio": 0.0,
                "head_excess_ratio": 0.0,
                "stable_frame_count": int(self.stable_frame_count),
            }

        nose = required_keypoints["nose"]
        left_shoulder = required_keypoints["left_shoulder"]
        right_shoulder = required_keypoints["right_shoulder"]
        left_hip = required_keypoints["left_hip"]
        right_hip = required_keypoints["right_hip"]

        shoulder_midpoint = self._midpoint(left_shoulder, right_shoulder)
        hip_midpoint = self._midpoint(left_hip, right_hip)
        torso_length = self._distance(shoulder_midpoint, hip_midpoint)
        if torso_length <= 1e-6:
            return "unknown", {
                "required_keypoints_ready": False,
                "torso_tilt_deg": 0.0,
                "head_tilt_deg": 0.0,
                "torso_excess_ratio": 0.0,
                "head_excess_ratio": 0.0,
                "stable_frame_count": int(self.stable_frame_count),
            }

        torso_tilt_deg = self._tilt_from_vertical(shoulder_midpoint, hip_midpoint)
        head_tilt_deg = self._tilt_from_vertical(nose, shoulder_midpoint)
        torso_excess_ratio = self._excess_ratio(
            torso_tilt_deg, self.torso_tilt_threshold_deg
        )
        head_excess_ratio = self._excess_ratio(
            head_tilt_deg, self.head_tilt_threshold_deg
        )

        pose_type = "normal"
        if torso_excess_ratio > 0.0 and head_excess_ratio <= 0.0:
            pose_type = "hunchback"
        elif head_excess_ratio > 0.0 and torso_excess_ratio <= 0.0:
            pose_type = "head_down"
        elif torso_excess_ratio > 0.0 and head_excess_ratio > 0.0:
            if torso_excess_ratio > head_excess_ratio:
                pose_type = "hunchback"
            else:
                pose_type = "head_down"

        stable_pose_type = self._stabilize_posture_type(pose_type)
        return stable_pose_type, {
            "required_keypoints_ready": True,
            "shoulder_midpoint": [
                float(shoulder_midpoint[0]),
                float(shoulder_midpoint[1]),
            ],
            "hip_midpoint": [float(hip_midpoint[0]), float(hip_midpoint[1])],
            "torso_length": float(torso_length),
            "torso_tilt_deg": float(torso_tilt_deg),
            "head_tilt_deg": float(head_tilt_deg),
            "torso_excess_ratio": float(torso_excess_ratio),
            "head_excess_ratio": float(head_excess_ratio),
            "stable_frame_count": int(self.stable_frame_count),
            "stable_posture_type": stable_pose_type,
        }

    def _extract_required_keypoints(self, keypoints):
        """
        提取姿态判定所需的 5 个关键点。

        返回：
        - 关键点字典。
        - 任意关键点缺失或置信度不足时返回 `None`。
        """
        if not keypoints or len(keypoints) <= self.KEYPOINT_INDICES["right_hip"]:
            return None

        required_names = (
            "nose",
            "left_shoulder",
            "right_shoulder",
            "left_hip",
            "right_hip",
        )
        result = {}
        for name in required_names:
            keypoint = keypoints[self.KEYPOINT_INDICES[name]]
            if len(keypoint) < 3:
                return None
            if self._kp_conf(keypoint) < self.required_keypoint_confidence:
                return None
            result[name] = keypoint
        return result

    def _stabilize_posture_type(self, pose_type):
        """
        按连续帧数要求稳定姿态类别。

        返回：
        - 当前稳定输出的姿态类别。
        """
        if self.stable_frame_count <= 1:
            self.stable_posture_type = pose_type
            self.pending_posture_type = None
            self.pending_posture_count = 0
            return pose_type

        if pose_type == self.stable_posture_type:
            self.pending_posture_type = None
            self.pending_posture_count = 0
            return self.stable_posture_type

        if pose_type != self.pending_posture_type:
            self.pending_posture_type = pose_type
            self.pending_posture_count = 1
            return self.stable_posture_type

        self.pending_posture_count += 1
        if self.pending_posture_count >= self.stable_frame_count:
            self.stable_posture_type = pose_type
            self.pending_posture_type = None
            self.pending_posture_count = 0
        return self.stable_posture_type

    def _tilt_from_vertical(self, p1, p2):
        """
        计算两点连线相对竖直方向的倾斜角。

        返回：
        - 倾角（度）。
        """
        dx = abs(float(p2[0] - p1[0]))
        dy = abs(float(p2[1] - p1[1]))
        if dx == 0.0 and dy == 0.0:
            return 0.0
        return math.degrees(math.atan2(dx, dy if dy > 1e-6 else 1e-6))

    def _distance(self, p1, p2):
        """计算两点欧氏距离并返回浮点值。"""
        dx = float(p2[0] - p1[0])
        dy = float(p2[1] - p1[1])
        return math.sqrt(dx * dx + dy * dy)

    def _midpoint(self, p1, p2):
        """计算两点中点并返回 `(x, y)`。"""
        return (
            (float(p1[0]) + float(p2[0])) / 2.0,
            (float(p1[1]) + float(p2[1])) / 2.0,
        )

    def _kp_conf(self, kp):
        """
        提取并裁剪关键点置信度。

        返回：
        - 0~1 之间的置信度。
        """
        if len(kp) >= 3:
            return max(0.0, min(1.0, float(kp[2])))
        return 0.0

    def _excess_ratio(self, value, threshold):
        """
        计算数值相对阈值的超限比例。

        返回：
        - 未超阈值时返回 `0.0`，否则返回超限比例。
        """
        if threshold <= 1e-6:
            return 0.0
        exceed = max(0.0, float(value) - float(threshold))
        return exceed / float(threshold)
