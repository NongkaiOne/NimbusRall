// ภาพรวม: ตัวกลางข้อมูลของหน้าจอ แยก Firebase จริงออกจากข้อมูลจำลองด้วย interface เดียวกัน
import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';

import 'models.dart';
import 'push_notifications.dart';

// สัญญาของแหล่งข้อมูล ให้หน้าจอใช้ Firebase หรือ Demo ได้เหมือนกัน
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

// เชื่อม Database ด้วยบัญชีเจ้าของและดูแล Push ของบัญชีนั้น
class FirebaseRepository implements DeviceRepository {
  FirebaseRepository(this.user) {
    unawaited(_push.restore());
  }
  late final _push = PushRegistration(user.uid);
  final User user;
  final _db = FirebaseDatabase.instance;
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
  Stream<List<Json>> logs(String id, int limit) =>
      _list(_db.ref('devices/$id/logs').orderByKey().limitToLast(limit));
  @override
  Stream<List<Json>> alerts(String id) => _list(
    _db.ref('devices/$id/alerts').orderByChild('createdAt').limitToLast(100),
  );

  // The owner writes an immutable request; only the device can write its ACK.
  // Subscribe to one ACK rather than downloading the whole command history.
  @override
  // ติดตาม control และ ACK ของคำสั่งปัจจุบัน ไม่โหลดประวัติ ACK ทั้งหมด
  Stream<Json> control(String id) {
    late StreamController<Json> stream;
    StreamSubscription<Json>? requestSub, ackSub;
    String? active;
    int generation = 0;
    stream = StreamController<Json>(
      onListen: () {
        requestSub = _watch('devices/$id/control').listen((request) {
          active = request['id'] as String?;
          final version = ++generation;
          ackSub?.cancel();
          stream.add(request);
          if (active != null) {
            ackSub = _watch('devices/$id/acks/$active').listen((ack) {
              if (version == generation && !stream.isClosed) {
                stream.add({
                  ...request,
                  if (ack['id'] == active) ...{
                    'status': ack['ok'] == true ? 'accepted' : 'rejected',
                    'ack': ack,
                  },
                });
              }
            }, onError: stream.addError);
          }
        }, onError: stream.addError);
      },
      onCancel: () async {
        generation++;
        await requestSub?.cancel();
        await ackSub?.cancel();
      },
    );
    return stream.stream;
  }

  @override
  // สร้างคำสั่ง ID ใหม่ อายุประมาณ 25 วินาที Rules ตรวจสิทธิ์และสถานะซ้ำที่เซิร์ฟเวอร์
  Future<void> command(String id, String action) async {
    final offset = (await _db.ref('.info/serverTimeOffset').get()).value;
    final now =
        DateTime.now().millisecondsSinceEpoch +
        (offset is num ? offset.toInt() : 0);
    final commandId = _db.ref('devices/$id/acks').push().key!;
    // Rules recheck ownership, online status, allowed action, servo state and pending requests atomically.
    await _db.ref('devices/$id/control').set({
      'id': commandId,
      'action': action,
      'requestedBy': uid,
      'createdAt': ServerValue.timestamp,
      'expiresAt': now + 25000,
      'status': 'pending',
    });
  }

  @override
  Future<String> enableNotifications() => _push.enable();
  @override
  Future<void> signOut() async {
    await _push.signOut();
    await FirebaseAuth.instance.signOut();
  }

  @override
  Future<void> dispose() => _push.dispose();
}

// ข้อมูลและคำสั่งจำลองในเครื่อง ไม่เชื่อมบอร์ดจริง
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
