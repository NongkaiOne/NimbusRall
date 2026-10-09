# ภาพรวม: อ่าน COM3 และส่ง MEASURE ทุก 10 วินาทีภายในช่วงทดสอบ 45 วินาที แล้วบันทึกข้อความ
"""Capture the current-input model without setting a clock; request manual samples."""
import time
from pathlib import Path
import serial

lines = []
try:
    with serial.Serial('COM3', 115200, timeout=0.4) as port:
        port.dtr = False
        port.rts = False
        start = time.monotonic()
        next_measure = 10
        while time.monotonic() - start < 45:
            if time.monotonic() - start >= next_measure:
                port.write(b'MEASURE\n')
                next_measure += 10
            raw = port.readline()
            if raw:
                line = raw.decode('utf-8', errors='replace').rstrip()
                lines.append(line)
                print(line, flush=True)
finally:
    out = Path('.test-build/prediction-offline-test.txt')
    out.parent.mkdir(exist_ok=True)
    out.write_text('\n'.join(lines), encoding='utf-8')
