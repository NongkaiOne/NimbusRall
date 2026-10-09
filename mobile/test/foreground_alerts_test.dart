// ภาพรวม: ทดสอบว่าเตือนเฉพาะเหตุการณ์ใหม่ ไม่แจ้งประวัติครั้งแรกหรือผลเก่าซ้ำ
import 'package:flutter_test/flutter_test.dart';
import 'package:smartline_app/models.dart';

void main() {
  test(
    'foreground alerts ignore initial history, duplicates and stale backfill',
    () {
      final tracker = ForegroundRainAlerts();
      final now = DateTime.fromMillisecondsSinceEpoch(1000000);
      Json row(String key, int at) => {
        'key': key,
        'createdAt': at,
        'p_rain': .8,
      };
      final first = row('rain-1', 1000000);
      expect(tracker.observe([first], now), isNull);
      final second = row('rain-2', 1000000);
      expect(tracker.observe([second, first], now)?['key'], 'rain-2');
      expect(tracker.observe([second, first], now), isNull);
      expect(tracker.observe([row('rain-0', 1), second, first], now), isNull);
      tracker.reset();
      expect(tracker.observe([second], now), isNull);
      tracker.reset();
      expect(tracker.observe([], now), isNull);
      expect(tracker.observe([first], now)?['key'], 'rain-1');
      expect(alertBody(first), contains('80%'));
    },
  );
}
