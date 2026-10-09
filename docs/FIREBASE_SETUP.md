# ตั้งค่า NimbusRall: แอพ + Firebase + ESP32

โค้ดแอพอยู่ใน mobile/ ใช้ร่วมกันสำหรับ Android และ iOS เฟิร์มแวร์ Firebase มี environment แยกจาก MQTT เดิม

ระบบที่ทดสอบฮาร์ดแวร์ไว้ก่อนหน้านี้ยังเป็นแบบไม่มี servo การเพิ่ม Firebase รอบนี้ยังไม่ได้เปลี่ยนเฟิร์มแวร์บนบอร์ด และยังไม่ได้ทดสอบ push notification บนโทรศัพท์จริง

## 1. สร้าง Firebase project

ใน Firebase Console:

1. เลือกหรือสร้าง project จด **Project ID**
2. เปิด Authentication → Sign-in method → Email/Password
3. สร้างผู้ใช้ใน Authentication → Users และจด **UID** ของผู้ใช้เจ้าของบอร์ด
4. สร้าง Realtime Database ใน **asia-southeast1 (Singapore)** ด้วย locked mode จด Database URL เช่น https://PROJECT-default-rtdb.asia-southeast1.firebasedatabase.app
5. เปิดแผน Blaze เพื่อ deploy Cloud Functions และตั้งงบประมาณแจ้งเตือนค่าใช้จ่าย

เฟิร์มแวร์ส่ง HTTPS ประมาณทุก 5 วินาทีหลังคำขอก่อนหน้าจบ และลดความถี่เมื่อผิดพลาด การ polling และ trigger มีค่าใช้งานตาม Firebase/Google Cloud อย่าถือว่าเปิดทิ้งไว้แล้วฟรี ไม่จำเป็นต้องเปิด Analytics สำหรับงานนี้

## 2. ติดตั้ง backend

ใช้ Node.js 22 และรันจาก root ของโครงการ:

```powershell
npm --prefix firebase ci
npm --prefix firebase/functions ci
cd firebase
npx firebase login
npx firebase use --add
npx firebase deploy --only database,functions
```

หากเคย login และหมดอายุ ใช้ npx firebase login --reauth เลือก project ที่สร้างในขั้นตอนแรก การ deploy จะสร้างบริการจริงในบัญชีของคุณ

Functions ใช้ region asia-southeast1:

| Function | หน้าที่ |
| --- | --- |
| deviceSync | รับ telemetry/logs และส่งคำสั่งให้ ESP32 ด้วย device credential |
| issueCommand | ตรวจเจ้าของและสถานะอุปกรณ์ก่อนรับคำสั่งจากแอพ |
| detectRain | สร้าง alert เมื่อผลวัดใหม่ข้ามเกณฑ์ฝน |
| sendRainPush | ส่ง FCM จาก alert ที่บันทึกไว้ |

Database rules จำกัดข้อมูลตามเจ้าของ บัญชีแอพอ่าน secret ของบอร์ดไม่ได้และเขียนคำสั่งลง database โดยตรงไม่ได้

## 3. ลงทะเบียน ESP32 ให้เจ้าของ

บอร์ดที่ใช้ทดสอบก่อนหน้านี้คือ sl-4824d3 ตรวจยืนยัน ID จาก Serial Monitor ของบอร์ดที่จะใช้

สคริปต์ provision ต้องมีสิทธิ์ Admin ผ่าน Application Default Credentials (ADC) เช่น gcloud auth application-default login หรือใช้ไฟล์ service account ผ่าน GOOGLE_APPLICATION_CREDENTIALS โดยเก็บไฟล์ไว้นอก repository **ไม่ใส่ service account ลงแอพ/ESP32 และไม่ส่งกุญแจในแชต**

จาก root:

```powershell
npm --prefix firebase/functions run provision -- YOUR_PROJECT_ID https://YOUR_DATABASE_URL OWNER_UID sl-4824d3
python scripts/configure_firebase_device.py firebase/.private/sl-4824d3.json
```

สคริปต์สร้าง secret แบบสุ่ม เก็บเฉพาะ hash บน server และสร้าง include/firebase_secrets.h ในเครื่อง ทั้งสองไฟล์ถูก gitignore ส่วน Wi-Fi ใช้ include/secrets.h เดิม:
WIFI_SSID และ WIFI_PASSWORD ไม่ต้องใส่รหัส Wi-Fi ในแอพ

การ provision ซ้ำด้วยข้อมูลเดิมสามารถทำต่อได้ หากบอร์ดถูกผูกกับบัญชีหรือ secret อื่นอยู่ สคริปต์จะปฏิเสธแทนการเขียนทับ

