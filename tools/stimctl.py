"""stimctl.py — хост-контроллер платы-стимулятора (auxiliary/Stimulus).

Прошейте stimulus на второй МК (env: -test_stimulus (прошивается на C3, порт см. pio device list)), соедините каналы с DUT
(карта — в шапке main.cpp стимулятора) и общий GND. Дальше отсюда:

    # найти порт стимулятора (PING по всем COM)
    python tools/stimctl.py --find

    # разовые команды
    python tools/stimctl.py COM7 "EN 1"
    python tools/stimctl.py COM7 "WAVE 1 50 0"
    python tools/stimctl.py COM7 "PULSE 1 0 200"

Ответ стимулятора печатается в stdout. Используется из сценарной обвязки
проверки Pin-примеров (см. examples/Pin/*/README.md, «Отчёт о проверке»).
"""

import sys
import time

import serial  # pyserial из penv PlatformIO


def find_stimulus():
    from serial.tools import list_ports

    for p in list_ports.comports():
        try:
            s = serial.Serial(p.device, 115200, timeout=0.5)
        except serial.SerialException:
            continue
        time.sleep(0.15)  # дать прошивке проглотить мусор подключения
        s.reset_input_buffer()
        s.write(b"PING\n")
        deadline = time.time() + 1.0
        found = False
        while time.time() < deadline:
            line = s.readline().decode(errors="replace").strip()
            if "PONG" in line:
                found = True
                break
        s.close()
        if found:
            print(p.device)
            return 0
    print("not found", file=sys.stderr)
    return 1


def open_stim(port):
    # rts/dtr ДО открытия: дефолтная пульсация DTR/RTS на USB-Serial-JTAG
    # перезагружает C3 — состояние каналов (WAVE и т.п.) терялось при каждом вызове
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 1.0
    s.rts = False
    s.dtr = False
    s.open()
    return s


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "--find":
        return find_stimulus()

    port, commands = sys.argv[1], sys.argv[2:]
    s = open_stim(port)
    time.sleep(0.15)
    s.reset_input_buffer()
    for cmd in commands:
        s.write((cmd + "\n").encode())
        deadline = time.time() + 15.0  # PULSE держит линию до конца импульса
        answered = False
        while time.time() < deadline:
            line = s.readline().decode(errors="replace").strip()
            if not line:
                continue
            print(line)
            if line.startswith("OK") or line.startswith("ERR") or line == "PONG":
                answered = True
                break
        if not answered:
            print("TIMEOUT on: %s" % cmd, file=sys.stderr)
            s.close()
            return 1
    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
