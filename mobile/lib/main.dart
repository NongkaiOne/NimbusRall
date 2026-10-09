import 'dart:async';

import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'models.dart';
import 'repository.dart';

@pragma('vm:entry-point')
Future<void> onBackgroundMessage(RemoteMessage message) async {
  await Firebase.initializeApp();
}

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  bool ready = false;
  if (!const bool.fromEnvironment('DEMO')) {
    try {
      await Firebase.initializeApp();
      FirebaseMessaging.onBackgroundMessage(onBackgroundMessage);
      ready = true;
    } catch (_) {
      /* Setup page keeps an unconfigured build usable. */
    }
  }
  runApp(SmartLineApp(firebaseReady: ready));
}

class SmartLineApp extends StatelessWidget {
  const SmartLineApp({super.key, this.firebaseReady = false, this.repository});
  final bool firebaseReady;
  final DeviceRepository? repository;
  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'NimbusRall',
    debugShowCheckedModeBanner: false,
    theme: ThemeData(
      useMaterial3: true,
      colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xff146c59)),
      scaffoldBackgroundColor: const Color(0xfff5f7f3),
      inputDecorationTheme: const InputDecorationTheme(
        border: OutlineInputBorder(),
      ),
      cardTheme: const CardThemeData(
        elevation: 0,
        margin: EdgeInsets.only(bottom: 12),
      ),
    ),
    home: repository != null
        ? DeviceHome(repository: repository!)
        : EntryPage(ready: firebaseReady),
  );
}

class EntryPage extends StatefulWidget {
  const EntryPage({super.key, required this.ready});
  final bool ready;
  @override
  State<EntryPage> createState() => _EntryPageState();
}

class _EntryPageState extends State<EntryPage> {
  final _email = TextEditingController(), _password = TextEditingController();
  bool _busy = false;
  String? _error;
  DeviceRepository? _repository;
  StreamSubscription<User?>? _auth;
  @override
  void initState() {
    super.initState();
    if (widget.ready) {
      _auth = FirebaseAuth.instance.authStateChanges().listen((user) {
        if (!mounted) return;
        if (_repository?.uid == user?.uid) return;
        final previous = _repository;
        setState(
          () => _repository = user == null ? null : FirebaseRepository(user),
        );
        unawaited(previous?.dispose());
      });
    }
  }

  @override
  void dispose() {
    _email.dispose();
    _password.dispose();
    _auth?.cancel();
    _repository?.dispose();
    super.dispose();
  }