ไฟล์ firebase_secrets.example.h เป็นตัวอย่างรูปแบบเท่านั้น การใส่ URL อย่างเดียวโดยยังไม่ provision จะเชื่อมต่อไม่ได้

## 4. ทดสอบ Firebase แบบไม่มี servo ก่อน

```powershell
C:\.platformio\penv\Scripts\platformio.exe run -e firebase_no_servo -t upload --upload-port COM3
C:\.platformio\penv\Scripts\platformio.exe device monitor --port COM3 --baud 115200
```

เปลี่ยน COM3 ให้ตรงพอร์ตปัจจุบัน ปิด Serial Monitor อีกตัวก่อน upload ไม่ต้อง uploadfs ซ้ำหาก model และ LittleFS ใช้งานอยู่แล้ว เพราะ uploadfs จะทับข้อมูลเดิม

Serial จะแสดง SSID/IP และสถานะ Firebase ดูในแอพว่าขึ้นออนไลน์และมีค่าเซ็นเซอร์จริง ถ้า HTTP 401 ให้ตรวจ device credential/ID ถ้าเริ่มซิงก์ไม่ได้ให้ตรวจ Wi-Fi, DNS/NTP, endpoint และ certificates โดยไม่ปิดการตรวจ TLS

เฟิร์มแวร์รอเวลา NTP ก่อนเชื่อม HTTPS อ่านเซ็นเซอร์และตัดสินใจในเครื่องได้แม้ cloud หลุด เครือข่าย HTTPS ทำงานใน task แยกจาก motorTick

ใน firebase_no_servo ปุ่ม servo จะถูกปิด แต่ปุ่มวัดค่าตอนนี้ยังใช้ได้

เมื่อพร้อมทดสอบ servo จริง ใช้ environment firebase_servo ต่อ servo ตาม config ของโครงการและใช้แหล่งจ่ายที่รองรับกับกราวด์ร่วม จากนั้นเลือก Manual → เก็บผ้าก่อนเพื่อเริ่มตำแหน่ง → ตากผ้า

สถานะ EXTENDED/RETRACTED อ้างอิงมุม servo ที่สั่งจาก firmware เดิม ไม่มี feedback ยืนยันตำแหน่งจริง Manual จะหมดเวลาตามค่า manual_timeout_s เดิมของบอร์ดแล้วกลับ Auto

## 5. ตั้งค่าแอพ Android/iOS

ติดตั้ง Flutter SDK และ Android Studio/Android SDK สำหรับ Android ส่วน iOS ต้องใช้ macOS + Xcode และตั้ง Signing Team

จากโฟลเดอร์ mobile:

```powershell
dart pub global activate flutterfire_cli
flutterfire configure --project=YOUR_PROJECT_ID --platforms=android,ios
flutter pub get
flutter run
```

บนเครื่องนี้ Flutter อยู่ที่ D:\NimbusRall\.tools\flutter\bin เพิ่ม path นี้ใน PATH หรือใช้ path เต็ม CLI FlutterFire อยู่ใน pub-cache/bin ของผู้ใช้

App identifiers ที่ scaffold ไว้:

- Android: com.nimbusrall.smartline_app
- iOS: com.nimbusrall.smartlineApp

FlutterFire CLI ต้องเพิ่ม native configuration และ Google Services Gradle plugin ให้ Android ตรวจว่ามี mobile/android/app/google-services.json และ mobile/ios/Runner/GoogleService-Info.plist แอพเรียก Firebase.initializeApp() ด้วยค่า native ของแต่ละแพลตฟอร์ม จึงไม่ต้องแก้ main.dart เพื่อใส่ API key

บน iOS ตรวจว่า GoogleService-Info.plist อยู่ใน Runner target และ Signing & Capabilities มี Push Notifications กับ Background Modes → Remote notifications มี entitlement template ให้แล้ว ต้องใช้ provisioning profile ของบัญชี Apple ของคุณ

เปิดแอพ เข้าสู่ระบบด้วยผู้ใช้จากขั้นตอน 1 บอร์ดที่ provision ให้ UID นี้จะปรากฏอัตโนมัติ หากยังไม่มีบอร์ด แอพแสดง UID ให้คัดลอก

สร้างไฟล์ติดตั้งเมื่อ native SDK พร้อม:

```powershell
flutter build apk --release
```

APK อยู่ใน mobile/build/app/outputs/flutter-apk/app-release.apk ค่า signing ปัจจุบันเป็น debug key ของ scaffold เหมาะสำหรับทดสอบในเครื่อง ต้องตั้ง release signing ก่อนส่ง Play Store

บน macOS ใช้ flutter build ipa แล้วจัดการ signing/distribution ผ่าน Xcode/App Store Connect

