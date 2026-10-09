# NimbusRail

คู่มือทำความเข้าใจทั้งระบบ: [Firmware โมเดล Firebase แอพ และวิธีใช้งาน](docs/PROJECT_EXPLAINED_TH.md)

ระบบวัดสภาพอากาศ ประเมินโอกาสฝนตก และควบคุมราวตากผ้าด้วย ESP32 พร้อมแอพ Flutter สำหรับ Android, iOS และเว็บ ใช้ **Firebase Authentication + Realtime Database เป็นช่องทางหลัก**

เริ่มที่ [ตั้งค่า Firebase](docs/FIREBASE_SETUP.md) · [ตั้งค่า Push Notification](docs/PUSH_NOTIFICATIONS.md) · [แผนที่โค้ด](docs/CODE_GUIDE.md)

## สถานะปัจจุบัน — 9 ตุลาคม 2026

- ผู้ใช้ยืนยันว่าเชื่อม Firebase ได้แล้วทั้งแอพและเว็บ โปรเจกต์ `nimbusrail` และบอร์ดที่ใช้งานคือ `sl-4824d3`
- ชุดทดสอบใช้ ESP32 + BME280 + BH1750 ต่อตรง ยังไม่ได้ยืนยันการทำงานบน PCB และยังทดสอบระบบหลักแบบ **ไม่มี servo** ด้วย `firebase_no_servo`
- ภาพหน้าจอล่าสุดแสดง **อุปกรณ์ออฟไลน์ / ข้อมูลเก่า**, ค่าเซ็นเซอร์เป็น `—` และยังประเมินฝนไม่ได้ การเปิดแอพและเชื่อมฐานข้อมูลได้จึงยังไม่ยืนยันว่าบอร์ดส่งค่าที่วัดได้จริงอยู่ในขณะนั้น
- มีโค้ดสำหรับควบคุม servo และรับ Push แล้ว แต่ยังไม่มีผลยืนยันการทดสอบ servo จริงหรือการแสดง Push บนเครื่องจริงในบันทึกการทดสอบนี้

## ระบบทำงานอย่างไร

```text
BME280 + BH1750 → ESP32 → Firebase Auth + Realtime Database ↔ แอพ Flutter
                 │                 │                        Android / iOS / เว็บ
                 ├─ ประเมินฝน      └─ เหตุการณ์ฝน → โปรแกรมส่ง Push → FCM → เครื่องผู้ใช้
                 ├─ เก็บ log ใน LittleFS
                 └─ ควบคุม servo เมื่อเปิดใช้งาน
```

ESP32 อ่านเซ็นเซอร์และประเมินฝนบนบอร์ดเอง จากนั้นส่งสถานะ บันทึก และเหตุการณ์แจ้งเตือนขึ้น Firebase ผ่าน HTTPS แอพอ่านข้อมูลจากฐานข้อมูลและส่งคำสั่งกลับ บอร์ดจึงยังวัดค่า บันทึก และตัดสินใจตามโหมดที่ตั้งไว้ได้เมื่ออินเทอร์เน็ตขาดหาย ส่วนคำสั่งระยะไกลและ Push ต้องมีการเชื่อมต่อ

ใช้การตั้งค่า Firebase **Spark** โดยไม่มี Cloud Functions ในเส้นทางหลัก โปรแกรมส่ง Push อยู่ใน `firebase/push-sender/` และต้องเปิดคอมหรือเซิร์ฟเวอร์ที่รันโปรแกรมนี้ไว้ ดูรายละเอียดใน [คู่มือ Push](docs/PUSH_NOTIFICATIONS.md)

## ความสามารถที่มีในโค้ด

| ส่วน | การทำงาน |
| --- | --- |
| เซ็นเซอร์ | BME280 วัดอุณหภูมิ ความชื้น ความกดอากาศ; BH1750 วัดความเข้มแสง |
| ประเมินฝน | โหลดโมเดลจาก NVS หรือ LittleFS และมีวิธีประเมินสำรองเมื่อใช้โมเดลไม่ได้ |
| ภาพรวมในแอพ | แสดงค่าเซ็นเซอร์ โอกาสฝน สถานะราว โหมด และความสดของข้อมูล |
| บันทึก | เก็บ log บนบอร์ดแบบวนทับ สูงสุด 4,608 รายการ และทยอยส่งขึ้น Firebase เพื่อดูย้อนหลังในแอพ |
| คำสั่งระยะไกล | วัดค่า, AUTO/MANUAL, เก็บผ้า, ตากผ้า และหยุด พร้อมผลตอบรับจากบอร์ด |
| แจ้งเตือน | บันทึกเหตุการณ์เมื่อผลฝนข้ามเกณฑ์ แจ้งในแอพ และส่ง OS Push หลังตั้งค่า FCM/ตัวส่งแล้ว |
| โหมดตัวอย่าง | ทดลองหน้าจอด้วยข้อมูลจำลอง โดยไม่สั่งบอร์ดจริง |

