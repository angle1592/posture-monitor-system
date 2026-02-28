import math
import sys

import nncase_runtime as nn
import ulab.numpy as np

from logger import get_logger
from config import MODEL_CONFIG, POSTURE_THRESHOLDS, PERSON_FILTER
from canmv_api import path_exists


def _hwc_to_chw(arr):
    shape = arr.shape
    flat = arr.reshape((shape[0] * shape[1], shape[2]))
    transposed = flat.transpose()
    copied = transposed.copy()
    return copied.reshape((shape[2], shape[0], shape[1]))


def _center_pad_param(input_size, output_size):
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
    KEYPOINT_INDICES = {
        "nose": 0,
        "left_shoulder": 5,
        "right_shoulder": 6,
        "left_hip": 11,
        "right_hip": 12,
    }

    def __init__(self, model_path=None, debug=False):
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
        self.risk_alpha = float(self.posture_thresholds.get("risk_ema_alpha", 0.4))
        self.risk_enter = float(self.posture_thresholds.get("risk_enter", 0.55))
        self.risk_exit = float(self.posture_thresholds.get("risk_exit", 0.35))
        self.risk_instant_enter = float(
            self.posture_thresholds.get("risk_instant_enter", self.risk_enter)
        )
        self.risk_soft_enter = float(
            self.posture_thresholds.get("risk_soft_enter", self.risk_instant_enter)
        )
        self.dominant_score_gap_enter = float(
            self.posture_thresholds.get("dominant_score_gap_enter", 0.035)
        )
        self.risk_instant_exit = float(
            self.posture_thresholds.get("risk_instant_exit", self.risk_exit)
        )
        self.side_face_min_ratio = float(
            self.posture_thresholds.get("side_face_min_ratio", 0.5)
        )
        self.w_side_face = float(self.posture_thresholds.get("w_side_face", 0.08))
        self.w_side_risk_factor = float(
            self.posture_thresholds.get("w_side_risk_factor", 0.35)
        )
        self.posture_mode = str(self.posture_thresholds.get("posture_mode", "legacy"))
        self.baseline_warmup_frames = int(
            self.posture_thresholds.get("baseline_warmup_frames", 20)
        )
        self.class_score_alpha = float(
            self.posture_thresholds.get("class_score_alpha", 0.5)
        )
        self.class_enter = float(self.posture_thresholds.get("class_enter", 0.56))
        self.class_exit = float(self.posture_thresholds.get("class_exit", 0.42))
        self.class_instant_enter = float(
            self.posture_thresholds.get("class_instant_enter", 0.62)
        )
        self.class_instant_exit = float(
            self.posture_thresholds.get("class_instant_exit", 0.36)
        )
        self.view_side_ratio = float(
            self.posture_thresholds.get("view_side_ratio", 0.14)
        )
        self.view_semiside_ratio = float(
            self.posture_thresholds.get("view_semiside_ratio", 0.24)
        )
        self.posture_state = "normal"
        self.posture_risk_ema = 0.0
        self.class_ema = {"head_down": 0.0, "hunchback": 0.0, "tilt": 0.0}
        self.baseline_ready = False
        self.baseline_count = 0
        self.baseline_neck = 0.0
        self.baseline_torso = 0.0
        self.baseline_roll = 0.0
        self.baseline_head_forward = 0.0

        self.kpu = None
        self.ai2d = None
        self.ai2d_builder = None
        self.ai2d_out_tensor = None
        self.is_initialized = False
        self.last_input_shape = None
        self.last_person_debug = {}

    def _log(self, message):
        self.logger.debug(message)

    def reset_posture_state(self):
        self.posture_state = "normal"
        self.posture_risk_ema = 0.0
        self.class_ema = {"head_down": 0.0, "hunchback": 0.0, "tilt": 0.0}

    def initialize(self):
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
        self.kpu.set_input_tensor(0, input_tensor)
        self.kpu.run()
        outputs = []
        for i in range(int(self.kpu.outputs_size())):
            out_tensor = self.kpu.get_output_tensor(i)
            outputs.append(out_tensor.to_numpy())
            del out_tensor
        return self._select_pose_output(outputs)

    def postprocess(self, output_data, conf_threshold=None, nms_threshold=None):
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

            # Some exported kmodels output bbox width/height in pixel space
            # (e.g. around model input size) instead of normalized 0~1.
            # If clipping collapses bbox to 0 area, use normalized raw_wh fallback.
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

            pose_type, angles = self._analyze_posture_detailed(best["keypoints"])
            best["pose_type"] = pose_type
            best["angles"] = angles
            best["person_debug"] = person_debug
            self.last_person_debug = person_debug
            return best
        except Exception as e:
            self._log("[-] detect_pose failed: {}".format(str(e)))
            self.last_person_debug = {"reason": "detect_exception", "error": str(e)}
            return None

    def _analyze_posture_detailed(self, keypoints):
        if self.posture_mode == "decoupled":
            return self._analyze_posture_decoupled(keypoints)
        if not keypoints or len(keypoints) < 17:
            return "unknown", {"neck": 0.0, "back": 0.0, "shoulder_diff": 0.0}

        nose = keypoints[self.KEYPOINT_INDICES["nose"]]
        left_shoulder = keypoints[self.KEYPOINT_INDICES["left_shoulder"]]
        right_shoulder = keypoints[self.KEYPOINT_INDICES["right_shoulder"]]
        left_hip = keypoints[self.KEYPOINT_INDICES["left_hip"]]
        right_hip = keypoints[self.KEYPOINT_INDICES["right_hip"]]

        shoulder_center = (
            (left_shoulder[0] + right_shoulder[0]) / 2.0,
            (left_shoulder[1] + right_shoulder[1]) / 2.0,
        )
        hip_center = (
            (left_hip[0] + right_hip[0]) / 2.0,
            (left_hip[1] + right_hip[1]) / 2.0,
        )

        shoulder_width = self._distance(left_shoulder, right_shoulder)
        hip_width = self._distance(left_hip, right_hip)
        if shoulder_width <= 1e-6:
            return "unknown", {"neck": 0.0, "back": 0.0, "shoulder_diff": 0.0}

        torso_tilt = self._tilt_from_vertical(shoulder_center, hip_center)
        neck_tilt = self._tilt_from_vertical(shoulder_center, nose)
        neck_relative_tilt = max(0.0, neck_tilt - torso_tilt)
        torso_length = self._distance(shoulder_center, hip_center)
        if torso_length <= 1e-6:
            return "unknown", {"neck": 0.0, "back": 0.0, "shoulder_diff": 0.0}
        stable_shoulder_width = max(shoulder_width, torso_length * 0.35)
        head_forward_ratio = abs(nose[0] - shoulder_center[0]) / stable_shoulder_width
        shoulder_roll_ratio = (
            abs(left_shoulder[1] - right_shoulder[1]) / stable_shoulder_width
        )
        shoulder_width_ratio = shoulder_width / torso_length
        hip_width_ratio = hip_width / torso_length
        side_face_ratio = min(shoulder_width_ratio, hip_width_ratio)

        c_shoulder = min(self._kp_conf(left_shoulder), self._kp_conf(right_shoulder))
        c_torso = min(
            c_shoulder, min(self._kp_conf(left_hip), self._kp_conf(right_hip))
        )
        c_neck = min(c_shoulder, self._kp_conf(nose))
        c_roll = c_shoulder

        thresholds = self.posture_thresholds
        sev_torso = self._severity(
            torso_tilt, float(thresholds.get("torso_tilt_max_deg", 18.0))
        )
        sev_neck_abs = self._severity(
            neck_tilt, float(thresholds.get("neck_tilt_max_deg", 24.0))
        )
        sev_neck_rel = self._severity(
            neck_relative_tilt,
            float(
                thresholds.get(
                    "neck_relative_tilt_max_deg",
                    thresholds.get("neck_tilt_max_deg", 24.0),
                )
            ),
        )
        sev_neck = max(sev_neck_rel, sev_neck_abs * 0.45)
        sev_head = self._severity(
            head_forward_ratio, float(thresholds.get("head_forward_ratio_max", 0.35))
        )
        sev_roll = self._severity(
            shoulder_roll_ratio, float(thresholds.get("shoulder_roll_ratio_max", 0.22))
        )
        sev_side = self._inverse_severity(
            side_face_ratio,
            float(thresholds.get("side_face_min_ratio", self.side_face_min_ratio)),
        )

        w_torso = float(thresholds.get("w_torso", 0.35)) * c_torso
        w_neck = float(thresholds.get("w_neck", 0.30)) * c_neck
        head_reliability = max(
            0.35,
            min(
                1.0,
                side_face_ratio
                / max(float(thresholds.get("side_face_min_ratio", 0.5)), 1e-6),
            ),
        )
        w_head = (
            float(thresholds.get("w_head_forward", 0.20)) * c_neck * head_reliability
        )
        w_roll = float(thresholds.get("w_roll", 0.15)) * c_roll
        w_side = float(thresholds.get("w_side_face", self.w_side_face)) * c_torso
        w_side_risk = w_side * float(
            thresholds.get("w_side_risk_factor", self.w_side_risk_factor)
        )

        weighted_sum = (
            w_torso * sev_torso
            + w_neck * sev_neck
            + w_head * sev_head
            + w_roll * sev_roll
            + w_side_risk * sev_side
        )
        weight_total = w_torso + w_neck + w_head + w_roll + w_side_risk
        risk = weighted_sum / weight_total if weight_total > 1e-6 else 0.0

        head_margin = float(thresholds.get("head_dominance_margin", 1.12))
        torso_margin = float(thresholds.get("torso_dominance_margin", 1.08))
        tilt_margin = float(thresholds.get("tilt_dominance_margin", 1.05))

        alpha = max(0.0, min(1.0, self.risk_alpha))
        # 用 EMA + 进入/退出双阈值做迟滞，减少边界帧抖动导致的标签闪烁。
        self.posture_risk_ema = alpha * risk + (1.0 - alpha) * self.posture_risk_ema
        head_score_now = max(sev_neck * w_neck, sev_head * w_head)
        torso_score_now = sev_torso * w_torso
        roll_score_now = max(sev_roll * w_roll, sev_side * w_side)
        scores_sorted = sorted(
            [head_score_now, torso_score_now, roll_score_now], reverse=True
        )
        dominant_gap = scores_sorted[0] - scores_sorted[1]
        if self.posture_state == "normal":
            if (
                self.posture_risk_ema >= self.risk_enter
                or risk >= self.risk_instant_enter
                or (
                    risk >= self.risk_soft_enter
                    and dominant_gap >= self.dominant_score_gap_enter
                )
            ):
                self.posture_state = "abnormal"
        elif self.posture_risk_ema <= self.risk_exit and risk <= self.risk_instant_exit:
            self.posture_state = "normal"

        dominant = self._dominant_abnormal(
            sev_torso=sev_torso,
            sev_neck=sev_neck,
            sev_head=sev_head,
            sev_roll=sev_roll,
            sev_side=sev_side,
            neck_tilt=neck_tilt,
            torso_tilt=torso_tilt,
            head_margin=head_margin,
            torso_margin=torso_margin,
            tilt_margin=tilt_margin,
            w_torso=w_torso,
            w_neck=w_neck,
            w_head=w_head,
            w_roll=w_roll,
            w_side=w_side,
        )
        posture = "normal" if self.posture_state == "normal" else dominant

        head_score = max(sev_neck * w_neck, sev_head * w_head)
        torso_score = sev_torso * w_torso
        roll_score = max(sev_roll * w_roll, sev_side * w_side)

        return posture, {
            "neck": float(neck_tilt),
            "neck_relative": float(neck_relative_tilt),
            "back": float(torso_tilt),
            "shoulder_diff": float(shoulder_roll_ratio),
            "shoulder_width_ratio": float(shoulder_width_ratio),
            "hip_width_ratio": float(hip_width_ratio),
            "side_face_ratio": float(side_face_ratio),
            "head_forward_ratio": float(head_forward_ratio),
            "head_reliability": float(head_reliability),
            "sev_torso": float(sev_torso),
            "sev_neck": float(sev_neck),
            "sev_neck_abs": float(sev_neck_abs),
            "sev_neck_rel": float(sev_neck_rel),
            "sev_head": float(sev_head),
            "sev_roll": float(sev_roll),
            "sev_side": float(sev_side),
            "score_head": float(head_score),
            "score_torso": float(torso_score),
            "score_tilt": float(roll_score),
            "score_gap": float(dominant_gap),
            "risk": float(risk),
            "risk_ema": float(self.posture_risk_ema),
        }

    def _analyze_posture_decoupled(self, keypoints):
        if not keypoints or len(keypoints) < 17:
            return "unknown", {"neck": 0.0, "back": 0.0, "shoulder_diff": 0.0}

        nose = keypoints[self.KEYPOINT_INDICES["nose"]]
        left_shoulder = keypoints[self.KEYPOINT_INDICES["left_shoulder"]]
        right_shoulder = keypoints[self.KEYPOINT_INDICES["right_shoulder"]]
        left_hip = keypoints[self.KEYPOINT_INDICES["left_hip"]]
        right_hip = keypoints[self.KEYPOINT_INDICES["right_hip"]]

        shoulder_center = (
            (left_shoulder[0] + right_shoulder[0]) / 2.0,
            (left_shoulder[1] + right_shoulder[1]) / 2.0,
        )
        hip_center = (
            (left_hip[0] + right_hip[0]) / 2.0,
            (left_hip[1] + right_hip[1]) / 2.0,
        )

        shoulder_width = self._distance(left_shoulder, right_shoulder)
        hip_width = self._distance(left_hip, right_hip)
        torso_length = self._distance(shoulder_center, hip_center)
        if shoulder_width <= 1e-6 or torso_length <= 1e-6:
            return "unknown", {"neck": 0.0, "back": 0.0, "shoulder_diff": 0.0}

        stable_shoulder_width = max(shoulder_width, torso_length * 0.35)
        torso_tilt = self._tilt_from_vertical(shoulder_center, hip_center)
        neck_tilt = self._tilt_from_vertical(shoulder_center, nose)
        shoulder_roll_ratio = (
            abs(left_shoulder[1] - right_shoulder[1]) / stable_shoulder_width
        )
        head_forward_ratio = abs(nose[0] - shoulder_center[0]) / stable_shoulder_width
        shoulder_width_ratio = shoulder_width / torso_length
        hip_width_ratio = hip_width / torso_length
        side_face_ratio = min(shoulder_width_ratio, hip_width_ratio)

        c_shoulder = min(self._kp_conf(left_shoulder), self._kp_conf(right_shoulder))
        c_torso = min(
            c_shoulder, min(self._kp_conf(left_hip), self._kp_conf(right_hip))
        )
        c_neck = min(c_shoulder, self._kp_conf(nose))

        if side_face_ratio < self.view_side_ratio:
            view_state = "side"
        elif side_face_ratio < self.view_semiside_ratio:
            view_state = "semi_side"
        else:
            view_state = "frontal"

        if (
            self.baseline_count < self.baseline_warmup_frames
            and c_torso > 0.25
            and c_neck > 0.25
        ):
            if view_state != "side":
                # baseline 只在非纯侧身视角更新，避免侧身几何比例污染基线。
                n = float(self.baseline_count)
                self.baseline_neck = (self.baseline_neck * n + neck_tilt) / (n + 1.0)
                self.baseline_torso = (self.baseline_torso * n + torso_tilt) / (n + 1.0)
                self.baseline_roll = (self.baseline_roll * n + shoulder_roll_ratio) / (
                    n + 1.0
                )
                self.baseline_head_forward = (
                    self.baseline_head_forward * n + head_forward_ratio
                ) / (n + 1.0)
                self.baseline_count += 1
                self.baseline_ready = self.baseline_count >= self.baseline_warmup_frames

        base_neck = self.baseline_neck if self.baseline_count > 0 else neck_tilt
        base_torso = self.baseline_torso if self.baseline_count > 0 else torso_tilt
        base_roll = (
            self.baseline_roll if self.baseline_count > 0 else shoulder_roll_ratio
        )
        base_head = (
            self.baseline_head_forward
            if self.baseline_count > 0
            else head_forward_ratio
        )

        neck_delta = max(0.0, neck_tilt - base_neck)
        torso_delta = max(0.0, torso_tilt - base_torso)
        roll_delta = max(0.0, shoulder_roll_ratio - base_roll)
        head_delta = max(0.0, head_forward_ratio - base_head)

        sev_neck_delta = self._severity(
            neck_delta, float(self.posture_thresholds.get("neck_delta_max_deg", 12.0))
        )
        sev_torso_delta = self._severity(
            torso_delta, float(self.posture_thresholds.get("torso_delta_max_deg", 10.0))
        )
        sev_roll_delta = self._severity(
            roll_delta, float(self.posture_thresholds.get("roll_delta_max", 0.16))
        )
        sev_head_delta = self._severity(
            head_delta,
            float(self.posture_thresholds.get("head_forward_delta_max", 0.20)),
        )
        sev_side = self._inverse_severity(
            side_face_ratio,
            float(
                self.posture_thresholds.get(
                    "side_face_min_ratio", self.side_face_min_ratio
                )
            ),
        )

        score_head_raw = 0.58 * sev_neck_delta + 0.42 * sev_head_delta
        score_hunch_raw = 0.70 * sev_torso_delta + 0.30 * max(
            0.0, sev_neck_delta - 0.25
        )
        score_tilt_raw = 0.62 * sev_roll_delta + 0.38 * sev_side

        if view_state == "side":
            score_head_raw *= 0.55
            score_hunch_raw *= 1.05
            score_tilt_raw *= 1.20
        elif view_state == "semi_side":
            score_head_raw *= 0.78
            score_hunch_raw *= 1.06
            score_tilt_raw *= 1.10
        else:
            score_head_raw *= 1.08
            score_tilt_raw *= 0.90

        reliability = max(0.2, min(1.0, c_torso * c_neck))
        score_head_raw *= reliability
        score_hunch_raw *= reliability
        score_tilt_raw *= max(0.25, min(1.0, c_shoulder))

        alpha = max(0.0, min(1.0, self.class_score_alpha))
        self.class_ema["head_down"] = (
            alpha * score_head_raw + (1.0 - alpha) * self.class_ema["head_down"]
        )
        self.class_ema["hunchback"] = (
            alpha * score_hunch_raw + (1.0 - alpha) * self.class_ema["hunchback"]
        )
        self.class_ema["tilt"] = (
            alpha * score_tilt_raw + (1.0 - alpha) * self.class_ema["tilt"]
        )

        top_raw = max(score_head_raw, score_hunch_raw, score_tilt_raw)
        top_ema = max(
            self.class_ema["head_down"],
            self.class_ema["hunchback"],
            self.class_ema["tilt"],
        )
        best_class = "head_down"
        if (
            self.class_ema["hunchback"] >= self.class_ema["head_down"]
            and self.class_ema["hunchback"] >= self.class_ema["tilt"]
        ):
            best_class = "hunchback"
        elif (
            self.class_ema["tilt"] >= self.class_ema["head_down"]
            and self.class_ema["tilt"] >= self.class_ema["hunchback"]
        ):
            best_class = "tilt"

        enter_reason = ""
        exit_reason = ""
        if self.posture_state == "normal":
            if top_ema >= self.class_enter:
                self.posture_state = "abnormal"
                enter_reason = "ema"
            elif top_raw >= self.class_instant_enter:
                self.posture_state = "abnormal"
                enter_reason = "instant"
        else:
            if top_ema <= self.class_exit and top_raw <= self.class_instant_exit:
                self.posture_state = "normal"
                exit_reason = "ema_and_raw"

        posture = "normal" if self.posture_state == "normal" else best_class

        return posture, {
            "view_state": view_state,
            "neck": float(neck_tilt),
            "back": float(torso_tilt),
            "shoulder_diff": float(shoulder_roll_ratio),
            "head_forward_ratio": float(head_forward_ratio),
            "side_face_ratio": float(side_face_ratio),
            "baseline_ready": bool(self.baseline_ready),
            "baseline_count": int(self.baseline_count),
            "baseline_neck": float(base_neck),
            "baseline_back": float(base_torso),
            "baseline_roll": float(base_roll),
            "baseline_head_forward": float(base_head),
            "neck_delta": float(neck_delta),
            "torso_delta": float(torso_delta),
            "roll_delta": float(roll_delta),
            "head_forward_delta": float(head_delta),
            "score_head_raw": float(score_head_raw),
            "score_hunch_raw": float(score_hunch_raw),
            "score_tilt_raw": float(score_tilt_raw),
            "score_head": float(self.class_ema["head_down"]),
            "score_torso": float(self.class_ema["hunchback"]),
            "score_tilt": float(self.class_ema["tilt"]),
            "risk": float(top_raw),
            "risk_ema": float(top_ema),
            "enter_reason": enter_reason,
            "exit_reason": exit_reason,
            "posture_mode": "decoupled",
        }

    def _tilt_from_vertical(self, p1, p2):
        dx = abs(float(p2[0] - p1[0]))
        dy = abs(float(p2[1] - p1[1]))
        if dx == 0.0 and dy == 0.0:
            return 0.0
        return math.degrees(math.atan2(dx, dy if dy > 1e-6 else 1e-6))

    def _distance(self, p1, p2):
        dx = float(p2[0] - p1[0])
        dy = float(p2[1] - p1[1])
        return math.sqrt(dx * dx + dy * dy)

    def _kp_conf(self, kp):
        if len(kp) >= 3:
            return max(0.0, min(1.0, float(kp[2])))
        return 0.0

    def _severity(self, value, limit):
        if limit <= 1e-6:
            return 0.0
        exceed = max(0.0, float(value) - float(limit))
        return min(1.0, exceed / float(limit))

    def _inverse_severity(self, value, minimum):
        if minimum <= 1e-6:
            return 0.0
        gap = max(0.0, float(minimum) - float(value))
        return min(1.0, gap / float(minimum))

    def _dominant_abnormal(
        self,
        sev_torso,
        sev_neck,
        sev_head,
        sev_roll,
        sev_side,
        neck_tilt,
        torso_tilt,
        head_margin,
        torso_margin,
        tilt_margin,
        w_torso,
        w_neck,
        w_head,
        w_roll,
        w_side,
    ):
        head_score = max(sev_neck * w_neck, sev_head * w_head)
        torso_score = sev_torso * w_torso
        roll_score = max(sev_roll * w_roll, sev_side * w_side)
        if (
            roll_score >= head_score * tilt_margin
            and roll_score >= torso_score * tilt_margin
        ):
            return "tilt"
        if (
            torso_score >= head_score * torso_margin
            and torso_score >= roll_score * 1.02
        ):
            if torso_score >= roll_score:
                return "hunchback"
        if head_score >= torso_score * head_margin and head_score >= roll_score * 1.02:
            return "head_down"
        if torso_score >= head_score and torso_score >= roll_score:
            return "hunchback"
        if head_score >= torso_score and head_score >= roll_score:
            return "head_down"
        return "tilt"