## 6. แจ้งเตือนฝน

กดกระดิ่งในแอพเพื่อขอสิทธิ์แจ้งเตือนและลงทะเบียน FCM token Android 13+ ต้องอนุญาต notification บน iOS ต้องอัปโหลด APNs authentication key ไปยัง Firebase Console → Project Settings → Cloud Messaging ก่อน

- เปิดแอพอยู่: แสดงข้อความแจ้งเตือนในแอพ
- แอพอยู่ background: FCM notification แสดงผ่านระบบมือถือเมื่อสิทธิ์และ APNs/FCM พร้อม
- แตะ notification: เปิดหน้าแจ้งเตือนของอุปกรณ์
- ระบบบันทึกประวัติ alert ไว้แม้มือถือไม่ได้ออนไลน์

การแจ้งเตือนเกิดเมื่อ sample ใหม่ข้าม retract_threshold ไม่ส่งทุก heartbeat ไม่แจ้งจากข้อมูลเซ็นเซอร์ผิดพลาดหรือ sample เก่ากว่า 5 นาที ถ้าบอร์ดออนไลน์ครั้งแรกแล้วค่าฝนสูงจะสร้าง alert ด้วย ID ตาม seq ผล high ต่อเนื่องไม่สร้างรายการซ้ำ จนค่าลงต่ำแล้วขึ้นใหม่

FCM อาจส่งซ้ำเมื่อบริการ retry และการส่งถึงมือถือขึ้นกับเครือข่าย/ระบบปฏิบัติการ ไม่มีการรับประกันเวลาแน่นอน รายการ alert ใน database ใช้ ID คงที่ต่อ sample

รองรับการส่งไปยัง 10 token ที่ลงทะเบียนล่าสุดต่อบัญชี การออกจากระบบจะถอน token ของโทรศัพท์นี้ก่อน sign-out

## 7. คำสั่งและบันทึก

คำสั่ง MEASURE, EXTEND, RETRACT, STOP, MODE AUTO และ MODE MANUAL เท่านั้นที่อนุญาตจากแอพ

คำสั่งหมดอายุภายใน 30 วินาที บัญชีอื่นหรืออุปกรณ์ที่ไม่ได้ซิงก์เกิน 30 วินาทีจะสั่งไม่ได้ ESP32 จำ command ID ก่อนทำงานเพื่อป้องกันการสั่งซ้ำหลัง reboot ถ้าบอร์ดดับระหว่างดำเนินคำสั่งและไม่ทราบผล จะปฏิเสธการเล่นซ้ำ ให้ตรวจสถานะแล้วสั่งใหม่

accepted หมายถึงเฟิร์มแวร์รับคำสั่งแล้ว ไม่ใช่เคลื่อนที่เสร็จ แอพแสดงสถานะราวแยกต่างหาก STOP ผ่าน cloud มีความหน่วงจาก polling/เครือข่าย จึงไม่ใช่ปุ่มหยุดฉุกเฉินทางไฟฟ้า

LittleFS เก็บบันทึกตามขนาดเดิม 4,608 รายการ ซิงก์สูงสุด 4 รายการต่อรอบ เมื่อเน็ตกลับมาจะไล่ส่งย้อนหลัง หาก offline จนเกินความจุ ข้อมูลเก่าสุดจะถูกแทนที่ตามการทำงานเดิม Cloud logs เก็บต่อเนื่อง ยังไม่มีนโยบายลบอัตโนมัติ ควรกำหนดอายุข้อมูลก่อนใช้งานระยะยาว

READLOG ผ่าน Firebase ไม่ใช้ CSV; แอพอ่าน logs ใน database โดยตรง คำสั่ง DELLOG ทาง Serial ยังจัดการเฉพาะไฟล์ในบอร์ด ไม่ลบประวัติ cloud

## 8. ทดสอบในเครื่อง

```powershell
npm --prefix firebase/functions test
npm --prefix firebase run test:rules
npm --prefix firebase run test:integration
cd mobile
flutter analyze
flutter test
```

Emulator tests ใช้ project จำลอง demo-smartline และต้องมี Java 21 ไม่ deploy หรือสั่งบอร์ดจริง

เอกสารอ้างอิง:
- [ตั้งค่า Firebase กับ Flutter](https://firebase.google.com/docs/flutter/setup)
- [Flutter FCM และการตั้งค่า APNs](https://firebase.google.com/docs/cloud-messaging/flutter/get-started)
- [รับข้อความขณะเปิดแอพและ background](https://firebase.google.com/docs/cloud-messaging/flutter/receive-messages)
- [Realtime Database security rules](https://firebase.google.com/docs/database/security)
- [HTTP Functions](https://firebase.google.com/docs/functions/http-events)
