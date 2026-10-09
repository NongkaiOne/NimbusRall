# ผลตรวจ Firebase Spark — 9 ตุลาคม 2026

เอกสารนี้แทนผลทดสอบรุ่น Blaze ก่อนหน้า โค้ดปัจจุบันใช้ Firebase Auth + Realtime Database โดยตรง

| รายการ | ผล |
| --- | --- |
| Flutter analyze | ผ่าน ไม่มีปัญหา |
| Flutter model/widget tests | ผ่าน 6 กรณี |
| Spark Database rules tests | ผ่าน 6 กรณี |
| Spark REST integration test | ผ่าน 1 กรณี |
| Push sender/config unit tests | ผ่าน 6 กรณี (FCM จำลอง) |
| ตัวช่วยตั้งค่า Python | ผ่าน 3 กรณี |
| ESP32 firebase_no_servo | คอมไพล์ผ่าน |
| ESP32 firebase_servo | คอมไพล์ผ่าน |

REST integration ทดสอบ sign-in ของบอร์ด การ refresh ID token การส่ง telemetry/logs/alerts การส่งคำสั่งโดยเจ้าของ และผลตอบรับ ACK ผ่าน Emulator จริง โดยไม่มี Functions Emulator

Rules tests ตรวจการแยกเจ้าของ/บอร์ด/ผู้ใช้ภายนอก การห้ามปลอมข้อมูลหรือ ACK การห้ามคำสั่งหมดอายุ อุปกรณ์ออฟไลน์หรือ servo ปิด และ STOP ที่แทนคำสั่งเดิม

Widget/model tests ตรวจหน้าภาพรวม บันทึก คำสั่งจำลอง การปิดปุ่มเมื่อข้อมูลเก่า/servo ปิด และการไม่แจ้งประวัติซ้ำ/ข้อมูลเก่า

## สิ่งที่ยังต้องทำกับโปรเจกต์ของผู้ใช้

ยังไม่ได้ deploy Rules หรือสร้าง/ผูกบัญชีใน Firebase จริง ยังไม่มีผลทดสอบ HTTPS จากบอร์ดไปยังโปรเจกต์ของผู้ใช้ และยังไม่ได้อัปโหลดเฟิร์มแวร์ Spark ลงบอร์ด

ยังไม่ได้สร้าง APK/IPA: เครื่องนี้ยังไม่มี Android SDK และการ build iOS ต้องใช้ macOS/Xcode

เพิ่ม FCM, Web Service Worker และตัวส่งที่คง Spark แล้ว แต่ยังไม่ได้ทดสอบ Push ไปเครื่องจริงหรือ deploy Hosting ต้องตั้ง Web config/VAPID, service account และ APNs สำหรับ iOS native ตาม [คู่มือ Push](PUSH_NOTIFICATIONS.md) ก่อน

## เปิดตัวอย่างและรันทดสอบ

ตัวอย่าง http://127.0.0.1:8085 ใช้ข้อมูลจำลอง เปิด server ใหม่จาก root ได้ด้วย:

```powershell
python -m http.server 8085 --bind 127.0.0.1 --directory mobile/build/web
```

สร้างตัวอย่างใหม่จาก mobile/:

```powershell
..\.tools\flutter\bin\flutter.bat build web --dart-define=DEMO=true --no-web-resources-cdn
```

ทดสอบ Spark จาก root:

```powershell
npm --prefix firebase test
python -m unittest discover -s tests -p test_spark_setup.py
```

อ่าน [คู่มือตั้งค่า Spark](FIREBASE_SETUP.md) เพื่อเชื่อมอุปกรณ์จริง
