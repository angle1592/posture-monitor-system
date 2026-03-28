"""
K230 UART 最小诊断脚本。

用途：
1. 周期性向 ESP32 发送固定 heartbeat 文本；
2. 在 CanMV IDE 串口打印发送计数；
3. 回显从 ESP32 收到的原始字节与整行文本。

运行方式：
- 上传本文件到 K230 并在 CanMV IDE 中运行；
- 不依赖摄像头、模型或姿态检测模块；
- 用于隔离验证 UART 物理链路本身是否可用。
"""

import time
from machine import UART
import machine

from config import APP_CONFIG


HEARTBEAT_INTERVAL_MS = 500
READ_POLL_INTERVAL_MS = 20


def _now_ms():
    if hasattr(time, "ticks_ms"):
        return int(time.ticks_ms())
    return int(time.time() * 1000)


def _diff_ms(end_ms, start_ms):
    if hasattr(time, "ticks_diff"):
        return int(time.ticks_diff(end_ms, start_ms))
    return int(end_ms - start_ms)


def _resolve_fpioa_func(fpioa_obj, *names):
    for name in names:
        value = getattr(fpioa_obj, name, None)
        if value is not None:
            return value
        value = getattr(type(fpioa_obj), name, None)
        if value is not None:
            return value
    return None


def _configure_uart_fpioa(uart_id, tx_pin, rx_pin):
    fpioa_cls = getattr(machine, "FPIOA", None)
    if fpioa_cls is None:
        print("[diag] machine.FPIOA unavailable")
        return False

    fpioa = fpioa_cls()
    tx_func = _resolve_fpioa_func(fpioa, "UART{}_TXD".format(uart_id), "UART_TXD")
    rx_func = _resolve_fpioa_func(fpioa, "UART{}_RXD".format(uart_id), "UART_RXD")

    if tx_func is None or rx_func is None:
        print("[diag] missing FPIOA funcs: tx={}, rx={}".format(tx_func, rx_func))
        return False

    fpioa.set_function(tx_pin, tx_func)
    fpioa.set_function(rx_pin, rx_func)
    print(
        "[diag] FPIOA mapped: UART{} tx_pin={} rx_pin={}".format(
            uart_id, tx_pin, rx_pin
        )
    )
    return True


def _init_uart():
    uart_id = int(APP_CONFIG.get("uart_id", 2) or 2)
    uart_baud = int(APP_CONFIG.get("uart_baudrate", 115200) or 115200)
    tx_pin = int(APP_CONFIG.get("uart_tx_pin", 5) or 5)
    rx_pin = int(APP_CONFIG.get("uart_rx_pin", 6) or 6)

    print(
        "[diag] UART config: id={}, baud={}, tx_pin={}, rx_pin={}".format(
            uart_id, uart_baud, tx_pin, rx_pin
        )
    )
    _configure_uart_fpioa(uart_id, tx_pin, rx_pin)

    candidates = [uart_id]
    for name in ("UART{}".format(uart_id), "UART_{}".format(uart_id)):
        value = getattr(UART, name, None)
        if value is not None and value not in candidates:
            candidates.append(value)

    for uid in candidates:
        try:
            uart = UART(uid, baudrate=uart_baud)
            print("[diag] UART init ok: id={}".format(uid))
            return uart
        except Exception as e:
            print("[diag] UART init failed for id={}: {}".format(uid, str(e)))

    return None


def main():
    print("\n=== K230 UART diagnostic demo ===")
    print("Purpose: heartbeat TX + RX echo")

    uart = _init_uart()
    if uart is None:
        print("[diag] UART unavailable, abort")
        return

    send_count = 0
    recv_count = 0
    last_send_ms = _now_ms()
    line_buf = bytearray()

    while True:
        now = _now_ms()
        if _diff_ms(now, last_send_ms) >= HEARTBEAT_INTERVAL_MS:
            last_send_ms = now
            send_count += 1
            msg = "K230_UART_OK_{}\n".format(send_count)
            uart.write(msg.encode())
            print("[diag][TX] {}".format(msg.strip()))

        while uart.any():
            data = uart.read(1)
            if not data:
                break

            recv_count += 1
            byte_value = data[0]
            print("[diag][RX][BYTE] 0x{:02X}".format(byte_value))

            if byte_value in (10, 13):
                if line_buf:
                    try:
                        print("[diag][RX][LINE] {}".format(line_buf.decode()))
                    except Exception:
                        print("[diag][RX][LINE] <decode failed>")
                    line_buf = bytearray()
            else:
                line_buf.extend(data)

        if send_count and (send_count % 10 == 0):
            print("[diag][STAT] tx={}, rx={}".format(send_count, recv_count))

        time.sleep_ms(READ_POLL_INTERVAL_MS)


if __name__ == "__main__":
    main()
