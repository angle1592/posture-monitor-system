_LEVELS = {
    "DEBUG": 10,
    "INFO": 20,
    "WARN": 30,
    "ERROR": 40,
}


class ProjectLogger:
    def __init__(self, name, level="INFO"):
        self.name = name
        self.level = level if level in _LEVELS else "INFO"

    def set_level(self, level):
        if level in _LEVELS:
            self.level = level

    def _enabled(self, level):
        # 数值越大级别越高：仅输出不低于当前门槛的日志。
        return _LEVELS[level] >= _LEVELS[self.level]

    def _emit(self, level, message):
        if self._enabled(level):
            print("[{}][{}] {}".format(level, self.name, message))

    def debug(self, message):
        self._emit("DEBUG", message)

    def info(self, message):
        self._emit("INFO", message)

    def warn(self, message):
        self._emit("WARN", message)

    def error(self, message):
        self._emit("ERROR", message)


def get_logger(name, debug_enabled=False):
    level = "DEBUG" if debug_enabled else "INFO"
    return ProjectLogger(name, level)
