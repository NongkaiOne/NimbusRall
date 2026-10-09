# แผนที่โค้ด NimbusRail

คอมเมนต์ภาษาไทยต้นไฟล์บอกภาพรวม และคอมเมนต์หน้าฟังก์ชัน/บล็อกสำคัญบอกหน้าที่และเหตุผล เริ่มอ่านตามลำดับนี้ได้เลย

## 1. ESP32: เริ่มจากการวัดจนถึงการควบคุม

| ไฟล์ | หน้าที่ / จุดสำคัญ |
| --- | --- |
| [platformio.ini](../platformio.ini) | เลือกชุด build; ปัจจุบันทดสอบรวมโดยไม่มี servo ใช้ `firebase_no_servo` |
| [include/config.h](../include/config.h) | GPIO, มุม servo, ค่าเริ่มต้น, เกณฑ์ฝน และ enum สถานะราว |
| [include/app.h](../include/app.h) | ตัวแปรร่วม, Sample, LogRecord และ API ระหว่างโมดูล |
| [src/main.cpp](../src/main.cpp) | `setup` เตรียมระบบ; `loop` ประสานงาน; `processCommand` รับคำสั่ง; `decide` ตัดสินใจ Auto |
| [src/sensors.cpp](../src/sensors.cpp) | BME280 + BH1750 อ่านทุก 200 ms ในหนึ่งรอบ เก็บค่าดี 3 ครั้งภายในไม่เกิน 6 ครั้ง |
| [src/predictor.cpp](../src/predictor.cpp) | ตรวจ/โหลดโมเดล แล้วทำนายแบบ current หรือ history; fallback หากข้อมูลไม่พร้อม |
| [include/features.h](../include/features.h) | สูตร feature และ Logistic Regression ที่ต้องตรงกับ Python |
| [src/motor.cpp](../src/motor.cpp) | ขยับ servo ทีละองศา, STOP, timeout และชุด stub เมื่อปิด servo |
| [src/config_store.cpp](../src/config_store.cpp) | ตรวจช่วงค่าตั้งและเก็บใน NVS |
| [src/logstore.cpp](../src/logstore.cpp) | LittleFS, log วนทับ, CRC, เติมเวลาภายหลัง และส่งออก CSV |
| [src/network_status.cpp](../src/network_status.cpp) | JSON สถานะและ ACK ที่ใช้ร่วมกัน |

ลำดับรอบวัด: `measureStart → sensorsTick → predictionStart/predictionTick → decide → logAppend → publishStatus`
ระหว่างรอแต่ละขั้น `loop` ยังเรียกงานเครือข่าย/servo/คำสั่งต่อเนื่อง

## 2. การเชื่อม Firebase และ Push

| ไฟล์ | หน้าที่ / จุดสำคัญ |
| --- | --- |
| [src/firebase_client.cpp](../src/firebase_client.cpp) | บัญชีบอร์ด → ID token → HTTPS Database; worker ทำเครือข่ายแยกจาก loop; จำ ID ก่อนทำคำสั่ง |
| [include/firebase_roots.h](../include/firebase_roots.h) | ใบรับรองราก HTTPS แบบสาธารณะ |
| [scripts/configure_firebase_device.py](../scripts/configure_firebase_device.py) | สร้าง header ส่วนตัวและ JSON จับคู่เจ้าของ/บอร์ด |
| [scripts/configure_web_push.cjs](../scripts/configure_web_push.cjs) | สร้าง Web config ชุดเดียวสำหรับแอพและ Service Worker |
| [firebase/push-sender/index.cjs](../firebase/push-sender/index.cjs) | โปรแกรมอ่าน alert ล่าสุดทุก 15 วินาที ต้องมีเครื่องรันอยู่ |
| [firebase/push-sender/delivery.cjs](../firebase/push-sender/delivery.cjs) | ตรวจอายุ alert/เจ้าของ ส่ง FCM และจำผลเพื่อลดซ้ำ |

`include/secrets.h` เก็บ Wi-Fi/MQTT และ `include/firebase_secrets.h` เก็บบัญชี Firebase ของบอร์ด เป็นไฟล์ส่วนตัว
ชื่อโปรเจกต์ที่แสดงคือ **NimbusRail** แต่ Firebase Project ID, UID และ application/bundle ID เป็นตัวระบุสำหรับการเชื่อมระบบ ต้องใช้ค่าที่ลงทะเบียนไว้จริง
โฟลเดอร์ทำงานปัจจุบันยังเป็น `D:\NimbusRall` จึงคง path นี้ในคำสั่งคู่มือตามตำแหน่งไฟล์จริง

