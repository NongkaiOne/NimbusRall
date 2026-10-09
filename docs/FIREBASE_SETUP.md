# ตั้งค่า NimbusRail ด้วย Firebase Spark

ชื่อที่แสดงของโปรเจกต์คือ **NimbusRail** ส่วน **Project ID** ให้คัดลอกจาก Firebase Console ตามจริง ไม่จำเป็นต้องตรงกับชื่อแสดง

อัปเดต 9 ตุลาคม 2026: โค้ดปัจจุบันใช้ **Spark** ส่งข้อมูลและคำสั่งตรงกับ Realtime Database
**ไม่ต้องเปิด Blaze ไม่ต้องผูกบัตร และไม่ต้อง deploy Cloud Functions**

```text
ESP32 ── Firebase Auth + HTTPS ── Realtime Database ── แอพ Flutter
                               สถานะ / logs / alerts / control / acks
```

แอพดูเซ็นเซอร์ ผลทำนาย ประวัติ และสั่ง Manual ได้ รวมถึง **Push เมื่อปิดแท็บเว็บหรือแอพอยู่เบื้องหลัง** หลังตั้งค่า FCM และเปิดตัวส่งบนคอมหรือเซิร์ฟเวอร์ ดู [คู่มือ Push Notification](PUSH_NOTIFICATIONS.md) หากยังไม่ตั้งค่า Push จะมีข้อความเตือนขณะเปิดแอพและเก็บประวัติตามเดิม

