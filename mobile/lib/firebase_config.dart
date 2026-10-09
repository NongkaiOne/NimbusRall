// ภาพรวม: รับ Firebase Web config จาก --dart-define; Android/iOS ใช้ native config ของตน
import 'package:firebase_core/firebase_core.dart';

// Web values are supplied by scripts/configure_web_push.cjs at build time.
const webFirebaseOptions = FirebaseOptions(
  apiKey: String.fromEnvironment('FIREBASE_API_KEY'),
  appId: String.fromEnvironment('FIREBASE_APP_ID'),
  messagingSenderId: String.fromEnvironment('FIREBASE_MESSAGING_SENDER_ID'),
  projectId: String.fromEnvironment('FIREBASE_PROJECT_ID'),
  authDomain: String.fromEnvironment('FIREBASE_AUTH_DOMAIN'),
  databaseURL: String.fromEnvironment('FIREBASE_DATABASE_URL'),
);
const webPushVapidKey = String.fromEnvironment('FIREBASE_VAPID_KEY');
