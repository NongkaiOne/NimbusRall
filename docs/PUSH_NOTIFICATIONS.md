# Push Notification บนมือถือและคอม โดยคง Firebase Spark

เพิ่มโค้ดแล้ว 9 ตุลาคม 2026: ใช้ Firebase Cloud Messaging (FCM) และตัวส่ง Node.js บนคอมหรือเซิร์ฟเวอร์ที่เปิดตลอด ไม่ต้อง deploy Cloud Functions หรือเปลี่ยนเป็น Blaze

```text
ESP32 → Realtime Database / alerts → โปรแกรม push-sender → FCM
                                                        ├─ Android / iOS app
                                                        └─ Web Service Worker → แจ้งเตือนของระบบ
```

**ปิดหน้าเว็บได้ แต่เครื่องที่รัน push-sender ต้องยังทำงานและมีอินเทอร์เน็ต** ถ้าเครื่องตัวส่งปิด หลับ หรือโปรแกรมหยุด จะไม่มีการส่งใหม่ จนกว่าจะเปิดกลับมา ตัวส่งข้ามผลฝนที่เก่าเกิน 5 นาที
FCM ไม่มีค่าบริการส่งข้อความ ส่วน Database/Hosting ยังมีโควตา Spark และเครื่องตัวส่งมีค่าไฟ/อินเทอร์เน็ตตามการใช้งาน [ราคา Firebase](https://firebase.google.com/pricing)

## 1. ทำการเชื่อมข้อมูลให้เสร็จก่อน

ทำตาม [FIREBASE_SETUP.md](FIREBASE_SETUP.md) จนแอพล็อกอินด้วยบัญชีเจ้าของและเห็นอุปกรณ์จริง แล้ว Publish ไฟล์ `firebase/database.rules.json` รุ่นล่าสุดอีกครั้ง เพราะเพิ่ม `users/OWNER_UID/pushTokens` สำหรับเครื่องที่รับแจ้งเตือน

การเพิ่ม Push รอบนี้ไม่ต้องเปลี่ยนเฟิร์มแวร์ ESP32 ถ้าใช้เฟิร์มแวร์ Spark ล่าสุดอยู่แล้ว และไม่ต้องเปิด servo

## 2. ตั้งค่าเว็บสำหรับมือถือและคอม

1. Firebase Console → ⚙ Project settings → General → Your apps → เพิ่มแอพ **Web (`</>`)** หากยังไม่มี แล้วคัดลอกค่าจาก `firebaseConfig`
2. Project settings → **Cloud Messaging → Web Push certificates → Generate key pair** แล้วคัดลอก **Public key**
3. จาก root โปรเจกต์ คัดลอกไฟล์ตัวอย่าง:

```powershell
New-Item -ItemType Directory -Force firebase/.private
Copy-Item firebase/web-push.example.json firebase/.private/web-push.json
```

เปิด `firebase/.private/web-push.json` ใส่ `apiKey`, `appId`, `messagingSenderId`, `projectId`, `authDomain`, `databaseURL` จากโปรเจกต์เดียวกัน และ `vapidKey` เป็น Public key ข้อ 2

ค่า Web config/Public VAPID key เป็นค่าที่เว็บต้องใช้ ไม่ใช่ service-account private key ห้ามนำ private key มาใส่ไฟล์นี้

```powershell
node scripts/configure_web_push.cjs firebase/.private/web-push.json
cd mobile
..\.tools\flutter\bin\flutter.bat pub get
..\.tools\flutter\bin\flutter.bat build web --dart-define-from-file=../firebase/.private/web-defines.json --no-web-resources-cdn
cd ../firebase
npx firebase login
npx firebase deploy --only database,hosting --project YOUR_PROJECT_ID
cd ..
```

คำสั่ง deploy นี้เผยแพร่เว็บและ Rules ในโปรเจกต์ที่ระบุ ใช้ Spark ได้ ไม่ deploy Functions ตรวจว่าเลือกโปรเจกต์ถูกต้องก่อนรัน
เมื่อสำเร็จ เปิด URL Hosting ที่ CLI แสดง เช่น `https://YOUR_PROJECT_ID.web.app` เว็บนี้ใช้ข้อมูลจริง **ห้ามใส่ `--dart-define=DEMO=true` ใน build ที่ใช้จริง**

ตัวช่วยสร้างค่าชุดเดียวให้ Flutter และ `mobile/web/firebase-push-config.js` เพื่อให้ Service Worker ใช้โปรเจกต์ตรงกับหน้าเว็บ ถ้าเปลี่ยนโปรเจกต์ให้รันตัวช่วยและ build/deploy ใหม่ทุกครั้ง
ต้องเสิร์ฟเว็บที่ root `/` เพราะ FCM ลงทะเบียน `/firebase-messaging-sw.js` อัตโนมัติเมื่อกดเปิด Push

### เปิดรับแจ้งเตือน

- **คอม / Android ผ่านเว็บ:** เปิดเว็บ HTTPS → เข้าสู่ระบบ → กดกระดิ่ง → Allow/อนุญาต จากนั้นปิดแท็บได้ ต้องอนุญาตแจ้งเตือนในระบบปฏิบัติการด้วย
- **iPhone/iPad ผ่านเว็บ:** iOS/iPadOS 16.4 ขึ้นไป เปิด Safari → Share → **Add to Home Screen** → เปิดไอคอนจากหน้าจอโฮม → ล็อกอิน → กดกระดิ่ง → อนุญาต ไม่ต้องมีแอพ native หรือบัญชีนักพัฒนา Apple สำหรับ Web Push
- **localhost:** ใช้ตรวจหน้าจอ/Service Worker บนคอมเครื่องเดียวได้ แต่ทดสอบ Push และการกดกลับเข้าเว็บจริงให้ใช้ HTTPS Hosting มือถือเข้าผ่าน `http://192.168...` ไม่ได้มีคุณสมบัติเท่ากับ localhost

[ตั้งค่า FCM](https://firebase.google.com/docs/cloud-messaging/flutter/get-started) · [Web Push บน iOS](https://webkit.org/blog/13878/web-push-for-web-apps-on-ios-and-ipados/)

## 3. เปิดตัวส่ง Push บนคอมนี้ก่อน

ตัวส่งใช้สิทธิ์ Admin อ่าน pairing และ token และส่งผ่าน FCM จึงต้องอยู่บนเครื่องที่ไว้ใจได้ กุญแจนี้ห้ามใส่ ESP32 แอพ หน้าเว็บ Git หรือส่งในแชต

1. Firebase Console → Project settings → **Service accounts → Firebase Admin SDK → Generate new private key**
2. เก็บไฟล์ที่ดาวน์โหลดเป็น `D:\NimbusRall\firebase\.private\push-service-account.json` โฟลเดอร์นี้ถูก gitignore ไว้แล้ว
3. ใน Project settings → Cloud Messaging ตรวจว่า **Firebase Cloud Messaging API (V1)** เปิดใช้งาน ถ้ายังปิด ให้กด Manage API in Google Cloud Console และ Enable
4. เปิด PowerShell ที่ `D:\NimbusRall` แล้วรัน:

```powershell
npm --prefix firebase ci
$env:GOOGLE_APPLICATION_CREDENTIALS = 'D:\NimbusRall\firebase\.private\push-service-account.json'
$env:FIREBASE_DATABASE_URL = 'https://YOUR_PROJECT_ID-default-rtdb.asia-southeast1.firebasedatabase.app'
$env:PUSH_APP_URL = 'https://YOUR_PROJECT_ID.web.app/'
npm --prefix firebase run push:start
```

ให้คงหน้าต่างโปรแกรมนี้ไว้และตั้งคอมไม่ให้ Sleep ระหว่างใช้งาน ปิดหน้าเว็บได้ โปรแกรมจะตรวจทุก 15 วินาทีและส่งไปยังมือถือ/เบราว์เซอร์ของเจ้าของบอร์ดที่กดลงทะเบียนแล้ว กด Ctrl+C เพื่อหยุด
บรรทัด `[push] sl-xxxxxx: sent` หมายถึง FCM รับคำขอแล้ว ยังไม่ใช่ใบยืนยันว่าระบบปฏิบัติการแสดงบนหน้าจอสำเร็จ

หากย้ายไปเซิร์ฟเวอร์ ใช้ Node.js 22 ขึ้นไปและคำสั่ง/environment variables เดียวกัน โดยติดตั้ง `npm ci --omit=dev` ในโฟลเดอร์ firebase และใช้ตัวจัดการ service ของเครื่องให้เริ่มใหม่หลัง reboot ไม่ต้องเปิดพอร์ตรับจากอินเทอร์เน็ต ค่าเช่าเซิร์ฟเวอร์ขึ้นกับที่ที่คุณเลือก

## 4. หากใช้แอพ Android/iOS ที่ติดตั้งจริง

เว็บในข้อ 2 ใช้บนมือถือได้อยู่แล้ว ส่วนแอพ Flutter native ให้ทำขั้นตอน native config ในคู่มือหลัก แล้ว build ใหม่หลังเพิ่ม `firebase_messaging`

- **Android:** ต้องมี Google Play services และอนุญาต Notifications โดยเฉพาะ Android 13+ โค้ดเพิ่ม `POST_NOTIFICATIONS` แล้ว
- **iOS native:** ใช้ Mac/Xcode เปิด Runner → Signing & Capabilities → Push Notifications และ Background Modes (Remote notifications/Background fetch) พร้อม provisioning ที่รองรับ Push จากนั้นอัปโหลด APNs authentication key `.p8`, Key ID และ Team ID ใน Firebase Cloud Messaging ต้องมีสิทธิ์ Apple Developer ที่เหมาะสม
- Entitlements ใน repo ใช้ `development` สำหรับทดสอบ ให้ Xcode/Signing จัด environment สำหรับ TestFlight/App Store อย่างถูกต้องตาม provisioning
- เปิดแอพ ล็อกอิน กดกระดิ่ง อนุญาต แล้วกลับหน้าจอโฮมหรือล็อกเครื่องเพื่อทดสอบ

## 5. ทดสอบว่าเด้งหลังปิดหน้าเว็บ

1. ล็อกอินบัญชีเจ้าของบน HTTPS หรือแอพจริง กดกระดิ่งให้ขึ้น **ลงทะเบียนรับ Push แล้ว**
2. ตรวจใน Database ว่ามี `users/OWNER_UID/pushTokens` ของเครื่องนี้ โดยไม่เผยแพร่ค่าของ token
3. เปิดโปรแกรม `push:start` แล้วปิดแท็บเว็บ/กลับหน้าจอโฮม
4. ทดสอบส่วนรับด้วย Firebase Console → Messaging → สร้าง Notification → **Send test message** → ใช้ token ของเครื่องทดสอบจากข้อ 2 การทดสอบนี้ข้ามตัวส่งและผลทำนาย ESP32
5. ทดสอบครบระบบโดยรอให้ ESP32 ส่งผลทำนายที่ **ข้ามเกณฑ์ฝนครั้งใหม่** ตรวจรายการใหม่ใน `devices/DEVICE_ID/alerts` และบรรทัด `sent` ของตัวส่ง จากนั้นตรวจแจ้งเตือนที่มือถือ/คอม
6. ทดสอบเปิดใหม่/ออกจากระบบ: ปิดแล้วเปิดเว็บไม่ควรทำให้เหตุการณ์เดิมส่งซ้ำ การออกจากระบบจะยกเลิก token ของเครื่องนั้น ข้อความที่ FCM รับไปก่อน logout แล้วอาจยังอยู่ระหว่างส่ง

เมื่อเปิดหน้าเว็บอยู่ ใช้ข้อความเตือนในแอพจาก Database ตามเดิม ส่วน FCM แสดง OS notification เมื่ออยู่เบื้องหลัง/ปิดแท็บ เพื่อหลีกเลี่ยงการเตือนซ้ำสองครั้ง

## ข้อจำกัดและการแก้ปัญหา

| อาการ | ตรวจอะไร |
| --- | --- |
| ปิดแท็บแล้วไม่เด้ง | ตัวส่งยังรันอยู่หรือไม่, สิทธิ์เว็บ/OS, Do Not Disturb/Focus, บอร์ดออนไลน์และเกิด alert ใหม่จริงหรือไม่ |
| บน iPhone ไม่ขึ้นขอสิทธิ์ | ต้องเปิดเว็บที่เพิ่มบน Home Screen และกดกระดิ่งจากเว็บนั้น |
| ลงทะเบียน Push ไม่สำเร็จ | Rules รุ่นล่าสุด, บัญชีเจ้าของมี devices, Web config/VAPID key ตรงโปรเจกต์ และไฟล์ Service Worker เปิดได้เป็น JavaScript ไม่ใช่หน้า HTML |
| permission-denied ตอน token | Publish Rules ล่าสุด และล็อกอินบัญชีเจ้าของ ไม่ใช่บัญชี ESP32 |
| messaging/authentication-error / mismatched-credential | บัญชีบริการอยู่โปรเจกต์เดียวกับ token และเปิด FCM HTTP v1 API แล้ว |
| messaging/third-party-auth-error | ตรวจ APNs credentials สำหรับ iOS หรือ Web Push key |
| มีประวัติแต่ไม่ส่ง | เหตุการณ์เก่ากว่า 5 นาที/เวลา ESP32 ยังเป็น 0 จะถูกข้าม หรือเคยส่งไปเครื่องนี้แล้ว |
| เปลี่ยน VAPID/โปรเจกต์แล้วรับไม่ได้ | สร้าง config และ build/deploy ใหม่ ล้างข้อมูลเว็บไซต์เฉพาะเว็บนี้ แล้วล็อกอินและกดกระดิ่งใหม่ |

การปิดเบราว์เซอร์ทั้งโปรแกรม, Force stop แอพ, ถอนสิทธิ์, โหมดประหยัดพลังงาน, ออฟไลน์ และ Focus/Do Not Disturb อาจหยุดหรือหน่วงการแสดง Push ตามระบบปฏิบัติการ ไม่รับประกันการส่งถึงทันที [พฤติกรรม FCM](https://firebase.google.com/docs/cloud-messaging/flutter/receive-messages)

ตัวส่งจำผลรายเหตุการณ์/ราย token ใน `pushDelivery` และลบข้อมูลเก่ากว่า 1 วันเป็นรอบ ๆ มี lease กันตัวส่งทำพร้อมกันและ retry เมื่อส่งไม่สำเร็จ ถ้าโปรแกรมล้มหลัง FCM รับข้อความแต่ก่อนบันทึกผล อาจส่งซ้ำได้ จึงใช้ notification tag ลดรายการซ้ำด้วย ไม่รับประกัน exactly-once
การอ่าน Database ของตัวส่งนับในโควตา Spark โดยไม่โหลดประวัติทั้งหมด อ่านเฉพาะ alert 5 นาทีล่าสุดสูงสุด 100 รายการต่อบอร์ดต่อรอบ

## ผลตรวจโค้ด

- `npm --prefix firebase test`: ผ่าน 13 กรณี รวมกฎ token ส่วนตัว, REST integration และตัวส่งจำลอง (ซ้ำ/หมดอายุ/logout/retry)
- `flutter analyze`: ผ่าน; `flutter test`: ผ่าน 6 กรณี
- ยังไม่ได้ส่ง FCM ไปเครื่องจริง ยังไม่ได้ deploy หรือสร้าง service account ของผู้ใช้ ต้องทำตามขั้นตอนด้านบนก่อน