## 3. แอพ Flutter และเว็บ

| ไฟล์ | หน้าที่ / จุดสำคัญ |
| --- | --- |
| [mobile/lib/main.dart](../mobile/lib/main.dart) | ล็อกอิน ภาพรวม ประวัติ แจ้งเตือน ควบคุม และจัดการ stream เมื่อเปลี่ยนอุปกรณ์ |
| [mobile/lib/repository.dart](../mobile/lib/repository.dart) | FirebaseRepository อ่าน/เขียนจริง; DemoRepository จำลองในเครื่อง |
| [mobile/lib/models.dart](../mobile/lib/models.dart) | ตรวจออนไลน์ ความสด แปลสถานะ และกรอง alert ที่เคยเห็น |
| [mobile/lib/firebase_config.dart](../mobile/lib/firebase_config.dart) | ค่า Firebase ของเว็บจาก build arguments |
| [mobile/lib/push_notifications.dart](../mobile/lib/push_notifications.dart) | ขอสิทธิ์ FCM ลงทะเบียน/เปลี่ยน token และยกเลิกเมื่อ logout |
| [mobile/web/firebase-messaging-sw.js](../mobile/web/firebase-messaging-sw.js) | รับ Push เมื่อเว็บอยู่เบื้องหลังหรือปิดแท็บ |

## 4. ไฟล์ JSON: อ่านคำอธิบายจากตารางนี้

JSON ไม่รองรับคอมเมนต์ จึงเก็บคำอธิบายแยกเพื่อให้ Firebase และเครื่องมืออ่านไฟล์เดิมได้

| ไฟล์ / ส่วน | ความหมาย |
| --- | --- |
| `firebase/database.rules.json` → `access` | ข้อมูลจับคู่ที่ตั้งผ่าน Admin/Console ห้าม client อ่านหรือเขียน |
| `devices/$id` | ให้เจ้าของ/บัญชีบอร์ดที่จับคู่แล้วอ่านข้อมูลอุปกรณ์ |
| `telemetry`, `logs`, `alerts` | บอร์ดเขียนได้เมื่อโครงสร้าง/ช่วงค่าผ่านการตรวจ |
| `control` | เจ้าของส่งคำสั่งที่มีอายุ จำกัดคำสั่งค้างและตรวจออนไลน์/servo |
| `acks` | บอร์ดตอบรับคำสั่งปัจจุบัน เจ้าของปลอม ACK ไม่ได้ |
| `users/$uid/devices` | รายการอุปกรณ์ที่เจ้าของอ่านได้ การจับคู่ทำผ่าน Admin/Console |
| `users/$uid/pushTokens` | token ของมือถือ/เบราว์เซอร์ เฉพาะบัญชีนั้นอ่าน/จัดการได้ตามกฎ |
| `pushDelivery` | ใบรับและ lease ของตัวส่ง Push เฉพาะ Admin เข้าถึงได้ |
| `firebase/firebase.json` | ที่อยู่ Rules, พอร์ต Emulator และไฟล์/เส้นทาง Firebase Hosting |
| `mobile/web/manifest.json` | ชื่อ NimbusRail ไอคอน และโหมดเว็บเมื่อเพิ่มบน Home Screen |
| `firmware_data/model.json`, `training/model.json` | feature, mean, scale, coef, intercept และ metadata ของโมเดล |

## 5. ชุดทดสอบ เครื่องมือ และระบบเดิม

- `src/diagnostics/`: Wi-Fi อย่างเดียว, BME (+Wi-Fi ตาม flag), เซ็นเซอร์ออฟไลน์ และโมเดลออฟไลน์ มีคำอธิบายต้นไฟล์และ setup/loop
- `training/`: สูตร feature, ฝึกโมเดล, เติมป้ายฝน และสร้างข้อมูลตัวอย่าง
- `tests/`, `mobile/test/`, `firebase/tests/`: ทดสอบสูตร คำสั่ง หน้าจอ Database Rules และ Push แบบจำลอง
- `src/mqtt_client.cpp`, `web/index.html`, `logger/logger.py`: ทางเลือก MQTT และเว็บรุ่นเดิม
- `firebase/functions/`: backend Blaze รุ่นเดิม เก็บอ้างอิง ไม่ใช่เส้นทาง Spark ปัจจุบัน
- ไฟล์ที่เครื่องมือสร้าง เช่น lockfile, build output และ native project scaffold ใช้รูปแบบของเครื่องมือเดิม

วิธีตั้งระบบจริง: [Firebase Spark](FIREBASE_SETUP.md) · [Push Notification](PUSH_NOTIFICATIONS.md)
