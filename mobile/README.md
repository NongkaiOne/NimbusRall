# NimbusRail — Firebase Spark

แอพ Flutter สำหรับ Android/iOS/เว็บ เชื่อม ESP32 + BME280 + BH1750 ผ่าน Firebase Auth และ Realtime Database โดยตรง ไม่ใช้ Cloud Functions และไม่ต้องเปิด Blaze

- ภาพรวม: เซ็นเซอร์ โอกาสฝนตก สถานะออนไลน์และราว
- บันทึก: โหลดทีละ 100 รายการและแตะดูรายละเอียด
- ควบคุม: วัดค่า Auto/Manual เก็บผ้า ตากผ้า หยุด พร้อมผลตอบรับจากบอร์ด
- แจ้งเตือน: เหตุการณ์ฝนใหม่ในแอพ, Push ขณะอยู่เบื้องหลังเมื่อเปิดใช้ FCM และประวัติย้อนหลัง
- โหมดตัวอย่าง: ทุกค่าและคำสั่งเป็นข้อมูลจำลอง ไม่มีการสั่งบอร์ดจริง

**รองรับ Push ผ่าน FCM เมื่อปิดแท็บ/อยู่เบื้องหลัง** ต้องลงทะเบียนรับและเปิดโปรแกรมตัวส่งบนคอมหรือเซิร์ฟเวอร์ ดู [คู่มือ Push](../docs/PUSH_NOTIFICATIONS.md) ยังใช้ Spark ได้

เริ่มตั้งค่าตาม [คู่มือ Spark ทีละขั้น](../docs/FIREBASE_SETUP.md)

## ลองตัวอย่างบนคอมพิวเตอร์

จาก mobile/:

```powershell
..\.tools\flutter\bin\flutter.bat pub get
..\.tools\flutter\bin\flutter.bat run -d chrome --dart-define=DEMO=true
```

หากเปิด server ตัวอย่างไว้แล้ว เข้าที่ http://127.0.0.1:8085 และกด “ลองโหมดตัวอย่าง”

## ทดสอบ

```powershell
..\.tools\flutter\bin\flutter.bat analyze
..\.tools\flutter\bin\flutter.bat test
```

main.dart: หน้าจอและแจ้งเตือนขณะเปิดแอพ
repository.dart: อ่าน/เขียน Realtime Database และแหล่งข้อมูลจำลอง
models.dart: ตรวจความสดของสถานะและป้องกันการแจ้งเตือนประวัติซ้ำ

การติดตั้ง Android ต้องมี Android SDK และ iOS ต้องใช้ macOS/Xcode สถานะราวอ้างอิงมุม servo ที่สั่ง ไม่มี feedback วัดตำแหน่งจริง