รอบวัดเริ่มต้นคือ **30 นาที** ส่วน Firebase ซิงก์ราว **10 วินาทีหลังคำขอก่อนหน้าจบ** และส่ง log สูงสุด 4 รายการต่อรอบ การซิงก์สถานะไม่ได้แปลว่าวัดค่าใหม่ทุกครั้ง กด **วัดค่า** เพื่อทดสอบโดยไม่ต้องรอรอบถัดไป

แอพถือว่าบอร์ดออนไลน์เมื่อสถานะที่เซิร์ฟเวอร์ได้รับมีอายุน้อยกว่า 30 วินาที และตรวจความสดของผลวัดแยกต่างหาก หากมี `sensor_err=true` หรือค่าที่วัดเป็น `null` อย่าใช้ค่าฝนสำรองเป็นผลประเมินที่เชื่อถือได้

## ปัจจุบันยังใช้ MQTT หรือไม่?

**รุ่น Firebase ที่ใช้กับแอพปัจจุบันไม่ใช้ MQTT และไม่ต้องมี MQTT broker** แต่ยังเก็บโค้ด MQTT เดิมไว้ให้เลือก build ได้

| Environment ใน PlatformIO | ช่องทางเครือข่าย | Servo |
| --- | --- | --- |
| `firebase_no_servo` | Firebase Auth + RTDB ผ่าน HTTPS | ปิด — ใช้กับชุดทดสอบปัจจุบัน |
| `firebase_servo` | Firebase Auth + RTDB ผ่าน HTTPS | เปิด |
| `system_no_servo` | MQTT | ปิด |
| `esp32dev` | MQTT | เปิด |
| `demo` | MQTT พร้อมจำลองค่าเซ็นเซอร์ | ตามรุ่น `esp32dev`; ไม่ใช่โหมดตัวอย่างของแอพ |

ตัวเลือก `SMARTLINE_FIREBASE` ทำให้ใช้ [src/firebase_client.cpp](src/firebase_client.cpp) และตัดโค้ดใน [src/mqtt_client.cpp](src/mqtt_client.cpp) ออกจากการคอมไพล์ ดังนั้นแม้ environment Firebase จะสืบทอดค่าพื้นฐานจาก `esp32dev` ก็ไม่ได้เชื่อม MQTT ควบคู่กัน

ข้อความเริ่มต้นบางบรรทัดใน Serial ยังเขียนว่า `Serial or MQTT` ซึ่งเป็นข้อความเดิมที่ใช้ร่วมกัน ให้ดู environment ที่อัปโหลดและข้อความ `[FIREBASE]` เพื่อระบุช่องทางที่ใช้งาน

`web/index.html`, `broker/` และ `logger/` เป็นชุด MQTT เดิม ส่วนแอพและเว็บ Firebase ปัจจุบันอยู่ใน **`mobile/`**

## ฮาร์ดแวร์และค่าตั้ง

| อุปกรณ์ / สัญญาณ | การตั้งค่าในโค้ด |
| --- | --- |
| ESP32 DevKit | Wi-Fi 2.4 GHz |
| BME280 | I2C address `0x76` หรือ `0x77` |
| BH1750 | I2C address `0x23` |
| I2C ร่วมกัน | SDA GPIO 21, SCL GPIO 22 |
| Servo เมื่อเปิดใช้งาน | สัญญาณ GPIO 13; เก็บ 15°, กาง 165° |
| Rain sensor เพิ่มเติม | GPIO 34; ปัจจุบันปิดด้วย `RAIN_ENABLED=0` |

ดูรายละเอียด [การต่อสาย](docs/wiring.md) และแก้ค่าขา/มุม/รอบวัดได้ที่ [include/config.h](include/config.h) ค่าตั้งที่เคยบันทึกใน NVS อาจแทนค่าเริ่มต้นในไฟล์

ระบบปัจจุบันใช้ servo ไม่มี limit switch ยืนยันตำแหน่ง สถานะราวอ้างอิงมุมที่สั่ง เมื่อพร้อมต่อ servo จริงให้ใช้แหล่งจ่ายที่เหมาะสมและกราวด์ร่วมตามคู่มือต่อสาย แล้วเลือก `firebase_servo` เริ่ม Manual → เก็บผ้าก่อน → ตากผ้า

