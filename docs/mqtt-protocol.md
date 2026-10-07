# MQTT protocol v1 — servo edition

Base: `smartline/sl-xxxxxx`; อ่าน Device ID จาก Serial 115200 หลังบูต
ESP32 ใช้ ESP-IDF ESP-MQTT ที่มากับ Arduino-ESP32 เพื่อรองรับ QoS1/PUBACK และ network task แยก
TLS ตรวจ CA และ hostname, clean session, keepalive 45 s, retry WiFi/MQTT แบบ 2–60 s

| Suffix | QoS | Retain | Payload |
|---|---:|---|---|
| availability | 1 | yes | online / offline (LWT) |
| status | 0 | yes | JSON ตาม README + sensor_err/demo |
| config | 1 | yes | interval_s, retract_threshold, extend_threshold, daylight_lux_min, manual_timeout_s, motor_timeout_s, heartbeat_s |
| cmd | 1 | **no** | คำสั่ง ASCII |
| ack | 1 | no | {cmd,ok,msg} |
| log/record | 1 | no | CSV หนึ่งบรรทัด ลงท้าย LF |
| log/file | 1 | no | #BEGIN / #CHUNK n / #END |
| model/set | 1 | **no** | JSON <=4096 bytes; ack.cmd = MODEL |

รองรับ EXTEND, RETRACT, STOP, MODE AUTO, MODE MANUAL, MEASURE, SET key value, SAVE,
READLOG [since_seq], DELLOG, LABEL seq 0|1, REBOOT ผ่าน MQTT และ Serial แบบ LF
ค่าตัวพิมพ์ต้องตรง; SET ยังไม่ถาวรจนกว่าจะ SAVE; MODE/ตำแหน่งไม่ restore หลังบูต
MEASURE ไม่เลื่อนรอบวัดปกติ; เปลี่ยน interval_s จะเริ่มนับรอบจากเวลาที่ตั้งใหม่
manual command เข้า MANUAL แล้วกลับ AUTO เมื่อครบ manual_timeout_s
retained cmd/model จะถูกปฏิเสธ ห้าม retain คำสั่งจาก client ใด ๆ

CSV header (ลำดับตายตัว):
```
seq,epoch,temp_c,hum_pct,press_hpa,lux,p_rain,line,action,flags,rain_label,rain_now,uptime_s
```
flags bit0 sensor_err, bit1 time_unsynced, bit2 model_fallback
sensor_err ใช้ช่อง sensor ว่าง; rain_label/rain_now=-1 คือยังไม่ทราบ
เก็บ 4,608 records (~96 วันเมื่อทุก 30 นาที); ลด interval หรือ MEASURE ถี่จะลดจำนวนวันที่เก็บได้
สอง generation ต่อ slot พร้อม CRC16 record + CRC32 slot; บูต scan คืนสถานะ ห้าม format อัตโนมัติเมื่อ mount ไม่ผ่าน
seq ไม่ reset เมื่อ DELLOG; ถ้า erase NVS/factory reset ให้ใช้ Device ID/prefix ใหม่เพื่อไม่ชนข้อมูลเก่า
NTP backfill ทำเฉพาะ records ของ boot ปัจจุบันที่ยังมี uptime anchor เชื่อถือได้; boot เก่าไม่เดาเวลา

READLOG ส่ง snapshot ถึง seq ล่าสุดขณะเริ่ม คำสั่ง LABEL/DELLOG ระหว่างส่งจะตอบ busy
แต่ละ chunk ~1 KB แบ่งตรง LF; header อยู่ chunk 0; CRC32 IEEE คิดจาก header + CSV ทุกบรรทัดรวม LF
ไม่รวม #BEGIN/#CHUNK/#END ใน CRC ผู้รับเก็บ chunk ด้วยเลข index เพื่อกัน QoS1 ซ้ำ
เมื่อหลุดให้เริ่ม READLOG ใหม่; เว็บไม่ยอมรวมการส่งเก่ากับ session ใหม่
Backlog ส่งทีละ record <=5/s เลื่อน last_pub_seq ใน NVS เฉพาะได้รับ PUBACK
ไฟดับหลัง PUBACK ก่อนบันทึก cursor อาจทำให้ส่งซ้ำ จึง dedup ด้วย (deviceId,seq) ที่ receiver
ข้อมูลเก่ากว่าความจุ ring จะถูกทับเมื่อ offline นานกว่า retention

เว็บใช้ MQTT.js 5.14.1 ผ่าน CDN แบบ pin; reconnect สร้าง client ใหม่ทุก 3 วินาที
จงใจใช้ reconnectPeriod=0 แล้ว reconnect เอง เพื่อทิ้งคำสั่งที่ยังไม่ยืนยันเมื่อ network ขาด
ไม่ส่งคำสั่งเก่ากลับไปเมื่ออุปกรณ์ออนไลน์ และมี queue รอ ack ทีละคำสั่ง/timeout 10 s
สถานะเก่ากว่า max(150 s, 2.5×heartbeat) ปิดปุ่มควบคุม; เปิดหน้าไม่ auto-connect
IndexedDB แยก base topic; Password เก็บ localStorage เฉพาะผู้ใช้ติ๊กจำรหัสผ่าน

อ้างอิง API: [ESP-MQTT 4.4](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/protocols/mqtt.html),
[MQTT.js](https://github.com/mqttjs/MQTT.js)
