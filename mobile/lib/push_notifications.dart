// ภาพรวม: ขอสิทธิ์แจ้งเตือน จัดการ FCM token ของผู้ใช้ และยกเลิกการลงทะเบียนเมื่อออกจากระบบ
import 'dart:async';
import 'dart:convert';

import 'package:crypto/crypto.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/foundation.dart';

import 'firebase_config.dart';

// จุดรับข้อความเบื้องหลังของ native ให้ OS แสดง notification เพื่อไม่แสดงซ้ำ
@pragma('vm:entry-point')
Future<void> receiveBackgroundPush(RemoteMessage message) async {
  await Firebase.initializeApp();
  // Notification payloads are displayed by the OS. Do not display them twice.
}

// ดูแล token ของเครื่อง/บัญชีนี้ ปิดหน้าได้โดยยังลงทะเบียนรับ Push
class PushRegistration {
  PushRegistration(this.uid);
  final String uid;
  StreamSubscription<String>? _refresh;
  Future<void> _pending = Future.value();
  final Set<String> _keys = {};
  bool _closed = false;
  String? _lastKey;
  final _messaging = FirebaseMessaging.instance;

  bool get _isOwner => FirebaseAuth.instance.currentUser?.uid == uid;

  // ต่อการลงทะเบียนเดิมเมื่อเคยอนุญาตแล้ว โดยไม่เปิดคำขอสิทธิ์เอง
  Future<void> restore() async {
    try {
      if (!await _messaging.isSupported()) return;
      final settings = await _messaging.getNotificationSettings();
      if (settings.authorizationStatus == AuthorizationStatus.authorized) {
        await _register();
      }
    } catch (_) {
      // The bell lets the user explicitly retry and see an actionable error.
    }
  }

  // เรียกจากกระดิ่งเพื่อขอสิทธิ์และลงทะเบียนตามแพลตฟอร์ม
  Future<String> enable() async {
    if (!await _messaging.isSupported()) {
      return 'เบราว์เซอร์นี้ยังไม่รองรับ Push กรุณาใช้ HTTPS และเบราว์เซอร์ที่รองรับ';
    }
    if (kIsWeb && webPushVapidKey.isEmpty) {
      return 'ยังไม่ได้ตั้งค่า Web Push VAPID key กรุณาทำตามคู่มือ PUSH_NOTIFICATIONS';
    }
    final settings = await _messaging.requestPermission(
      alert: true,
      badge: true,
      sound: true,
    );
    if (settings.authorizationStatus != AuthorizationStatus.authorized &&
        settings.authorizationStatus != AuthorizationStatus.provisional) {
      return 'ยังไม่ได้อนุญาตแจ้งเตือน กรุณาเปิดสิทธิ์ในเบราว์เซอร์หรือการตั้งค่ามือถือ';
    }
    await _register();
    return 'ลงทะเบียนรับ Push แล้ว ต้องเปิดโปรแกรมตัวส่งไว้จึงจะแจ้งเตือนฝนอัตโนมัติ';
  }

  Future<void> _register() async {
    if (_closed || !_isOwner) return;
    if (kIsWeb && webPushVapidKey.isEmpty) return;
    if (!kIsWeb && defaultTargetPlatform == TargetPlatform.iOS) {
      if (await _messaging.getAPNSToken() == null) {
        throw StateError(
          'APNs ยังไม่พร้อม กรุณารอสักครู่แล้วกดกระดิ่งอีกครั้ง',
        );
      }
    }
    _refresh ??= _messaging.onTokenRefresh.listen((token) {
      unawaited(_save(token).catchError((Object _) {}));
    });
    final token = await _messaging.getToken(
      vapidKey: kIsWeb ? webPushVapidKey : null,
    );
    if (token == null) throw StateError('ยังไม่ได้รับ Push token');
    await _save(token);
  }

  // เก็บ token ใต้ UID เจ้าของ ใช้ SHA-256 ตั้งชื่อรายการและจัดลำดับการเปลี่ยน token
  Future<void> _save(String token) {
    final task = _pending.catchError((Object _) {}).then((_) async {
      if (_closed || !_isOwner) return;
      final key = sha256.convert(utf8.encode(token)).toString();
      final refs = FirebaseDatabase.instance.ref('users/$uid/pushTokens');
      _keys.add(key);
      await refs.child(key).set({
        'token': token,
        'platform': kIsWeb ? 'web' : defaultTargetPlatform.name,
        'updatedAt': ServerValue.timestamp,
      });
      if (_lastKey != null && _lastKey != key) {
        await refs.child(_lastKey!).remove();
        _keys.remove(_lastKey);
      }
      _lastKey = key;
    });
    _pending = task;
    return task;
  }

  // ลบการลงทะเบียนเครื่องนี้ก่อนออกจากบัญชี รวมกรณีเพิ่งรีโหลดหน้า
  Future<void> signOut() async {
    _closed = true;
    await _refresh?.cancel();
    await _pending.catchError((Object _) {});
    // A page can be reloaded and signed out before restore() finishes. Resolve
    // this installation's existing token too, not only tokens saved this session.
    if ((!kIsWeb || webPushVapidKey.isNotEmpty) &&
        await _messaging.isSupported()) {
      final settings = await _messaging.getNotificationSettings();
      if (settings.authorizationStatus == AuthorizationStatus.authorized ||
          settings.authorizationStatus == AuthorizationStatus.provisional) {
        final token = await _messaging.getToken(
          vapidKey: kIsWeb ? webPushVapidKey : null,
        );
        if (token != null) {
          _keys.add(sha256.convert(utf8.encode(token)).toString());
        }
      }
    }
    for (final key in _keys) {
      await FirebaseDatabase.instance
          .ref('users/$uid/pushTokens/$key')
          .remove();
    }
    if (_keys.isNotEmpty) await _messaging.deleteToken();
    _keys.clear();
  }

  // ยกเลิกตัวฟังของ UI แต่เก็บ token เพื่อรับ Push หลังปิดหน้า
  Future<void> dispose() async {
    _closed = true;
    await _refresh?.cancel();
    await _pending.catchError((Object _) {});
    // Closing the UI must keep the token registered for background delivery.
  }
}
