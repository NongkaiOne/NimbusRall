import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'package:cloud_functions/cloud_functions.dart';
import 'package:crypto/crypto.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:firebase_messaging/firebase_messaging.dart';

import 'models.dart';

abstract class DeviceRepository {
  bool get demo;
  String get uid;
  Stream<Json> devices();
  Stream<Json> state(String id);
  Stream<Json> control(String id);
  Stream<List<Json>> logs(String id, int limit);
  Stream<List<Json>> alerts(String id);
  Future<void> command(String id, String action);
  Future<String> enableNotifications();
  Future<void> signOut();
  Future<void> dispose();
}

class FirebaseRepository implements DeviceRepository {
  FirebaseRepository(this.user) {
    FirebaseMessaging.instance
        .getNotificationSettings()
        .then((settings) async {
          if (settings.authorizationStatus == AuthorizationStatus.authorized ||
              settings.authorizationStatus == AuthorizationStatus.provisional) {
            await enableNotifications();
          }
        })
        .catchError((Object _) {});
  }
  final User user;
  final _db = FirebaseDatabase.instance;
  StreamSubscription<String>? _tokenRefresh;
  String? _tokenKey;
  @override
  bool get demo => false;
  @override
  String get uid => user.uid;
  Stream<Json> _watch(String path) =>
      _db.ref(path).onValue.map((e) => jsonMap(e.snapshot.value));
  Stream<List<Json>> _list(Query query) => query.onValue.map(
    (e) => e.snapshot.children
        .map((s) => {...jsonMap(s.value), 'key': s.key})
        .toList()
        .reversed
        .toList(),
  );
  @override
  Stream<Json> devices() => _watch('users/$uid/devices');
  @override
  Stream<Json> state(String id) => _watch('devices/$id/telemetry');
  @override
  Stream<Json> control(String id) => _watch('devices/$id/control');
  @override
  Stream<List<Json>> logs(String id, int limit) =>
      _list(_db.ref('devices/$id/logs').orderByKey().limitToLast(limit));
  @override
  Stream<List<Json>> alerts(String id) => _list(
    _db.ref('devices/$id/alerts').orderByChild('createdAt').limitToLast(100),
  );
  @override
  Future<void> command(String id, String action) async {
    await FirebaseFunctions.instanceFor(region: 'asia-southeast1')
        .httpsCallable('issueCommand')
        .call({'deviceId': id, 'action': action});
  }

  Future<void> _saveToken(String token) async {
    final key = sha256.convert(utf8.encode(token)).toString();
    await _db.ref('users/$uid/pushTokens/$key').set({
      'token': token,
      'createdAt': ServerValue.timestamp,
    });
    if (_tokenKey != null && _tokenKey != key) {
      await _db.ref('users/$uid/pushTokens/$_tokenKey').remove();
    }
    _tokenKey = key;
  }

  @override
  Future<String> enableNotifications() async {
    final messaging = FirebaseMessaging.instance;
    final permission = await messaging.requestPermission();
    if (permission.authorizationStatus == AuthorizationStatus.denied) {
      return 'ยังไม่ได้อนุญาตแจ้งเตือน เปิดสิทธิ์ได้ในการตั้งค่ามือถือ';
    }
    if (defaultTargetPlatform == TargetPlatform.iOS &&
        await messaging.getAPNSToken() == null) {
      return 'กำลังรอการลงทะเบียนจาก Apple กรุณาลองอีกครั้ง';
    }
    final token = await messaging.getToken();
    if (token == null) return 'ยังลงทะเบียนแจ้งเตือนไม่สำเร็จ กรุณาลองอีกครั้ง';
    await _saveToken(token);
    await _tokenRefresh?.cancel();
    _tokenRefresh = messaging.onTokenRefresh.listen((value) {
      // A later manual registration can recover a transient network failure.
      _saveToken(value).catchError((Object _) {});
    });
    return 'เปิดแจ้งเตือนฝนแล้ว';
  }

