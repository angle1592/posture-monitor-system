"""
CanMV/K230 平台 API 封装工具。

模块职责：
- 封装传感器常量解析、通道解析和路径处理等平台相关差异。
- 为 `camera.py` 与 `main.py` 提供稳定、可复用的基础能力。

与其他模块的关系：
- `camera.py` 通过本模块解析 framesize/pixformat/channel，减少硬编码。
- `main.py` 通过 `ensure_dir` 在启动阶段准备图像目录。
"""

import os


def _resolve_attr(sensor_cls, value, default_name, accepted):
    """
    将配置值解析为 Sensor 常量。

    为什么这样做：
    - 配置文件使用字符串更易读，但底层 API 需要常量对象。
    - 统一在这里做校验与转换，避免调用方重复写判断。

    返回：
    - 对应的传感器常量值。
    """
    target = value if value is not None else default_name
    if not isinstance(target, str):
        return target

    # 统一转大写，兼容配置里 mixed-case 的写法。
    key = target.upper()
    if key not in accepted:
        raise ValueError("unsupported sensor setting: {}".format(target))

    if hasattr(sensor_cls, key):
        return getattr(sensor_cls, key)

    raise AttributeError("sensor constant not found: {}".format(key))


def resolve_framesize(value, sensor_cls):
    """
    解析分辨率配置。

    返回：
    - `Sensor` 支持的 framesize 常量。
    """
    accepted = {
        "QQVGA",
        "QVGA",
        "VGA",
        "HD",
        "FHD",
    }
    return _resolve_attr(sensor_cls, value, "QVGA", accepted)


def resolve_pixformat(value, sensor_cls):
    """
    解析像素格式配置。

    返回：
    - `Sensor` 支持的 pixformat 常量。
    """
    accepted = {
        "RGB565",
        "RGB888",
        "RGBP888",
        "GRAYSCALE",
        "YUV420SP",
    }
    return _resolve_attr(sensor_cls, value, "RGB565", accepted)


def resolve_channel(value, chn0, chn1, chn2):
    """
    将配置中的通道标识映射为底层通道常量。

    为什么这样做：
    - 兼容数字、字符串和常量名三种写法，方便调试与移植。

    返回：
    - 匹配的通道常量。
    """
    channel_map = {
        0: chn0,
        1: chn1,
        2: chn2,
        "0": chn0,
        "1": chn1,
        "2": chn2,
        "CAM_CHN_ID_0": chn0,
        "CAM_CHN_ID_1": chn1,
        "CAM_CHN_ID_2": chn2,
    }
    if value not in channel_map:
        raise ValueError("unsupported channel: {}".format(value))
    return channel_map[value]


def path_exists(path):
    """
    判断路径在 CanMV 文件系统中是否存在。

    为什么这样做：
    - CanMV 项目常见绝对路径访问，这里统一补前缀并做兼容。

    返回：
    - `True` 表示存在，`False` 表示不存在或访问失败。
    """
    try:
        if path.startswith("./"):
            path = path[1:]
        if not path.startswith("/"):
            # CanMV 常用绝对路径访问存储，统一补前缀避免路径歧义。
            path = "/" + path
        os.stat(path)
        return True
    except Exception:
        return False


def ensure_dir(path):
    """
    递归确保目录存在。

    为什么这样做：
    - MicroPython 环境通常没有 `makedirs`，需逐级创建目录。

    返回：
    - 无返回值；目录已存在时静默通过。
    """
    if not path:
        return

    if path_exists(path):
        return

    current = ""
    for part in path.split("/"):
        if not part:
            continue
        current = current + "/" + part
        if not path_exists(current):
            os.mkdir(current)
