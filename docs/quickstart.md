# เริ่มใช้ Smart Clothesline — เซอร์โวจำลองราว

## 1. ลองเว็บโดยไม่ต้องมีบอร์ด

เปิด `web/index.html` แล้วกด **ทดลองโหมดจำลอง** มีข้อมูลย้อนหลัง 48 ชั่วโมงให้ลองกราฟ
กดวัดทันทีเพื่อเปลี่ยนสภาพอากาศ, กาง/เก็บ/หยุด, AUTO/MANUAL, ปรับค่า, label, อ่าน log และดาวน์โหลด CSV ได้
โหมดนี้ไม่ติดต่อ broker และแยกข้อมูลจากอุปกรณ์จริงใน IndexedDB
ไฟล์โมเดลอัปโหลดใน demo เป็นเพียงการจำลอง ack; สภาพอากาศ demo ยังใช้ rule waveform ไม่ใช่ผลโมเดลนั้น

ถ้าเบราว์เซอร์จำกัด IndexedDB บน file:// ให้ใช้:
```powershell
python -m http.server 8765 --bind 127.0.0.1 --directory web
```
เปิด http://127.0.0.1:8765 ในคอม; เว็บ responsive สำหรับมือถือด้วย
ส่วนเชื่อมต่อ MQTT ต้องมีอินเทอร์เน็ตเพื่อโหลด MQTT.js จาก CDN แต่ demo ทำงานได้แม้ CDN โหลดไม่สำเร็จ
host เว็บใช้งานจริงได้ด้วย static hosting/HTTPS โดยอัปโหลด `web/index.html`; ไม่มี backend/build step
อัปเดต MQTT.js โดยเปลี่ยนเวอร์ชันแน่นอนใน script URL แล้วรันชุดทดสอบเว็บและทดสอบ TLS/WSS กับ broker จริงก่อนใช้

## 2. ตั้งค่าบอร์ด

ต่อสายตาม [wiring.md](wiring.md) แล้วสำเนาไฟล์:
```powershell
Copy-Item include/secrets.example.h include/secrets.h
```
กรอก WiFi, MQTT_URI (`mqtts://...:8883`), username/password และ root CA ของ broker
ไฟล์ secrets.h ถูก ignore; ถ้ายังไม่กรอกบอร์ดยังวัด/ควบคุมผ่าน Serial แบบ offline ได้
broker ใช้ [คู่มือ broker](../broker/README.md) หรือ cloud ที่มี TLS/WSS

```powershell
pio run -e esp32dev
pio run -e esp32dev -t upload
# ครั้งแรกเท่านั้น: สร้าง LittleFS พร้อมโมเดลเริ่มต้น
pio run -e esp32dev -t uploadfs
pio device monitor -b 115200
```

**uploadfs แทนที่ข้อมูล LittleFS ทั้งหมด** อย่ารันซ้ำหลังมี log ที่ต้องการเก็บ
แยก `firmware_data/` ไว้เฉพาะไฟล์บอร์ด; CSV ประวัติอยู่ `data/` จึงไม่ถูกอัปโหลดลง flash
โมเดลเริ่มต้นใน firmware_data/model.json ใช้ได้โดยไม่ต้องเปิดเว็บ; โมเดลที่อัปโหลดภายหลังเก็บ NVS และมีลำดับก่อนโมเดลใน LittleFS
หากไม่มีโมเดล/ยังไม่มีประวัติ 3 ชั่วโมง/ยังไม่ sync NTP ระบบใช้ fallback

Serial monitor ต้องส่ง newline (LF). หลังบูตอ่านชื่อ `sl-xxxxxx` แล้วส่ง:
```text
RETRACT
MODE AUTO
```
บอร์ดรอ RETRACT ครั้งแรกเพื่อเริ่มเซอร์โวอย่างชัดเจน ไม่เคลื่อนเองทันทีหลังเปิดไฟ
คำสั่งกาง/เก็บ/หยุดเข้า MANUAL 6 ชั่วโมง; `MODE AUTO` กลับทันที
คำสั่ง Serial ตอบ JSON ack/status เช่นเดียวกับ MQTT แต่ READLOG ส่งไฟล์ผ่าน MQTT เท่านั้น