  @override
  Future<void> signOut() async {
    // Remove this phone before signing out so another user cannot receive its alerts.
    await _tokenRefresh?.cancel();
    final appleNotReady =
        defaultTargetPlatform == TargetPlatform.iOS &&
        await FirebaseMessaging.instance.getAPNSToken() == null;
    final token =
        _tokenKey ??
        (appleNotReady
            ? null
            : await FirebaseMessaging.instance.getToken().then(
                (t) => t == null
                    ? null
                    : sha256.convert(utf8.encode(t)).toString(),
              ));
    if (token != null) await _db.ref('users/$uid/pushTokens/$token').remove();
    if (!appleNotReady) await FirebaseMessaging.instance.deleteToken();
    await FirebaseAuth.instance.signOut();
  }

  @override
  Future<void> dispose() async {
    await _tokenRefresh?.cancel();
  }
}

class DemoRepository implements DeviceRepository {
  DemoRepository() {
    _heartbeat = Timer.periodic(
      const Duration(seconds: 10),
      (_) => _changes.add(null),
    );
  }
  late final Timer _heartbeat;
  final _changes = StreamController<void>.broadcast();
  Json _command = {};
  String _line = 'RETRACTED', _mode = 'MANUAL';
  int _seq = 24;
  @override
  bool get demo => true;
  @override
  String get uid => 'DEMO';
  Json get _state => {
    'temp': 30.8,
    'hum': 72.4,
    'press': 1005.2,
    'lux': 1800,
    'p_rain': 0.78,
    'retract_threshold': 0.7,
    'sensor_err': false,
    'fallback': false,
    'servo_enabled': true,
    'mode': _mode,
    'line': _line,
    'receivedAt': DateTime.now().millisecondsSinceEpoch,
    'measurement_age_s': 3,
    'seq': _seq,
    'rssi': -51,
    'model': 'Demo',
  };
  Stream<T> _watch<T>(T Function() read) async* {
    yield read();
    yield* _changes.stream.map((_) => read());
  }

  @override
  Stream<Json> devices() => Stream.value({
    'sl-demo': {'name': 'ราวตากผ้าตัวอย่าง'},
  });
  @override
  Stream<Json> state(String id) => _watch(() => _state);
  @override
  Stream<Json> control(String id) => _watch(() => _command);
  @override
  Stream<List<Json>> logs(String id, int limit) => _watch(
    () => List.generate(
      24,
      (i) => {
        ..._state,
        'seq': _seq - i,
        'flags': 0,
        'epoch': DateTime.now().millisecondsSinceEpoch ~/ 1000 - i * 60,
        'temp': 30.8 - i * 0.08,
        'p_rain': (0.78 - i * 0.025).clamp(0, 1),
      },
    ),
  );
  @override
  Stream<List<Json>> alerts(String id) => Stream.value([
    {
      'title': 'มีแนวโน้มฝนตก',
      'body': 'โอกาสฝนตก 78% ตรวจสอบราวตากผ้า',
      'createdAt': DateTime.now().millisecondsSinceEpoch,
      'p_rain': 0.78,
    },
  ]);
  @override
  Future<void> command(String id, String action) async {
    final now = DateTime.now().millisecondsSinceEpoch;
    _command = {
      'action': action,
      'status': 'pending',
      'expiresAt': now + 30000,
    };
    _changes.add(null);
    await Future<void>.delayed(const Duration(milliseconds: 400));
    switch (action) {
      case 'RETRACT':
        _line = 'RETRACTED';
      case 'EXTEND':
        _line = 'EXTENDED';
      case 'MODE AUTO':
        _mode = 'AUTO';
      case 'MODE MANUAL':
        _mode = 'MANUAL';
      case 'MEASURE':
        _seq++;
      case 'STOP':
        _line = 'UNKNOWN';
    }
    _command = {..._command, 'status': 'accepted'};
    if (!_changes.isClosed) _changes.add(null);
  }

  @override
  Future<String> enableNotifications() async =>
      'โหมดตัวอย่าง: ไม่มีการส่งแจ้งเตือนจริง';
  @override
  Future<void> signOut() async {}
  @override
  Future<void> dispose() async {
    _heartbeat.cancel();
    await _changes.close();
  }
}
