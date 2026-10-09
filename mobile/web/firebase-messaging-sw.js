// ภาพรวม: Service Worker รับ Push หลังปิดแท็บ/อยู่เบื้องหลัง โดยให้ FCM แสดง notification payload
/* Firebase Messaging uses this separate scope; Flutter owns the app scope. */
importScripts('firebase-push-config.js');
// Keep this SDK version aligned with firebase_core_web.
importScripts('https://www.gstatic.com/firebasejs/12.19.0/firebase-app-compat.js');
importScripts('https://www.gstatic.com/firebasejs/12.19.0/firebase-messaging-compat.js');
firebase.initializeApp(self.NIMBUS_FIREBASE_CONFIG);
firebase.messaging();
// FCM notification payloads display automatically. Do not call showNotification
// again: that would produce duplicate background notifications. Clicks use the
// sender's HTTPS webpush.fcmOptions.link.