## รันบอร์ดชุดปัจจุบัน

ใช้ไฟล์ตั้งค่าที่มีอยู่แล้ว:

- `include/secrets.h`: `WIFI_SSID` และ `WIFI_PASSWORD`
- `include/firebase_secrets.h`: Database URL, API key และบัญชี Firebase ของบอร์ด ตาม [คู่มือตั้งค่า](docs/FIREBASE_SETUP.md)
- Firebase: บัญชีเจ้าของและบัญชีบอร์ดแยกกัน พร้อม Rules และการผูก Device ID ตาม [คู่มือตั้งค่า](docs/FIREBASE_SETUP.md)

รัน PowerShell จาก root ของโปรเจกต์ ปิด Serial Monitor เดิมก่อนอัปโหลด และเปลี่ยน `COM3` ให้ตรงพอร์ตจริง:

```powershell
C:\.platformio\penv\Scripts\platformio.exe device list
C:\.platformio\penv\Scripts\platformio.exe run -e firebase_no_servo -t upload --upload-port COM3
C:\.platformio\penv\Scripts\platformio.exe device monitor --port COM3 --baud 115200
```

เมื่อเชื่อมสำเร็จควรเห็น `[WIFI] Connected` พร้อม SSID/IP และ `[FIREBASE] Spark database connected` จากนั้นตรวจว่า `devices/sl-4824d3/telemetry` อัปเดต กดวัดค่าจากแอพ หรือส่ง `MEASURE` พร้อมขึ้นบรรทัดใหม่ทาง Serial แล้วตรวจว่าค่าเซ็นเซอร์เป็นตัวเลขและ `sensor_err=false`

ไม่ต้อง `uploadfs` ซ้ำเพื่ออัปเดตเครือข่ายหรือเฟิร์มแวร์ เพราะจะเขียนทับ LittleFS และ log เดิม สำหรับบอร์ดใหม่ที่ยังไม่มีโมเดล/ข้อมูลเริ่มต้น อ่าน [firmware_data/README.txt](firmware_data/README.txt) ก่อนติดตั้ง filesystem

## เปิดแอพและเว็บ

แอพปัจจุบันเริ่ม Firebase ด้วย `DefaultFirebaseOptions.currentPlatform` จากไฟล์ `mobile/lib/firebase_options.dart` ที่ FlutterFire สร้างไว้ ใช้โปรเจกต์เดียวกับบอร์ด แล้วล็อกอินด้วย **บัญชีเจ้าของ** เพื่อดูอุปกรณ์ที่ผูกไว้

จาก root ของโปรเจกต์ รันเว็บบนคอม:

```powershell
Set-Location mobile
..\.tools\flutter\bin\flutter.bat pub get
..\.tools\flutter\bin\flutter.bat run -d chrome
```

หากต้องการรันบน Android ที่เชื่อมอยู่ ใช้ `flutter devices` เพื่อดู Device ID แล้ว `flutter run -d DEVICE_ID` โดยใช้ Flutter จาก path เดียวกัน การ build/run iOS ต้องทำบน macOS + Xcode พร้อมการตั้งค่า signing

หากย้ายเครื่องและยังไม่มีไฟล์ Firebase ให้ตั้งค่า FlutterFire สำหรับ `android,ios,web` ตามแพลตฟอร์มที่ใช้งาน รายละเอียดบัญชีและการผูกบอร์ดอยู่ใน [คู่มือตั้งค่า](docs/FIREBASE_SETUP.md) ส่วนการ build/deploy เว็บพร้อม Service Worker และ VAPID key อยู่ใน [คู่มือ Push](docs/PUSH_NOTIFICATIONS.md) ค่าเว็บสำหรับ Push ต้องตรงกับโปรเจกต์ใน `firebase_options.dart`

ทดลองหน้าจอโดยไม่ใช้บอร์ด จากโฟลเดอร์ `mobile`:

```powershell
..\.tools\flutter\bin\flutter.bat run -d chrome --dart-define=DEMO=true
```

## Push และการควบคุม servo

