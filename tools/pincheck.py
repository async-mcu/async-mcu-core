"""pincheck.py — чтение COM-порта DUT (ESP32) для сценарной проверки Pin-примеров.

Открывает порт БЕЗ дёрганья DTR/RTS (иначе CH340 сбрасывает ESP32 при каждом
открытии). Режимы:

    reset <port> <secs>              аппаратный ресет (RTS-пульс) + чтение
    read  <port> <secs>              чтение без ресета
    wait  <port> <pattern> <secs>    чтение без ресета до строки-паттерна
                                     (regex, grep по всему буферу); exit 0 — найдено

Стимул на вторую плату — tools/stimctl.py (COM12, ESP32-C3).
"""

import re
import sys
import time

import serial


def open_port(port):
    # rts/dtr до open: иначе CH340 при открытии дергает EN и сбрасывает ESP32
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.3
    s.rts = False
    s.dtr = False
    s.open()
    return s


def read_for(s, secs):
    buf = b""
    end = time.time() + secs
    while time.time() < end:
        chunk = s.read(4096)
        if chunk:
            buf += chunk
    return buf


def main():
    mode = sys.argv[1]
    port = sys.argv[2]
    s = open_port(port)

    if mode == "reset":
        s.setDTR(False)
        s.setRTS(True)
        time.sleep(0.1)
        s.setRTS(False)
        out = read_for(s, float(sys.argv[3]))
    elif mode == "read":
        out = read_for(s, float(sys.argv[3]))
    elif mode == "wait":
        pattern = sys.argv[3].encode()
        secs = float(sys.argv[4])
        out = b""
        end = time.time() + secs
        while time.time() < end:
            chunk = s.read(4096)
            if chunk:
                out += chunk
                if re.search(pattern, out):
                    break
        s.close()
        sys.stdout.buffer.write(out)
        return 0 if re.search(pattern, out) else 1
    else:
        print("unknown mode", file=sys.stderr)
        return 2

    s.close()
    sys.stdout.buffer.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