Spark มีโควตาฟรี ไม่ใช่ไม่จำกัด ตรวจปริมาณข้อมูลที่แท็บ Usage ของ Firebase ตาม [ราคาทางการ](https://firebase.google.com/pricing)

## 1. เลือกโปรเจกต์และคงแผน Spark

เปิด [Firebase Console](https://console.firebase.google.com/) → เลือกโปรเจกต์ NimbusRail ที่สร้างไว้

ถ้ายังไม่มี ให้กดสร้างโปรเจกต์ ตั้งชื่อ NimbusRail และข้าม Google Analytics/Gemini ได้ จด **Project ID** จาก ⚙️ → Project settings → General

คงแผน **Spark** ไว้ ข้ามขั้นตอน Upgrade/Blaze ในคู่มือรุ่นก่อนทั้งหมด

## 2. เปิด Authentication และสร้างผู้ใช้ 2 บัญชี

ไปที่ **Build → Authentication → Get started → Sign-in method** แล้วเปิด **Email/Password**

ที่แท็บ **Users → Add user** สร้าง:

| บัญชี | ใช้ที่ไหน | ข้อมูลที่ต้องจด |
| --- | --- | --- |
| เจ้าของ เช่นอีเมลของคุณ | เข้าสู่ระบบในแอพ | อีเมล รหัสผ่าน และ UID |
| บอร์ด เช่น esp32@example.com | ให้ ESP32 ส่งข้อมูลและอ่านคำสั่ง | อีเมล รหัสผ่าน และ UID |

**UID สองบัญชีต้องต่างกัน** บอร์ดใช้บัญชีของตนเอง ไม่ใส่รหัสผ่านเจ้าของหรือ service account ลงใน ESP32
จด UID จากแถวผู้ใช้ ไม่ใช่ Project ID

## 3. สร้าง Realtime Database

ไปที่ **Build → Realtime Database → Create database**

1. เลือก **Singapore / asia-southeast1**
2. เลือก **Start in locked mode**
3. คัดลอก Database URL ด้านบนแท็บ Data เช่น:

```text
https://YOUR_PROJECT-default-rtdb.asia-southeast1.firebasedatabase.app
```

ใช้ **Realtime Database** สำหรับโค้ดนี้

ที่แท็บ **Rules** เปิดไฟล์ [firebase/database.rules.json](../firebase/database.rules.json) แล้วคัดลอกเนื้อหาทั้งไฟล์ไปแทน rules ใน Console จากนั้นกด **Publish**

กฎนี้อนุญาตให้บอร์ดเขียนข้อมูลเฉพาะของตน และให้เจ้าของส่งคำสั่งเฉพาะอุปกรณ์ของตน ไม่เปิดอ่าน/เขียนสาธารณะ

หากต้องการใช้ CLI แทนการวาง Rules ใน Console รันจาก root:

```powershell
npm --prefix firebase ci
cd firebase
npx firebase login
npx firebase deploy --only database --project YOUR_PROJECT_ID
```

คำสั่งนี้ deploy เฉพาะ Rules ใช้ Spark ได้ ถ้า login เดิมหมดอายุ ใช้ npx firebase login --reauth

## 4. เตรียมค่า Web API Key และ Device ID

**Web API Key:** ⚙️ Project settings → General → Web API Key

หากไม่เห็น ให้เพิ่มแอพเว็บด้วยไอคอน **</>** ตั้งชื่อเช่น NimbusRail-setup แล้วดูค่า apiKey ใน firebaseConfig ที่แสดง ไม่ต้องเปิด Hosting
API Key ตัวนี้เป็นค่า configuration สำหรับ Firebase Auth ไม่ใช่ service account private key

**Device ID:** ดู Serial Monitor ของ ESP32 เช่น:

```text
Device: sl-4824d3
```

บอร์ดที่ทดสอบก่อนหน้านี้คือ sl-4824d3 แต่ให้ใช้ค่าจากบอร์ดของคุณจริง

## 5. สร้างไฟล์ตั้งค่าบอร์ดด้วยตัวช่วย

เปิด Terminal ใน VS Code ที่ D:\NimbusRall แล้วรัน:

```powershell
python scripts/configure_firebase_device.py
```

กรอกตามลำดับ:

1. Database URL
2. Web API Key
3. UID เจ้าของ
4. UID บอร์ด
5. อีเมลบัญชีบอร์ด
6. รหัสผ่านบัญชีบอร์ด — ตอนพิมพ์จะไม่แสดงตัวอักษรบนจอ
7. Device ID — กด Enter ใช้ sl-4824d3 เฉพาะถ้าตรงกับบอร์ดนี้

ตัวช่วยสร้าง:

- include/firebase_secrets.h — สำหรับคอมไพล์บอร์ด ถูก Git ignore
- firebase/.private/pairing-sl-4824d3.json — สำหรับผูกเจ้าของกับบอร์ด
- firebase/.private/access-sl-4824d3.json และ owner-sl-4824d3.json — สำหรับเพิ่มในฐานข้อมูลที่มีข้อมูลเดิม

หากมี firebase_secrets.h จากครั้งก่อนและต้องการแทนที่ ใช้:

```powershell
python scripts/configure_firebase_device.py --replace
```

หากต้องการกรอกไฟล์เอง ให้คัดลอก [include/firebase_secrets.example.h](../include/firebase_secrets.example.h) เป็น include/firebase_secrets.h แล้วใส่ค่าครบ 5 ตัว ส่วน Wi-Fi ยังใช้ WIFI_SSID และ WIFI_PASSWORD ใน include/secrets.h เดิม

ห้ามส่งรหัสผ่านหรือไฟล์ firebase_secrets.h ลงแชตหรือ Git

## 6. ผูกบัญชีเจ้าของกับบอร์ดใน Database

สำหรับ **ฐานข้อมูลใหม่ที่ยังว่าง**:

1. ไป Realtime Database → Data
2. เลือก root ของฐานข้อมูล
3. เปิดเมนู ⋮ → **Import JSON**
4. เลือก firebase/.private/pairing-sl-4824d3.json ที่ตัวช่วยสร้าง
5. ยืนยันการนำเข้า

**Import JSON จะแทนข้อมูลในตำแหน่งที่เลือก** ถ้ามีข้อมูลเดิมอยู่ ห้าม import pairing ทั้งไฟล์ที่ root ให้เพิ่มเฉพาะสองตำแหน่งนี้แทน:

| ตำแหน่งใน Data | ไฟล์นำเข้า |
| --- | --- |
| access/sl-4824d3 | access-sl-4824d3.json |
| users/OWNER_UID/devices/sl-4824d3 | owner-sl-4824d3.json |

เปลี่ยน OWNER_UID และ Device ID ให้ตรงข้อมูลจริง หากตำแหน่งยังไม่มี ใช้ปุ่ม + เพิ่ม child ตามลำดับได้ หรือเพิ่มค่าด้วยมือดังนี้:

```json
{
  "access": {
    "sl-4824d3": {
      "ownerUid": "UID_เจ้าของ",
      "deviceUid": "UID_บอร์ด"
    }
  },
  "users": {
    "UID_เจ้าของ": {
      "devices": {
        "sl-4824d3": { "name": "NimbusRail" }
      }
    }
  }
}
```

access ใช้ให้ Rules ตรวจสิทธิ์ แอพและบอร์ดไม่สามารถแก้เจ้าของเองได้ ให้ผู้ดูแลจัดการผ่าน Console

## 7. อัปโหลดบอร์ดแบบไม่มี servo ก่อน

ปิด Serial Monitor ที่จับพอร์ตอยู่ แล้วรันจาก root:

```powershell
C:\.platformio\penv\Scripts\platformio.exe run -e firebase_no_servo -t upload --upload-port COM3
C:\.platformio\penv\Scripts\platformio.exe device monitor --port COM3 --baud 115200
```

เปลี่ยน COM3 ตามพอร์ตที่ Windows เห็น รอ Wi-Fi และเวลา NTP แล้วควรเห็น:

```text
[FIREBASE] Spark | Device sl-4824d3 | Auth + RTDB ready
[WIFI] Connected | SSID: ... | IP: ...
[FIREBASE] Spark database connected
```

ตรวจใน Firebase Data ว่ามี devices/sl-4824d3/telemetry และ logs บอร์ดส่งข้อมูลราวทุก 10 วินาทีหลังคำขอก่อนหน้าจบ และทยอยส่ง logs สูงสุด 4 รายการต่อรอบ

ไม่ต้อง uploadfs ซ้ำหาก LittleFS และโมเดลใช้งานอยู่ เพราะ uploadfs จะเขียนทับ filesystem เดิม

ใน firebase_no_servo แอพสั่งวัดค่าได้ แต่ปุ่ม servo ถูกปิด เมื่อพร้อมทดสอบ servo จริงจึงเลือก firebase_servo และเริ่ม Manual → เก็บผ้าก่อน → ตากผ้า ตามการเริ่มตำแหน่งของเฟิร์มแวร์เดิม

สถานะราวคำนวณจากมุม servo ที่สั่ง ไม่มีเซ็นเซอร์ยืนยันตำแหน่งจริง และคำสั่งผ่านเครือข่ายมีความหน่วง

## 8. ตั้งค่าแอพ Android/iOS

ติดตั้ง Android SDK/Android Studio สำหรับ Android ส่วน iOS ใช้ macOS + Xcode พร้อม Signing Team

แอพ Android ใช้ application ID: com.nimbusrall.smartline_app
แอพ iOS ใช้ bundle ID: com.nimbusrall.smartlineApp

จากโฟลเดอร์ mobile:

```powershell
dart pub global activate flutterfire_cli
flutterfire configure --project=YOUR_PROJECT_ID --platforms=android,ios
flutter pub get
flutter run
```

Flutter บนเครื่องนี้อยู่ที่ D:\NimbusRall\.tools\flutter\bin เพิ่ม path นี้ใน PATH ก่อนใช้คำสั่ง flutter/dart หรือใช้ path เต็ม

ตรวจให้มี native configuration:

- Android: mobile/android/app/google-services.json และ Google Services Gradle plugin ที่ FlutterFire CLI ตั้งให้
- iOS: mobile/ios/Runner/GoogleService-Info.plist อยู่ใน Runner target

แอพใช้ Firebase.initializeApp() จากค่า native จึงไม่ต้องใส่ API key ใน main.dart ส่วน Push บน iOS native ต้องตั้ง APNs เพิ่มตาม [คู่มือ Push](PUSH_NOTIFICATIONS.md)

เข้าสู่แอพด้วย **บัญชีเจ้าของ** บอร์ดที่ผูก UID นี้จะปรากฏ ถ้าไม่ปรากฏ ให้ตรวจ users/OWNER_UID/devices และ UID ของผู้ใช้ที่ล็อกอินอยู่

ตัวอย่างที่ http://127.0.0.1:8085 ยังเป็นโหมดจำลองสำหรับลองหน้าจอ ไม่ได้เชื่อม Firebase project ของคุณ

## 9. พฤติกรรมคำสั่งและแจ้งเตือน

- เจ้าของเขียน control ผ่าน SDK; Rules ตรวจเจ้าของ อุปกรณ์ออนไลน์ภายใน 30 วินาที คำสั่งที่อนุญาต และสถานะเปิด servo
- คำสั่งปกติมีอายุประมาณ 25 วินาที Rules จำกัดไม่เกิน 30 วินาที บอร์ดไม่ทำคำสั่งหมดอายุและจำ ID ก่อนทำงาน เพื่อไม่ทำซ้ำหลัง reboot
- มีคำสั่งค้างได้ครั้งเดียว STOP สามารถแทนคำสั่งเดิมได้ เฟิร์มแวร์เขียนผลตอบรับไว้ที่ acks/COMMAND_ID
- accepted คือรับคำสั่งแล้ว ให้ดูสถานะราวประกอบ ไม่ใช่การยืนยันตำแหน่งทางกายภาพ
- บอร์ดสร้าง alerts จากผลใน logs เมื่อค่าฝนข้ามเกณฑ์ปัจจุบัน ใช้ ID ตาม seq เพื่อให้การส่งซ้ำไม่เพิ่มรายการซ้ำ
- เมื่อเน็ตกลับมา logs/alerts ย้อนหลังจะซิงก์ตามข้อมูลที่ยังอยู่ในบอร์ด แอพไม่แจ้งเตือนซ้ำจากประวัติครั้งแรก และไม่แสดงข้อความเตือนจากเหตุการณ์เก่ากว่า 5 นาที
- รับ OS Push เมื่อปิดแท็บ/อยู่เบื้องหลังได้หลังตั้งค่า FCM และเปิดตัวส่งตาม [คู่มือ Push](PUSH_NOTIFICATIONS.md) ยังคง Spark ได้
- LittleFS เก็บสูงสุด 4,608 รายการตามค่าเดิม Cloud logs ไม่มีการลบอัตโนมัติ ให้ตรวจโควตา Spark ใน Console
- MQTT เดิมยังอยู่ใน environment อื่น ส่วน firebase_no_servo / firebase_servo ใช้ Spark แล้ว

## แก้ปัญหาเบื้องต้น

| อาการ | ตรวจอะไร |
| --- | --- |
| Auth failed HTTP 400 | เปิด Email/Password แล้วหรือยัง; อีเมล รหัสผ่าน และ Web API Key ของบอร์ดถูกต้องหรือไม่ |
| Auth response or device UID mismatch | FIREBASE_DEVICE_UID ต้องตรงกับบัญชีที่ใช้ล็อกอินบอร์ด |
| Sync failed HTTP 401 | ตรวจ Rules, access/DEVICE_ID, deviceUid และ Database URL |
| ยังไม่มีอุปกรณ์ในแอพ | ตรวจ users/OWNER_UID/devices และล็อกอินบัญชีเจ้าของ |
| ปุ่ม servo กดไม่ได้ | firebase_no_servo ปิด servo หรือสถานะบอร์ดเก่ากว่า 30 วินาที |
| มีบันทึกแต่ไม่เด้งเตือน | ประวัติครั้งแรก/ข้อมูลเก่าไม่แจ้งซ้ำ; หากใช้ Push ตรวจตัวส่งและสิทธิ์ตามคู่มือ Push |
| แอพเปิดหน้าแนะนำแทนล็อกอิน | native Firebase configuration ยังไม่ครบ |

## ทดสอบโค้ดในเครื่อง

```powershell
npm --prefix firebase ci
npm --prefix firebase test
python -m unittest discover -s tests -p test_spark_setup.py
cd mobile
flutter analyze
flutter test
```

ชุดทดสอบใช้ Auth + Database Emulator, project จำลอง demo-smartline และ Java 21 ไม่ต้องเปิด billing

โค้ด firebase/functions/ เป็นรุ่น Blaze เก่าที่เก็บไว้อ้างอิง ไม่ได้อยู่ใน firebase.json และแอพ/บอร์ดรุ่นปัจจุบันไม่เรียกใช้งาน

อ้างอิง: [Firebase Pricing](https://firebase.google.com/pricing), [Auth REST](https://firebase.google.com/docs/reference/rest/auth), [Database REST authentication](https://firebase.google.com/docs/database/rest/auth), [ตั้งค่า Flutter](https://firebase.google.com/docs/flutter/setup)
