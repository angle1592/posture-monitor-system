#include "mode_manager.h"

/**
 * @brief mode_manager.cpp 实现说明
 *
 * 实现要点：
 * - EC11 旋转检测在中断中仅做最小计数，主循环再统一消费，降低 ISR 负担。
 * - 方向判定基于 CLK 下降沿 + DT 电平关系，配合消抖减少误计数。
 * - 按键采用“原始态 -> 稳定态”两级状态机，在释放沿判定短按/长按。
 * - 模式切换采用环形取模，保证在 0..MODE_COUNT-1 内循环。
 *
 * 内部状态变量含义：
 * - _currentMode/_modeChanged：当前模式与本轮是否切换。
 * - _rotationLocked/_pendingDelta：旋转是否锁定及锁定期间累计调节量。
 * - _pendingClick：待消费按键事件。
 * - _ec11RawDelta：ISR 累计的原始旋转脉冲增量。
 */

// Static variables
static SystemMode _currentMode = MODE_POSTURE;
static bool _modeChanged = false;
static bool _rotationLocked = false;
static int _pendingDelta = 0;
static ModeClickEvent _pendingClick = MODE_CLICK_NONE;

#if ENABLE_EC11
static volatile int _ec11RawDelta = 0;
static int _ec11LastClk = HIGH;
static bool _btnRawPressed = false;
static bool _btnStablePressed = false;
static unsigned long _btnLastChangeMs = 0;
static unsigned long _btnPressStartMs = 0;

static void IRAM_ATTR _ec11ISR() {
    int clkState = digitalRead(EC11_S1_PIN);
    if (clkState != _ec11LastClk && clkState == LOW) {
        // 仅在 CLK 下降沿判向，降低抖动下的重复计数概率。
        // 方向规则：比较 DT 与 CLK 当前态，不同方向对应 +1/-1。
        if (digitalRead(EC11_S2_PIN) != clkState) {
            _ec11RawDelta++;
        } else {
            _ec11RawDelta--;
        }
    }
    _ec11LastClk = clkState;
}
#endif

void mode_init() {
    _currentMode = MODE_POSTURE;
    _modeChanged = false;
    _rotationLocked = false;
    _pendingDelta = 0;
    _pendingClick = MODE_CLICK_NONE;

#if ENABLE_EC11
    // 启用上拉，匹配大多数 EC11 模块开漏/机械触点输出特性。
    pinMode(EC11_S1_PIN, INPUT_PULLUP);
    pinMode(EC11_S2_PIN, INPUT_PULLUP);
    pinMode(EC11_KEY_PIN, INPUT_PULLUP);

    _ec11LastClk = digitalRead(EC11_S1_PIN);
    _ec11RawDelta = 0;
    _btnRawPressed = digitalRead(EC11_KEY_PIN) == LOW;
    _btnStablePressed = _btnRawPressed;
    _btnLastChangeMs = millis();
    _btnPressStartMs = 0;

    // 仅挂 CLK 引脚中断，DT 作为方向辅助采样信号。
    attachInterrupt(digitalPinToInterrupt(EC11_S1_PIN), _ec11ISR, CHANGE);
    LOGI("EC11 初始化完成");
#else
    // 禁用 EC11 时仍保留模式管理能力（可由软件接口切换）。
    LOGI("EC11 未启用（ENABLE_EC11=0）");
#endif
}

void mode_update() {
    _modeChanged = false;

#if ENABLE_EC11
    // 将 ISR 累积量原子取走，避免主循环与中断并发读写冲突。
    noInterrupts();
    int delta = _ec11RawDelta;
    _ec11RawDelta = 0;
    interrupts();

    if (delta != 0) {
        if (_rotationLocked) {
            // 锁定时不改模式，把旋转量缓存给上层做参数调节（如定时时长）。
            _pendingDelta += delta;
        } else {
            // 未锁定时直接进行模式轮转：顺时针 +1，逆时针 -1（并做环形回绕）。
            int nextMode = (int)_currentMode;
            if (delta > 0) {
                nextMode = (nextMode + 1) % MODE_COUNT;
            } else {
                nextMode = (nextMode + MODE_COUNT - 1) % MODE_COUNT;
            }

            if (nextMode != (int)_currentMode) {
                _currentMode = (SystemMode)nextMode;
                _modeChanged = true;
                LOGI("[模式切换] -> %s", mode_getName());
            }
        }
    }

    unsigned long now = millis();
    bool rawPressed = digitalRead(EC11_KEY_PIN) == LOW;
    if (rawPressed != _btnRawPressed) {
        // 原始电平变化只更新时间戳，不立即触发事件，先等待消抖窗口。
        _btnRawPressed = rawPressed;
        _btnLastChangeMs = now;
    }

    if ((now - _btnLastChangeMs) >= EC11_DEBOUNCE_MS) {
        if (_btnStablePressed != _btnRawPressed) {
            _btnStablePressed = _btnRawPressed;
            if (_btnStablePressed) {
                // 稳定按下：记录按下起点，时长在释放时结算。
                _btnPressStartMs = now;
            } else {
                // 在按键释放时判短按/长按，避免按下瞬间误触发动作。
                unsigned long pressMs = now - _btnPressStartMs;
                if (pressMs >= EC11_LONG_PRESS_MS) {
                    _pendingClick = MODE_CLICK_LONG;
                } else {
                    _pendingClick = MODE_CLICK_SHORT;
                }
            }
        }
    }
#endif
}

SystemMode mode_getCurrent() {
    return _currentMode;
}

bool mode_setCurrent(SystemMode mode) {
    if ((int)mode < MODE_POSTURE || (int)mode >= MODE_COUNT) {
        return false;
    }
    if (_currentMode != mode) {
        _currentMode = mode;
        _modeChanged = true;
        LOGI("[模式切换] -> %s", mode_getName());
    }
    return true;
}

bool mode_hasChanged() {
    return _modeChanged;
}

const char* mode_getName() {
    switch (_currentMode) {
        case MODE_POSTURE: return "坐姿检测";
        case MODE_CLOCK: return "时钟";
        case MODE_TIMER: return "定时器";
        default: return "未知";
    }
}

void mode_setRotationLocked(bool locked) {
    _rotationLocked = locked;
    if (!locked) {
        // 解锁时清空历史增量，避免旧输入影响后续业务参数。
        _pendingDelta = 0;
    }
}

int mode_takeRotationDelta() {
    int delta = _pendingDelta;
    _pendingDelta = 0;
    return delta;
}

ModeClickEvent mode_takeClickEvent() {
    ModeClickEvent click = _pendingClick;
    _pendingClick = MODE_CLICK_NONE;
    return click;
}
