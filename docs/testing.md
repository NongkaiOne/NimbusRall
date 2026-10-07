# การตรวจสอบ

รันจาก root project ด้วย Python 3.10+ (ชุดนี้ทดสอบ Python 3.11) และ Node.js:

```powershell
python -m venv .venv
.\.venv\Scripts\python -m pip install -r training/requirements.txt -r logger/requirements.txt -r tests/requirements.txt
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path (Get-Location) '.test-build/zig-cache'
New-Item -ItemType Directory -Force .test-build
.\.venv\Scripts\python -m ziglang c++ tests/parity.cpp -I include -o .test-build/parity.exe -O2 -std=c++17 -target x86_64-windows-gnu -Wno-nullability-completeness
.\.venv\Scripts\python -m ziglang c++ tests/motor_test.cpp src/motor.cpp -I tests/stubs -I include -o .test-build/motor_test.exe -O2 -std=c++17 -target x86_64-windows-gnu -Wno-nullability-completeness
.\.test-build\motor_test.exe
.\.venv\Scripts\python -m unittest discover -s tests -p test_*.py -v
node tests/web.test.cjs
.\.venv\Scripts\python tests/mqtt_integration.py
pio run -e esp32dev -e demo
```

Linux ใช้ g++ แทน zig โดยเอา `-target` ออก; ชื่อ executable ให้ตรง tests/test_system.py
integration test สร้าง cert/รหัสผ่านสุ่มชั่วคราวเฉพาะ localhost:18884 และล้างเมื่อจบ
ไม่เชื่อม broker ภายนอกและไม่สั่งบอร์ดของผู้ใช้

สิ่งที่ตรวจแล้ว:
- Python/C++ features, LR และ fallback 203 vectors ต่าง <0.01
- ห้าม feature ใช้ข้อมูลอนาคต และตัด labels ที่อนาคตไม่ครบ
- motor.cpp จริงกับ clock/PWM stub: boot UNKNOWN, dead time, sweep, STOP, timeout, recovery
- logger กันซ้ำหลัง restart/แยกอุปกรณ์/รักษา label ผู้ใช้เมื่อ duplicate เก่ามาถึง
- JavaScript ของหน้าเว็บจริงใน Node VM กับ DOM stub: demo, interlock, offline guard, model schema, READLOG CRC, text escaping
- MQTT TLS/authenticated ในเครื่อง: retained status, ack, chunk duplicates, reconnect, CSV persistence
- Build ESP32 และ firmware demo

ข้อจำกัด: Node VM ไม่ใช่การตรวจภาพหน้าจอ/มือถือ/CDN จริง เครื่องมือ browser ในเซสชันนี้ไม่มี browser ให้ใช้
MQTT device ใน integration เป็น simulator ไม่ใช่เฟิร์มแวร์บนชิป; ยังไม่ทดสอบดับไฟระหว่างเขียน flash,
เซ็นเซอร์จริง, PWM บนขา, WiFi drop บนบอร์ด, Wokwi runtime, หรือ Mosquitto Docker จริง
ไม่ใส่เครื่องหมายผ่านให้ hardware acceptance criteria จนกว่าจะทดสอบบอร์ด

ทดสอบด้วยมือหลัง flash: RETRACT → MODE AUTO; SET interval_s 60 เพื่อเดโม; MEASURE หลายรอบ;
ตัด WiFi และตรวจว่ายังทำงาน/มี log จากนั้นคืน WiFi ตรวจ seq/CRC; สั่ง STOP ระหว่าง sweep;
เปลี่ยน motor_timeout_s เป็น 1 เพื่อจำลอง FAULT_TIMEOUT แล้วคืน 40; ถอดเซ็นเซอร์ตรวจ sensor_err
คืน interval_s=1800 แล้ว SAVE หลังทดสอบ