ทดสอบเฉพาะเซอร์โวโดยยังไม่มีเซ็นเซอร์:
```powershell
pio run -e demo -t upload
```
นี่เป็น firmware ที่จำลองค่าอากาศ แต่ขยับเซอร์โวจริง; ไม่มีการต่อ WiFi อัตโนมัติถ้า secrets ว่าง
`wokwi.toml` ชี้ build demo; `diagram.json` มี ESP32 + servo GPIO13 ตรงกับ PCB

## 3. เชื่อมหน้าเว็บ

กรอก WSS URL, บัญชี `web`, Device ID จาก Serial และ prefix `smartline` แล้วกด Connect
Broker connected และ Device online เป็นคนละสถานะ; ปุ่มควบคุมเปิดเมื่อได้รับสถานะสดแล้ว
อัปโหลด `training/model.json` ในส่วน 05 หากต้องการแทนโมเดลใน NVS
threshold_suggest ไม่ปรับราวเอง: ถ้าจะใช้ 0.27425 ให้ตั้ง retract_threshold นี้และ extend_threshold ที่ต่ำกว่า เช่น 0.20 แล้วกดบันทึก
ค่าเริ่มต้นตามโจทย์ยังเป็น retract=0.50 / extend=0.25

## 4. เก็บข้อมูลต่อเนื่อง

```powershell
python -m venv .venv
.\.venv\Scripts\python -m pip install -r training/requirements.txt -r logger/requirements.txt
Copy-Item logger/.env.example logger/.env
# กรอก host, user=logger, password และ CA ตาม broker
.\.venv\Scripts\python logger/logger.py
```
SQLite ใน `data/device_log.sqlite` เป็นข้อมูลหลัก; `data/device_log.csv` ส่งออกแบบ atomic ทุกครั้งที่มีข้อมูลใหม่
กันซ้ำ `(deviceId,seq)` แม้ปิดแล้วเปิด logger ใหม่ และรองรับการอัปเดต label
Dockerfile และ systemd unit อยู่ใน `logger/`

## 5. เทรนใหม่

ไฟล์ที่พบจริงชื่อ `สภาพอากาศศรีราชา.csv` มี radiation ครบ; คัดลอกไว้ `data/history_hourly.csv` แล้ว
`แสงศรีราชา.csv` มี radiation ซ้ำจึงไม่จำเป็นต้องรวม แต่รองรับ `--light` เพื่อ merge ตาม time แบบ one-to-one
```powershell
.\.venv\Scripts\python training/train.py --history data/history_hourly.csv --out training/model.json
# เมื่อมี log จริงอย่างน้อย 4 สัปดาห์ และติด label เพียงพอ:
.\.venv\Scripts\python training/train.py --history data/history_hourly.csv --device-log data/device_log.csv --out training/model.json
```
logger รองรับหลาย device แต่การ calibrate ต้องเลือก CSV ของ device เดียว
กดฝนตก/ไม่ตกในตารางเพื่อ label **ฝน ณ เวลาวัด**; training สร้างเป้าหมาย “ฝนใน 2 ชั่วโมงข้างหน้า” ให้อีกครั้ง
เติม label จาก Open-Meteo: คัดลอก `training/.env.example` เป็น `.env`, ใส่ LAT/LON แล้วรัน
`python training/label_weather.py --input data/device_log.csv --out data/labeled_log.csv`
API เป็น proxy และไม่เขียนทับ human label; ไม่ส่งพิกัดจนกว่าคุณจะเรียกสคริปต์นี้

ดูรายงาน `training/reports/metrics.json` และ `reliability.png` สูตรใน [features.md](features.md)
ข้อมูลจำลองทดสอบ: `python training/generate_sample_data.py --out data/sample_device_log.csv`
ไม่ใช้ข้อมูลจำลองวัดความแม่นยำจริง และอย่าเอาโมเดลที่ปรับด้วยข้อมูลจำลองไปใช้งานจริง

## 6. สถานะการตรวจสอบ

Build สอง environment, tests สูตร/servo/logger/เว็บ และ MQTT TLS integration ผ่านตาม [testing.md](testing.md)
ยังไม่ได้ flash หรือทดสอบฮาร์ดแวร์จริง และยังไม่ได้ deploy broker/เว็บไปบริการภายนอก
ไม่มีไฟล์ HTML Lab 10A ในต้นทาง จึงสร้างเว็บใหม่ตามโครง README; สำรอง main.cpp เดิมไว้ใน reference
โฟลเดอร์เดิมไม่ใช่ Git repository จึงไม่มี commit แยกขั้นตอน; ไฟล์ทั้งหมดพร้อมตรวจและนำเข้า Git ได้
