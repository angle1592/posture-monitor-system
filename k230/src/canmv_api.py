import os


def _resolve_attr(sensor_cls, value, default_name, accepted):
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
    accepted = {
        "QQVGA",
        "QVGA",
        "VGA",
        "HD",
        "FHD",
    }
    return _resolve_attr(sensor_cls, value, "QVGA", accepted)


def resolve_pixformat(value, sensor_cls):
    accepted = {
        "RGB565",
        "RGB888",
        "RGBP888",
        "GRAYSCALE",
        "YUV420SP",
    }
    return _resolve_attr(sensor_cls, value, "RGB565", accepted)


def resolve_channel(value, chn0, chn1, chn2):
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
