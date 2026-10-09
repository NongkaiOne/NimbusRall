// ภาพรวม: ตรวจความสดของ alert สร้างข้อความแต่ละแพลตฟอร์ม และจำผลราย token เพื่อลดการส่งซ้ำ
const {createHash, randomUUID} = require('node:crypto');
const MAX_AGE_MS = 5 * 60 * 1000;
const digest = value => createHash('sha256').update(value).digest('hex');
// รับเฉพาะค่าฝนถูกช่วงและเหตุการณ์ไม่เก่าเกิน 5 นาที ไม่เตือนย้อนหลังเมื่อเน็ตกลับ
function fresh(alert, now) {
  return Number.isFinite(alert?.p_rain) && alert.p_rain >= 0 && alert.p_rain <= 1 &&
    Number.isFinite(alert?.createdAt) && alert.createdAt > 0 &&
    now - alert.createdAt >= -5000 && now - alert.createdAt <= MAX_AGE_MS;
}
// สร้างข้อความร่วม พร้อมอายุ เสียง และตัวเลือก Android/APNs/Web Push
function payload(token, deviceId, key, alert, appUrl, now) {
  const ttl = Math.max(1, Math.ceil((alert.createdAt + MAX_AGE_MS - now) / 1000));
  return {
    token,
    notification: {title: 'NimbusRail: มีแนวโน้มฝนตก',
      body: `โอกาสฝนตก ${Math.round(alert.p_rain * 100)}% ตรวจสอบราวตากผ้า (${deviceId})`},
    data: {deviceId, alertId: key},
    android: {priority: 'high', ttl: ttl * 1000,
      notification: {tag: `${deviceId}-${key}`, sound: 'default'}},
    apns: {headers: {'apns-push-type': 'alert', 'apns-priority': '10',
      'apns-expiration': String(Math.floor(now / 1000) + ttl)},
      payload: {aps: {sound: 'default'}}},
    webpush: {headers: {TTL: String(ttl)},
      notification: {tag: `${deviceId}-${key}`}, fcmOptions: {link: appUrl}},
  };
}
// A lease prevents simultaneous senders; persisted per-token receipts survive
// restarts. A crash after FCM accepts but before the receipt is saved can repeat
// delivery. Notification tags reduce duplicates; exactly-once is not promised.
// จับสิทธิ์ส่งชั่วคราว (lease) ต่อเหตุการณ์/ผู้รับ ตรวจเจ้าของซ้ำ ส่ง แล้วจำผล
async function deliver({db, messaging, deviceId, key, alert, ownerUid, tokens,
  appUrl, clock = Date.now, report = () => {}}) {
  if (!fresh(alert, clock())) return;
  for (const [tokenKey, entry] of Object.entries(tokens || {})) {
    if (typeof entry?.token !== 'string' || tokenKey !== digest(entry.token)) continue;
    const receipt = db.ref(`pushDelivery/${digest(`${ownerUid}/${deviceId}/${key}/${tokenKey}`)}`);
    const leaseId = randomUUID();
    const claimed = await receipt.transaction(old => {
      if (old?.done || old?.leaseUntil > clock()) return;
      return {leaseId, leaseUntil: clock() + 120000, createdAt: alert.createdAt};
    });
    if (!claimed.committed) continue;
    try {
      // Recheck pairing and token immediately before delivery, including logout.
      const [owner, current] = await Promise.all([
        db.ref(`access/${deviceId}/ownerUid`).get(),
        db.ref(`users/${ownerUid}/pushTokens/${tokenKey}`).get(),
      ]);
      if (owner.val() !== ownerUid || current.val()?.token !== entry.token || !fresh(alert, clock())) {
        await receipt.transaction(old => old?.leaseId === leaseId ? null : undefined);
        continue;
      }
      await messaging.send(payload(entry.token, deviceId, key, alert, appUrl, clock()));
      await receipt.transaction(old => old?.leaseId === leaseId
        ? {...old, done: true, leaseUntil: 0} : undefined);
      report('sent', deviceId);
    } catch (error) {
      const invalid = ['messaging/registration-token-not-registered',
        'messaging/invalid-registration-token'].includes(error.code);
      if (invalid) {
        await db.ref(`users/${ownerUid}/pushTokens/${tokenKey}`).transaction(old =>
          old?.token === entry.token ? null : undefined);
      }
      // Invalid credentials / network failures keep their lease for bounded retry.
      if (invalid) await receipt.transaction(old => old?.leaseId === leaseId
        ? {...old, done: true, leaseUntil: 0} : undefined);
      report(error.code || 'push/send-failed', deviceId);
    }
  }
}
module.exports = {fresh, payload, deliver, digest, MAX_AGE_MS};
