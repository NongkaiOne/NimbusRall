# ผลตรวจแอพและ Firebase — 8 ตุลาคม 2026

| รายการ | ผล |
| --- | --- |
| Flutter analyze | ผ่าน ไม่มีข้อผิดพลาด/คำเตือน |
| Flutter widget/model tests | ผ่าน 5 กรณี |
| Firebase domain tests | ผ่าน 5 กรณี |
| Realtime Database rules บน Emulator | ผ่าน 3 กรณี |
| Integration: device sync → คำสั่ง → acknowledgement → rain alert | ผ่าน |
| Flutter debug bundle | สร้างสำเร็จ |
| Flutter web demo build | สร้างสำเร็จ |
| เปิดตัวอย่างใน Chrome ขนาด 430 × 932 | แสดง Dashboard ภาษาไทยได้ |
| ESP32 system_no_servo | คอมไพล์ผ่าน |
| ESP32 firebase_no_servo | คอมไพล์ผ่าน |
| ESP32 firebase_servo | คอมไพล์ผ่าน |

ทดสอบ integration กับ Auth/Database/Functions Emulator: ปฏิเสธ device credential ที่ไม่ถูกต้อง, จำกัดคำสั่งตามเจ้าของ, ปฏิเสธคำสั่ง servo เมื่อปิดไว้, ส่งคำสั่งที่ค้างอยู่, เก็บ acknowledgement แบบซ้ำแล้วผลเดิม, บันทึก log และสร้าง alert ครั้งเดียวเมื่อค่าฝนข้ามเกณฑ์

Firebase Database Emulator รองรับ v2 trigger ใน us-central1 เท่านั้น โค้ดเลือก region นี้เฉพาะเมื่อ FUNCTIONS_EMULATOR=true ส่วนบริการจริงใช้ asia-southeast1 ตามเดิม

หากเครื่องโหลด Functions ครั้งแรกไม่ทัน ให้ตั้ง $env:FUNCTIONS_DISCOVERY_TIMEOUT='60' ก่อนรัน npm --prefix firebase run test:integration

## ตัวอย่างที่เปิดในเครื่อง

เปิด http://127.0.0.1:8085 แล้วกด “ลองโหมดตัวอย่าง” ทุกค่าและคำสั่งเป็นข้อมูลจำลอง

หาก server ถูกปิด เปิดใหม่จาก root:

```powershell
python -m http.server 8085 --bind 127.0.0.1 --directory mobile/build/web
```

หากต้องสร้างตัวอย่างใหม่:

```powershell
cd mobile
..\.tools\flutter\bin\flutter.bat build web --dart-define=DEMO=true --no-web-resources-cdn
```

## ส่วนที่ต้องตั้งค่าก่อนทดสอบจริง

ผู้ใช้ยังไม่มี Firebase project จึงยังไม่ได้ deploy, ลงทะเบียน device credential หรือทดสอบ HTTPS จาก ESP32 ไปยัง Firebase จริง รวมถึงยังไม่ได้ทดสอบ FCM/APNs บนมือถือ

เครื่องนี้ยังไม่มี Android SDK จึงยังไม่ได้สร้าง APK และการ build/sign iOS ต้องใช้ macOS/Xcode ยังไม่มีไฟล์ติดตั้งมือถือจากการทดสอบรอบนี้

เฟิร์มแวร์บนบอร์ดยังเป็นรุ่นที่ใช้งานอยู่ก่อนเริ่มงานแอพ ไม่ได้อัปโหลด Firebase หรือเปิด servo ลงบอร์ดในรอบนี้

เริ่มตั้งค่าตาม [คู่มือ Firebase](FIREBASE_SETUP.md)