- การดูประวัติแจ้งเตือนในแอพกับการรับ Push ขณะปิดหน้าเว็บเป็นคนละส่วน สำหรับ Push ต้องอนุญาตแจ้งเตือน ลงทะเบียน FCM และเปิดโปรแกรมส่งตาม [คู่มือ Push](docs/PUSH_NOTIFICATIONS.md)
- เว็บใช้งานจริงต้องตั้ง HTTPS และ Service Worker; iOS native ต้องตั้ง APNs เพิ่ม การล็อกอินแอพสำเร็จอย่างเดียวยังไม่ยืนยันว่า Push พร้อม
- ใน `firebase_no_servo` กดวัดค่าได้ แต่คำสั่งควบคุม servo ถูกปิด แอพยังปิดปุ่มเคลื่อนราวเมื่อสถานะบอร์ดเก่า/ออฟไลน์ด้วย
- คำสั่งมีอายุประมาณ 25 วินาที บอร์ดไม่ทำคำสั่งหมดอายุ และผลตอบรับ `accepted` หมายถึงรับคำสั่งแล้ว ไม่ได้ยืนยันว่าราวเคลื่อนถึงตำแหน่งจริง
- Cloud logs ไม่มีการลบอัตโนมัติ ควรตรวจการใช้พื้นที่/โควตาของโปรเจกต์ ส่วนกุญแจ service account สำหรับตัวส่ง Push เก็บเฉพาะบนคอมหรือเซิร์ฟเวอร์ ไม่ใส่ในแอพหรือ ESP32

## โครงสร้างและคู่มือ

| ตำแหน่ง | หน้าที่ |
| --- | --- |
| `src/`, `include/` | เฟิร์มแวร์ ESP32, เซ็นเซอร์, โมเดล, servo, log และเครือข่าย |
| `src/diagnostics/` | ทดสอบ Wi-Fi, BME, เซ็นเซอร์ และการทำนายแยกส่วน |
| `mobile/` | แอพ Flutter สำหรับ Android/iOS/เว็บ |
| `firebase/database.rules.json` | สิทธิ์เจ้าของ/บอร์ดและการตรวจข้อมูล/คำสั่ง |
| `firebase/push-sender/` | โปรแกรมส่ง FCM ที่รันนอก Cloud Functions |
| `firebase/tests/` | ทดสอบ Rules, REST และตัวส่ง Push |
| `firebase/functions/` | โค้ดรุ่น Cloud Functions เดิม ไม่อยู่ในรายการ deploy ปัจจุบัน |
| `scripts/` | ตัวช่วยตั้งค่าบอร์ด/เว็บ Push และเครื่องมือทดสอบ |
| `training/`, `firmware_data/` | เตรียม/เทรนโมเดล และไฟล์เริ่มต้นของ LittleFS |
| `web/`, `broker/`, `logger/` | Dashboard, broker และ logger สำหรับ MQTT เดิม |

อ่านโค้ดต่อที่ [CODE_GUIDE](docs/CODE_GUIDE.md) · [ทดสอบเซ็นเซอร์ออฟไลน์](docs/offline-sensor-test.md) · [ทดสอบการทำนาย](docs/offline-prediction-test.md)

เอกสาร [quickstart](docs/quickstart.md), [mqtt-protocol](docs/mqtt-protocol.md) และ [testing](docs/testing.md) อธิบายชุด MQTT/รุ่นเดิม ให้ใช้คู่มือ Firebase และ README นี้เป็นจุดเริ่มสำหรับแอพปัจจุบัน

## การตรวจสอบ

ผลตรวจที่บันทึกไว้ก่อนการปรับ README: Flutter analyze ผ่าน, Flutter tests 6 กรณี, Firebase tests 13 กรณี (Rules 6 + REST 1 + Push จำลอง 6), ตัวช่วย Python 3 กรณี และเคยคอมไพล์ทั้ง `firebase_no_servo` / `firebase_servo` ผ่าน ผลเหล่านี้ไม่แทนการทดสอบเซ็นเซอร์ servo หรือ Push บนเครื่องจริง

รายละเอียดผลตรวจเดิมอยู่ใน [FIREBASE_VALIDATION](docs/FIREBASE_VALIDATION.md) ส่วนสถานะการเชื่อมต่อที่ผู้ใช้ยืนยันล่าสุดอยู่ด้านบนของ README นี้ ข้อความเรื่องงานตั้งค่าที่ยังค้างในรายงานเดิมเป็นสถานะ ณ เวลาที่ตรวจครั้งนั้น

รันชุดตรวจอีกครั้งจาก root (ต้องติดตั้ง dependencies และเครื่องมือ Emulator ตามคู่มือตั้งค่า):

```powershell
npm --prefix firebase test
python -m unittest discover -s tests -p test_spark_setup.py
Set-Location mobile
..\.tools\flutter\bin\flutter.bat analyze
..\.tools\flutter\bin\flutter.bat test
```