  Future<void> _login() async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      await FirebaseAuth.instance.signInWithEmailAndPassword(
        email: _email.text.trim(),
        password: _password.text,
      );
      _password.clear();
    } on FirebaseAuthException catch (e) {
      if (mounted) setState(() => _error = 'เข้าสู่ระบบไม่สำเร็จ (${e.code})');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  void _demo() => setState(() => _repository = DemoRepository());
  @override
  Widget build(BuildContext context) {
    if (_repository case final repo?) {
      return DeviceHome(
        key: ValueKey(repo.uid),
        repository: repo,
        onExit: () async {
          if (repo.demo) {
            setState(() => _repository = null);
            await repo.dispose();
          } else {
            await repo.signOut();
          }
        },
      );
    }
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(28),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 420),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const Icon(
                    Icons.cloud_outlined,
                    size: 72,
                    color: Color(0xff146c59),
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'NimbusRall',
                    textAlign: TextAlign.center,
                    style: Theme.of(context).textTheme.headlineLarge,
                  ),
                  const Text(
                    'ดูสภาพอากาศ ดูแลผ้าได้จากทุกที่',
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 32),
                  if (widget.ready) ...[
                    TextField(
                      controller: _email,
                      keyboardType: TextInputType.emailAddress,
                      autofillHints: const [AutofillHints.email],
                      decoration: const InputDecoration(labelText: 'อีเมล'),
                    ),
                    const SizedBox(height: 16),
                    TextField(
                      controller: _password,
                      obscureText: true,
                      autofillHints: const [AutofillHints.password],
                      onSubmitted: (_) {
                        if (!_busy) _login();
                      },
                      decoration: const InputDecoration(labelText: 'รหัสผ่าน'),
                    ),
                    const SizedBox(height: 16),
                    FilledButton(
                      onPressed: _busy ? null : _login,
                      child: Text(_busy ? 'กำลังเข้าสู่ระบบ…' : 'เข้าสู่ระบบ'),
                    ),
                    if (_error != null)
                      Text(
                        _error!,
                        style: TextStyle(
                          color: Theme.of(context).colorScheme.error,
                        ),
                      ),
                  ] else
                    const Card(
                      child: Padding(
                        padding: EdgeInsets.all(20),
                        child: Text(
                          'ยังไม่ได้ตั้งค่า Firebase\nเปิดโหมดตัวอย่างเพื่อดูหน้าจอได้ทันที การใช้งานกับ ESP32 ต้องตั้งค่า Firebase ตามคู่มือก่อน',
                        ),
                      ),
                    ),
                  const SizedBox(height: 12),
                  OutlinedButton(
                    onPressed: _demo,
                    child: const Text('ลองโหมดตัวอย่าง'),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class DeviceHome extends StatefulWidget {
  const DeviceHome({super.key, required this.repository, this.onExit});
  final DeviceRepository repository;
  final Future<void> Function()? onExit;
  @override
  State<DeviceHome> createState() => _DeviceHomeState();
}

class _DeviceHomeState extends State<DeviceHome> {
  DeviceRepository get repo => widget.repository;
  final List<StreamSubscription<dynamic>> _subs = [];
  final List<StreamSubscription<dynamic>> _deviceSubs = [];
  StreamSubscription<List<Json>>? _logSub;
  Timer? _timer;
  Json _devices = {}, _state = {}, _control = {};
  List<Json> _logs = [], _alerts = [];
  String? _id, _error, _notificationDevice;
  bool _busy = false;
  int _page = 0, _limit = 100;
  DateTime _now = DateTime.now();

  @override
  void initState() {
    super.initState();
    _subs.add(
      repo.devices().listen((devices) {
        if (!mounted) return;
        setState(() => _devices = devices);
        if (_notificationDevice != null &&
            devices.containsKey(_notificationDevice)) {
          _select(_notificationDevice);
          _notificationDevice = null;
        } else if (!devices.containsKey(_id)) {
          _select(devices.keys.firstOrNull);
        }
      }, onError: _streamError),
    );
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (mounted) setState(() => _now = DateTime.now());
    });
    if (!repo.demo) {
      _subs.add(
        FirebaseMessaging.onMessage.listen((m) {
          _notice(m.notification?.body ?? 'มีการแจ้งเตือนใหม่');
        }),
      );
      _subs.add(FirebaseMessaging.onMessageOpenedApp.listen(_openNotification));
      FirebaseMessaging.instance.getInitialMessage().then((m) {
        if (mounted && m != null) _openNotification(m);
      });
    }
  }

  void _openNotification(RemoteMessage m) {
    if (!mounted) return;
    final id = m.data['deviceId'];
    if (_devices.containsKey(id)) {
      _select(id);
    } else {
      _notificationDevice = id;
    }
    setState(() => _page = 2);
  }

  void _streamError(Object error) {
    if (mounted) {
      setState(
        () => _error = 'โหลดข้อมูลไม่สำเร็จ ตรวจสอบอินเทอร์เน็ตและสิทธิ์บัญชี',
      );
    }
  }

  void _select(String? id) {
    for (final sub in _deviceSubs) {
      sub.cancel();
    }
    _deviceSubs.clear();
    _logSub?.cancel();
    setState(() {
      _id = id;
      _state = {};
      _control = {};
      _logs = [];
      _alerts = [];
      _error = null;
      _limit = 100;
    });
    if (id == null) return;
    void update(void Function() fn) {
      if (mounted && _id == id) setState(fn);
    }

    _deviceSubs.add(
      repo
          .state(id)
          .listen((v) => update(() => _state = v), onError: _streamError),
    );
    _deviceSubs.add(
      repo
          .control(id)
          .listen((v) => update(() => _control = v), onError: _streamError),
    );
    _deviceSubs.add(
      repo
          .alerts(id)
          .listen((v) => update(() => _alerts = v), onError: _streamError),
    );
    _loadLogs();
  }

  void _loadLogs() {
    _logSub?.cancel();
    final id = _id;
    if (id == null) return;
    _logSub = repo.logs(id, _limit).listen((v) {
      if (mounted && _id == id) setState(() => _logs = v);
    }, onError: _streamError);
  }

  @override
  void dispose() {
    _timer?.cancel();
    _logSub?.cancel();
    for (final sub in [..._subs, ..._deviceSubs]) {
      sub.cancel();
    }
    super.dispose();
  }

  void _notice(String text) {
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(text)));
    }
  }

  Future<void> _send(String action) async {
    final id = _id;
    if (_busy || id == null) return;
    setState(() => _busy = true);
    try {
      await repo.command(id, action);
    } catch (_) {
      _notice(
        'ส่งคำสั่งไม่สำเร็จ อุปกรณ์อาจออฟไลน์ มีคำสั่งค้าง หรือไม่ได้เปิดใช้ servo',
      );
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Widget _card(Widget child) => Card(
    child: Padding(padding: const EdgeInsets.all(20), child: child),
  );
  Widget _heading(String text) => Padding(
    padding: const EdgeInsets.only(top: 8, bottom: 16),
    child: Text(text, style: Theme.of(context).textTheme.titleLarge),
  );
  @override
  Widget build(BuildContext context) {
    final state = DeviceState(_state);
    final online = state.onlineAt(_now);
    return Scaffold(
      appBar: AppBar(
        title: const Text('NimbusRall'),
        actions: [
          IconButton(
            tooltip: 'เปิดแจ้งเตือนฝน',
            icon: const Icon(Icons.notifications_active_outlined),
            onPressed: () async {
              try {
                _notice(await repo.enableNotifications());
              } catch (_) {
                _notice(
                  'เปิดแจ้งเตือนไม่สำเร็จ ตรวจสอบการตั้งค่า Firebase และสิทธิ์มือถือ',
                );
              }
            },
          ),
          if (widget.onExit != null)
            IconButton(
              tooltip: 'ออกจากระบบ',
              icon: const Icon(Icons.logout),
              onPressed: () async {
                try {
                  await widget.onExit!();
                } catch (_) {
                  _notice(
                    'ออกจากระบบไม่สำเร็จ กรุณาลองอีกครั้งเมื่อมีอินเทอร์เน็ต',
                  );
                }
              },
            ),
        ],
      ),
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 760),
            child: ListView(
              key: ValueKey(_page),
              padding: const EdgeInsets.fromLTRB(20, 8, 20, 24),
              children: [
                if (repo.demo)
                  _card(
                    const Row(
                      children: [
                        Icon(Icons.science_outlined),
                        SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            'โหมดตัวอย่าง • ข้อมูลจำลอง\nปุ่มต่าง ๆ ไม่สั่งงาน ESP32 จริง',
                          ),
                        ),
                      ],
                    ),
                  ),
                if (_devices.isNotEmpty)
                  DropdownButtonFormField<String>(
                    key: ValueKey(_id),
                    initialValue: _id,
                    isExpanded: true,
                    decoration: const InputDecoration(labelText: 'อุปกรณ์'),
                    items: _devices.entries
                        .map(
                          (e) => DropdownMenuItem(
                            value: e.key,
                            child: Text(
                              '${jsonMap(e.value)['name'] ?? e.key} • ${e.key}',
                            ),
                          ),
                        )
                        .toList(),
                    onChanged: _select,
                  ),
                const SizedBox(height: 16),
                if (_error != null)
                  _card(
                    Text(
                      _error!,
                      style: TextStyle(
                        color: Theme.of(context).colorScheme.error,
                      ),
                    ),
                  ),
                if (_devices.isEmpty)
                  _card(
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          'ยังไม่มีอุปกรณ์ในบัญชี\nลงทะเบียน ESP32 ด้วย UID ของบัญชีนี้ตามคู่มือ',
                        ),
                        const SizedBox(height: 12),
                        SelectableText(repo.uid),
                        TextButton.icon(
                          onPressed: () =>
                              Clipboard.setData(ClipboardData(text: repo.uid)),
                          icon: const Icon(Icons.copy),
                          label: const Text('คัดลอก UID'),
                        ),
                      ],
                    ),
                  )
                else ...[
                  Row(
                    children: [
                      Icon(
                        Icons.circle,
                        size: 10,
                        color: online ? Colors.green : Colors.grey,
                      ),
                      const SizedBox(width: 8),
                      Text(
                        online
                            ? 'อุปกรณ์ออนไลน์'
                            : 'อุปกรณ์ออฟไลน์ / ข้อมูลเก่า',
                      ),
                      const Spacer(),
                      Text(state.mode),
                    ],
                  ),
                  const SizedBox(height: 4),
                  Text(
                    'อัปเดต ${dateText(_state['receivedAt'])}',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 16),
                  if (_page == 0) ..._dashboard(state, online),
                  if (_page == 1) ..._history(),
                  if (_page == 2) ..._notifications(),
                  if (_page == 3) ..._controls(state),
                ],
              ],
            ),
          ),
        ),
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _page,
        onDestinationSelected: (v) => setState(() => _page = v),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            label: 'ภาพรวม',
          ),
          NavigationDestination(icon: Icon(Icons.history), label: 'บันทึก'),
          NavigationDestination(
            icon: Icon(Icons.notifications_outlined),
            label: 'แจ้งเตือน',
          ),
          NavigationDestination(icon: Icon(Icons.tune), label: 'ควบคุม'),
        ],
      ),
    );
  }

  List<Widget> _dashboard(DeviceState state, bool online) {
    final p = _state['p_rain'];
    final valid = !state.sensorError && state.measurementFresh && p is num;
    return [
      _card(
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('โอกาสฝนตก'),
            Text(
              valid ? '${numberText(p * 100, 0)}%' : 'ยังประเมินไม่ได้',
              style: Theme.of(context).textTheme.displayMedium
                  ?.copyWith(color: const Color(0xff146c59)),
            ),
            const SizedBox(height: 12),
            LinearProgressIndicator(
              value: valid ? p.toDouble().clamp(0, 1) : 0,
              minHeight: 8,
              borderRadius: BorderRadius.circular(8),
            ),
            const SizedBox(height: 12),
            Text(
              !online
                  ? 'ข้อมูลล่าสุดที่บันทึกไว้ อุปกรณ์ยังไม่ออนไลน์'
                  : !valid
                  ? 'ตรวจสอบเซ็นเซอร์หรือรอผลวัดใหม่'
                  : p >= (_state['retract_threshold'] as num? ?? 0.7)
                  ? 'มีแนวโน้มฝนตก ตรวจสอบราวตากผ้า'
                  : 'ยังไม่ถึงเกณฑ์แจ้งเตือนฝน',
            ),
            if (_state['fallback'] == true) const Text('ใช้การประเมินสำรอง'),
          ],
        ),
      ),
      LayoutBuilder(
        builder: (context, box) => Wrap(
          spacing: 12,
          children: [
            for (final item in [
              ('อุณหภูมิ', 'temp', '°C', Icons.thermostat),
              ('ความชื้น', 'hum', '%', Icons.water_drop_outlined),
              ('ความกดอากาศ', 'press', 'hPa', Icons.speed),
              ('แสง', 'lux', 'lux', Icons.light_mode_outlined),
            ])
              SizedBox(
                width: (box.maxWidth - 12) / 2,
                child: _card(
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(item.$4, color: const Color(0xff146c59)),
                      const SizedBox(height: 8),
                      Text(item.$1),
                      Text(
                        '${state.sensorError ? '—' : numberText(_state[item.$2])} ${item.$3}',
                        style: Theme.of(context).textTheme.titleLarge,
                      ),
                    ],
                  ),
                ),
              ),
          ],
        ),
      ),
      _card(
        ListTile(
          contentPadding: EdgeInsets.zero,
          leading: const Icon(Icons.dry_cleaning_outlined),
          title: Text(lineText(state.line)),
          subtitle: Text(
            state.servoEnabled
                ? 'เปิดใช้งาน servo'
                : 'เฟิร์มแวร์ปิดใช้งาน servo อยู่',
          ),
        ),
      ),
      FilledButton.icon(
        onPressed: online && !_busy ? () => _send('MEASURE') : null,
        icon: const Icon(Icons.refresh),
        label: const Text('วัดค่าตอนนี้'),
      ),
      Text(
        'โมเดล: ${_state['model'] ?? '—'} • Wi-Fi ${numberText(_state['rssi'], 0)} dBm',
        textAlign: TextAlign.center,
        style: Theme.of(context).textTheme.bodySmall,
      ),
    ];
  }

  List<Widget> _history() => [
    _heading('บันทึกการวัด'),
    const Text('แตะรายการเพื่อดูรายละเอียด • ใหม่ที่สุดอยู่ด้านบน'),
    const SizedBox(height: 12),
    if (_logs.isEmpty) _card(const Text('ยังไม่มีบันทึกที่ซิงก์จากอุปกรณ์')),
    for (final row in _logs)
      Card(
        child: ListTile(
          leading: const Icon(Icons.analytics_outlined),
          title: Text('ฝน ${rainText(row)} • ${numberText(row['temp'])} °C'),
          subtitle: Text(
            '${dateText((row['epoch'] as num? ?? 0) * 1000)} • #${row['seq']}',
          ),
          trailing: const Icon(Icons.chevron_right),
          onTap: () => showModalBottomSheet<void>(
            context: context,
            isScrollControlled: true,
            builder: (context) => SafeArea(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'บันทึก #${row['seq']}',
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    const SizedBox(height: 16),
                    Text(
                      'เวลาวัด: ${dateText((row['epoch'] as num? ?? 0) * 1000)}',
                    ),
                    Text('อุณหภูมิ: ${numberText(row['temp'])} °C'),
                    Text('ความชื้น: ${numberText(row['hum'])}%'),
                    Text('ความกดอากาศ: ${numberText(row['press'])} hPa'),
                    Text('แสง: ${numberText(row['lux'])} lux'),
                    Text('โอกาสฝนตก: ${rainText(row)}'),
                    Text(lineText(row['line'] as String? ?? 'UNKNOWN')),
                    if (((row['flags'] as num? ?? 0).toInt() & 1) != 0)
                      const Text('เซ็นเซอร์อ่านค่าไม่สำเร็จ'),
                    const SizedBox(height: 20),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    if (_logs.length >= _limit)
      OutlinedButton(
        onPressed: () {
          setState(() => _limit += 100);
          _loadLogs();
        },
        child: const Text('ดูบันทึกเก่ากว่านี้'),
      ),
  ];
  List<Widget> _notifications() => [
    _heading('แจ้งเตือนฝน'),
    const Text(
      'แจ้งเมื่อผลทำนายข้ามเกณฑ์ฝนตก กดไอคอนกระดิ่งด้านบนเพื่ออนุญาตแจ้งเตือนบนมือถือ',
    ),
    const SizedBox(height: 16),
    if (_alerts.isEmpty) _card(const Text('ยังไม่มีการแจ้งเตือน')),
    for (final alert in _alerts)
      _card(
        ListTile(
          contentPadding: EdgeInsets.zero,
          leading: const Icon(Icons.umbrella_outlined),
          title: Text(alert['title'] as String? ?? 'มีแนวโน้มฝนตก'),
          subtitle: Text(
            '${alert['body'] ?? ''}\n${dateText(alert['createdAt'])}',
          ),
        ),
      ),
  ];
  List<Widget> _controls(DeviceState state) {
    final can = state.canMove(_now) && !_busy;
    final pending =
        ['pending', 'delivered'].contains(_control['status']) &&
        (_control['expiresAt'] as num? ?? 0) > _now.millisecondsSinceEpoch;
    return [
      _heading('ควบคุมราวตากผ้า'),
      _card(
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              lineText(state.line),
              style: Theme.of(context).textTheme.titleLarge,
            ),
            const SizedBox(height: 8),
            Text(
              !state.servoEnabled
                  ? 'ยังปิด servo ในเฟิร์มแวร์ ปุ่มควบคุมจะพร้อมเมื่อเปิดใช้งาน'
                  : !state.onlineAt(_now)
                  ? 'รออุปกรณ์ออนไลน์ก่อนส่งคำสั่ง'
                  : 'เลือกโหมด Manual ก่อนสั่งตากหรือเก็บผ้าด้วยตัวเอง',
            ),
          ],
        ),
      ),
      SegmentedButton<String>(
        segments: const [
          ButtonSegment(value: 'AUTO', label: Text('อัตโนมัติ')),
          ButtonSegment(value: 'MANUAL', label: Text('Manual')),
        ],
        selected: {state.mode == 'AUTO' ? 'AUTO' : 'MANUAL'},
        onSelectionChanged: can && !pending
            ? (v) => _send('MODE ${v.first}')
            : null,
      ),
      const SizedBox(height: 20),
      FilledButton.icon(
        onPressed: can && !pending && state.mode == 'MANUAL'
            ? () => _send('RETRACT')
            : null,
        icon: const Icon(Icons.home_outlined),
        label: const Text('เก็บผ้า'),
      ),
      const SizedBox(height: 10),
      OutlinedButton.icon(
        onPressed: can && !pending && state.mode == 'MANUAL'
            ? () => _send('EXTEND')
            : null,
        icon: const Icon(Icons.wb_sunny_outlined),
        label: const Text('ตากผ้า'),
      ),
      const SizedBox(height: 10),
      FilledButton.tonalIcon(
        onPressed: can ? () => _send('STOP') : null,
        icon: const Icon(Icons.stop_circle_outlined),
        label: const Text('หยุด'),
      ),
      const SizedBox(height: 20),
      _card(
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('คำสั่งล่าสุด: ${_control['action'] ?? '—'}'),
            Text(commandText(_control, _now)),
          ],
        ),
      ),
      const Text(
        'คำสั่งผ่านอินเทอร์เน็ตอาจใช้เวลาหลายวินาที การตอบรับคำสั่งยังไม่ได้หมายความว่าราวเคลื่อนที่เสร็จแล้ว',
      ),
    ];
  }
}
