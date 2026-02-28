"""
日志工具模块。

模块职责：
- 提供轻量级日志输出能力，统一主程序、摄像头和姿态检测模块的日志格式。
- 在 MicroPython/CanMV 环境下使用 print 输出，避免依赖复杂日志框架。

与其他模块的关系：
- `main.py`、`camera.py`、`pose_detector.py` 通过 `get_logger` 获取实例。
- 通过 `debug_enabled` 开关控制调试日志是否输出。
"""

_LEVELS = {
    "DEBUG": 10,
    "INFO": 20,
    "WARN": 30,
    "ERROR": 40,
}


class ProjectLogger:
    """
    项目日志类。

    类职责：
    - 管理单个模块的日志输出级别与格式。

    关键属性含义：
    - `name`：日志来源模块名（如 main/pose/camera）。
    - `level`：当前最小输出级别，低于该级别的日志会被过滤。
    """

    def __init__(self, name, level="INFO"):
        """
        创建日志对象。

        为什么这样做：
        - 在对象初始化阶段就固定模块名与级别，后续调用只需传 message。

        返回：
        - 无返回值，直接初始化实例状态。
        """
        self.name = name
        self.level = level if level in _LEVELS else "INFO"

    def set_level(self, level):
        """
        动态更新日志门槛级别。

        为什么这样做：
        - 运行中可按需打开/关闭更详细日志，便于排查问题。

        返回：
        - 无返回值。
        """
        if level in _LEVELS:
            self.level = level

    def _enabled(self, level):
        """
        判断指定日志级别是否应当输出。

        为什么这样做：
        - 先做门槛判断，再输出文本，可避免无效 print 带来的性能损耗。

        返回：
        - `True` 表示允许输出，`False` 表示过滤。
        """
        # 数值越大级别越高：仅输出不低于当前门槛的日志。
        return _LEVELS[level] >= _LEVELS[self.level]

    def _emit(self, level, message):
        """
        统一格式输出日志文本。

        为什么这样做：
        - 所有日志都经过同一出口，格式稳定，便于 IDE 串口检索。

        返回：
        - 无返回值。
        """
        if self._enabled(level):
            print("[{}][{}] {}".format(level, self.name, message))

    def debug(self, message):
        """输出 DEBUG 级日志，返回无。"""
        self._emit("DEBUG", message)

    def info(self, message):
        """输出 INFO 级日志，返回无。"""
        self._emit("INFO", message)

    def warn(self, message):
        """输出 WARN 级日志，返回无。"""
        self._emit("WARN", message)

    def error(self, message):
        """输出 ERROR 级日志，返回无。"""
        self._emit("ERROR", message)


def get_logger(name, debug_enabled=False):
    """
    获取项目日志实例。

    为什么这样做：
    - 统一由工厂函数创建日志器，减少各模块重复配置逻辑。

    返回：
    - `ProjectLogger` 对象。
    """
    level = "DEBUG" if debug_enabled else "INFO"
    return ProjectLogger(name, level)
