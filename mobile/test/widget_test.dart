import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:smartline_app/main.dart';
import 'package:smartline_app/models.dart';
import 'package:smartline_app/repository.dart';

class LockedRepository extends DemoRepository {
  LockedRepository({required this.enabled, required this.stale});
  final bool enabled, stale;
  @override
  Stream<Json> state(String id) => Stream.value({
    'receivedAt': DateTime.now().millisecondsSinceEpoch - (stale ? 60000 : 0),
    'servo_enabled': enabled,
    'sensor_err': true,
    'mode': 'MANUAL',
  });
}

void main() {
  test(
    'stale or disabled devices cannot move; missing readings stay unknown',
    () {
      final now = DateTime.fromMillisecondsSinceEpoch(50000);
      expect(const DeviceState({}).canMove(now), isFalse);
      expect(
        const DeviceState({'receivedAt': 1000, 'servo_enabled': true})
            .canMove(now),
        isFalse,
      );
      expect(
        const DeviceState({'receivedAt': 49999, 'servo_enabled': false})
            .canMove(now),
        isFalse,
      );
      expect(
        const DeviceState({'receivedAt': 49999, 'servo_enabled': true})
            .canMove(now),
        isTrue,
      );
      expect(numberText(null), '—');
      expect(
        commandText({'status': 'accepted'}, now),
        contains('ดูผลที่สถานะราว'),
      );
      expect(
        commandText({'status': 'pending', 'expiresAt': 49999}, now),
        contains('หมดเวลา'),
      );
    },
  );
  testWidgets('demo dashboard, history detail and manual commands work', (
    tester,
  ) async {
    final repo = DemoRepository();
    await tester.pumpWidget(SmartLineApp(repository: repo));
    await tester.pumpAndSettle();
    expect(find.text('78%'), findsOneWidget);
    expect(find.textContaining('ข้อมูลจำลอง'), findsOneWidget);
    await tester.tap(find.text('บันทึก'));
    await tester.pumpAndSettle();
    await tester.tap(find.byType(ListTile).first);
    await tester.pumpAndSettle();
    expect(find.textContaining('ความกดอากาศ:'), findsOneWidget);
    Navigator.of(tester.element(find.textContaining('ความกดอากาศ:'))).pop();
    await tester.pumpAndSettle();
    await tester.tap(find.text('ควบคุม'));
    await tester.pumpAndSettle();
    await tester.scrollUntilVisible(
      find.text('ตากผ้า'),
      200,
      scrollable: find.byType(Scrollable).first,
    );
    await tester.tap(find.text('ตากผ้า'));
    await tester.pumpAndSettle(const Duration(milliseconds: 500));
    expect((await repo.state('sl-demo').first)['line'], 'EXTENDED');
    await tester.pumpWidget(const SizedBox());
    await repo.dispose();
  });
  for (final settings in [(false, false), (true, true)]) {
    testWidgets('controls reject disabled servo or stale telemetry $settings', (
      tester,
    ) async {
      final repo = LockedRepository(enabled: settings.$1, stale: settings.$2);
      await tester.pumpWidget(SmartLineApp(repository: repo));
      await tester.pumpAndSettle();
      expect(find.text('ยังประเมินไม่ได้'), findsOneWidget);
      await tester.tap(find.text('ควบคุม'));
      await tester.pumpAndSettle();
      await tester.scrollUntilVisible(
        find.text('เก็บผ้า'),
        200,
        scrollable: find.byType(Scrollable).first,
      );
      final retract = tester.widget<FilledButton>(
        find.widgetWithText(FilledButton, 'เก็บผ้า'),
      );
      expect(retract.onPressed, isNull);
      await tester.scrollUntilVisible(
        find.text('ตากผ้า'),
        100,
        scrollable: find.byType(Scrollable).first,
      );
      final extend = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'ตากผ้า'),
      );
      expect(extend.onPressed, isNull);
      await tester.pumpWidget(const SizedBox());
      await repo.dispose();
    });
  }
  testWidgets('unconfigured app offers demo without Firebase login', (
    tester,
  ) async {
    await tester.pumpWidget(const SmartLineApp());
    expect(find.textContaining('ยังไม่ได้ตั้งค่า Firebase'), findsOneWidget);
    await tester.tap(find.text('ลองโหมดตัวอย่าง'));
    await tester.pumpAndSettle();
    expect(find.text('78%'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
