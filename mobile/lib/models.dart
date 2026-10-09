typedef Json = Map<String, dynamic>;

Json jsonMap(Object? value) => value is Map
    ? value.map((key, value) => MapEntry(key.toString(), value))
    : {};

class DeviceState {
  const DeviceState(this.data);
  final Json data;
  bool onlineAt(DateTime now) {
    final timestamp = data['receivedAt'];
    if (timestamp is! num) return false;
    final age = now.millisecondsSinceEpoch - timestamp.toInt();
    return age >= -5000 && age < 30000;
  }

  bool get servoEnabled => data['servo_enabled'] == true;
  bool get sensorError => data['sensor_err'] != false;
  bool get measurementFresh =>
      (data['measurement_age_s'] as num? ?? 999999) <= 300;
  bool canMove(DateTime now) => onlineAt(now) && servoEnabled;
  String get line => data['line'] as String? ?? 'UNKNOWN';
  String get mode => data['mode'] as String? ?? '—';
}

String numberText(Object? value, [int decimals = 1]) =>
    value is num && value.isFinite ? value.toStringAsFixed(decimals) : '—';

String dateText(Object? milliseconds) {
  if (milliseconds is! num || milliseconds <= 0) return 'ยังไม่มีเวลา';
  final d = DateTime.fromMillisecondsSinceEpoch(milliseconds.toInt());
  String two(int n) => n.toString().padLeft(2, '0');
  return '${two(d.day)}/${two(d.month)}/${d.year} ${two(d.hour)}:${two(d.minute)}:${two(d.second)}';
}

String lineText(String value) => switch (value) {
  'EXTENDED' => 'ราวอยู่ด้านนอก',
  'RETRACTED' => 'เก็บราวแล้ว',
  'MOVING_OUT' => 'กำลังตากผ้า',
  'MOVING_IN' => 'กำลังเก็บผ้า',
  'FAULT_TIMEOUT' => 'มอเตอร์ทำงานเกินเวลา',
  _ => 'ยังไม่ทราบตำแหน่งราว',
};

String commandText(Json c, DateTime now) {
  if (c.isEmpty) return 'ยังไม่มีคำสั่ง';
  final status = c['status'];
  if ((status == 'pending' || status == 'delivered') &&
      (c['expiresAt'] as num? ?? 0) <= now.millisecondsSinceEpoch) {
    return 'คำสั่งหมดเวลา กรุณาตรวจสอบอุปกรณ์';
  }
  return switch (status) {
    'pending' => 'รออุปกรณ์รับคำสั่ง',
    'delivered' => 'ส่งถึงอุปกรณ์แล้ว รอการตอบรับ',
    'accepted' => 'อุปกรณ์รับคำสั่งแล้ว ดูผลที่สถานะราว',
    'rejected' => 'อุปกรณ์ปฏิเสธ: ${jsonMap(c['ack'])['msg'] ?? ''}',
    'expired' => 'คำสั่งหมดเวลา',
    _ => 'กำลังตรวจสอบคำสั่ง',
  };
}

String rainText(Json row) {
  if (((row['flags'] as num? ?? 0).toInt() & 1) != 0) return 'ประเมินไม่ได้';
  final p = row['p_rain'];
  return p is num ? '${numberText(p * 100, 0)}%' : '—';
}
