# NimbusRall — Android และ iOS

แอพ Flutter สำหรับ ESP32 + BME280 + BH1750 ใช้ Firebase Authentication, Realtime Database, Cloud Functions และ Cloud Messaging

- ภาพรวม: อุณหภูมิ ความชื้น ความกดอากาศ แสง โอกาสฝนตก สถานะออนไลน์และราว
- บันทึก: โหลดทีละ 100 รายการ แตะเพื่อดูรายละเอียด
- แจ้งเตือน: ดูประวัติฝนและอนุญาต push notification
- ควบคุม: วัดใหม่ เลือก Auto/Manual เก็บผ้า ตากผ้า หยุด พร้อมสถานะตอบรับ
- โหมดตัวอย่าง: เปิดได้โดยยังไม่ตั้งค่า Firebase ทุกค่าและการเคลื่อนไหวเป็นการจำลอง

## เริ่มต้น

อ่าน [คู่มือตั้งค่า Firebase และ ESP32](../docs/FIREBASE_SETUP.md) ก่อนเชื่อมอุปกรณ์จริง

จากโฟลเดอร์ mobile:

```powershell
..\.tools\flutter\bin\flutter.bat pub get
..\.tools\flutter\bin\flutter.bat test
..\.tools\flutter\bin\flutter.bat run --dart-define=DEMO=true
```

คำสั่ง run ต้องมี Android SDK และมือถือ/อีมูเลเตอร์ Android เชื่อมอยู่ บน macOS ใช้ Flutter ที่ติดตั้งไว้กับ Xcode สำหรับ iOS

แอพใช้ configuration ของ native Firebase: Android google-services.json และ iOS GoogleService-Info.plist ซึ่งสร้างด้วย FlutterFire CLI แอพที่ไม่มี configuration จะเปิดหน้าคำแนะนำและปุ่มโหมดตัวอย่าง

## ไฟล์หลัก

- lib/main.dart: หน้าจอ เข้าสู่ระบบ สถานะสด การแจ้งเตือน และคำสั่ง
- lib/repository.dart: Firebase และแหล่งข้อมูลจำลอง
- lib/models.dart: ตรวจข้อมูลเก่าและแสดงสถานะคำสั่ง
- test/widget_test.dart: ทดสอบหน้าจอ การดูบันทึก และปุ่มที่ห้ามสั่งเมื่อออฟไลน์/servo ปิด

สถานะราวมาจาก firmware เดิมที่สั่งมุม servo จึงเป็นสถานะที่เฟิร์มแวร์คำนวณ ไม่มีเซ็นเซอร์ยืนยันตำแหน่งจริง

## ลองบนคอมพิวเตอร์ก่อนติดตั้ง SDK มือถือ

เปิด http://127.0.0.1:8085 แล้วกด “ลองโหมดตัวอย่าง” หาก server ไม่ทำงาน ให้ทำตาม [ผลทดสอบและคำสั่งเปิดตัวอย่าง](../docs/FIREBASE_VALIDATION.md)

web/ ใช้สำหรับ preview ในเครื่อง การใช้งาน Firebase และ push บนเว็บจริงยังไม่ได้ตั้งค่า ชุดแอพหลักคือ Android/iOS
