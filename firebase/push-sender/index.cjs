// ภาพรวม: ตัวส่ง Push สำหรับ Spark ต้องรันบนคอมหรือเซิร์ฟเวอร์ที่เปิดอยู่ ใช้บัญชีบริการเฉพาะฝั่งนี้
const {initializeApp, applicationDefault} = require('firebase-admin/app');
const {getDatabase} = require('firebase-admin/database');
const {getMessaging} = require('firebase-admin/messaging');
const {setTimeout: pause} = require('node:timers/promises');
const {deliver, MAX_AGE_MS} = require('./delivery.cjs');
// ตรวจ Firebase HTTPS และ URL เว็บ HTTPS ที่จะเปิดจากแจ้งเตือน
function settings(env) {
  const databaseURL = new URL(env.FIREBASE_DATABASE_URL || '');
  const appUrl = new URL(env.PUSH_APP_URL || '');
  if (databaseURL.protocol !== 'https:' || !/\.(firebaseio\.com|firebasedatabase\.app)$/.test(databaseURL.hostname)) {
    throw new Error('Set FIREBASE_DATABASE_URL to your HTTPS Realtime Database URL.');
  }
  if (appUrl.protocol !== 'https:' || appUrl.username || appUrl.password) {
    throw new Error('Set PUSH_APP_URL to the HTTPS address of your deployed app.');
  }
  if (env.FIREBASE_DATABASE_EMULATOR_HOST || env.FIREBASE_AUTH_EMULATOR_HOST) {
    throw new Error('Unset emulator environment variables before running a real push sender.');
  }
  return {databaseURL: databaseURL.href, appUrl: appUrl.href};
}
// เปิด Admin SDK แล้ววนอ่านเจ้าของ/token/alert ล่าสุดทุก 15 วินาที
async function main() {
  const {databaseURL, appUrl} = settings(process.env);
  const app = initializeApp({credential: applicationDefault(), databaseURL});
  const db = getDatabase(app), messaging = getMessaging(app);
  const report = (status, id) => console.log(`[push] ${id}: ${status}`);
  let stopped = false, lastCleanup = 0;
  process.on('SIGINT', () => { stopped = true; });
  process.on('SIGTERM', () => { stopped = true; });
  console.log('Push sender running. Keep this computer awake and connected. Ctrl+C to stop.');
  while (!stopped) {
    try {
      const access = (await db.ref('access').get()).val() || {};
      for (const [deviceId, pair] of Object.entries(access)) {
        if (stopped) break;
        if (!/^sl-[0-9a-f]{6}$/.test(deviceId) || typeof pair?.ownerUid !== 'string') continue;
        const ownerUid = pair.ownerUid;
        const tokens = (await db.ref(`users/${ownerUid}/pushTokens`).get()).val();
        if (!tokens) continue;
        const alerts = await db.ref(`devices/${deviceId}/alerts`)
          .orderByChild('createdAt').startAt(Date.now() - MAX_AGE_MS).limitToLast(100).get();
        for (const alert of Object.entries(alerts.val() || {})) {
          await deliver({db, messaging, deviceId, ownerUid, key: alert[0], alert: alert[1],
            tokens, appUrl, report});
        }
      }
      // ลบใบรับการส่งเก่ากว่า 1 วันเป็นรอบ เพื่อลดข้อมูลสะสม
      if (Date.now() - lastCleanup > 3600000) {
        const old = await db.ref('pushDelivery').orderByChild('createdAt')
          .endAt(Date.now() - 86400000).limitToFirst(1000).get();
        const removal = {};
        old.forEach(child => { removal[child.key] = null; });
        if (Object.keys(removal).length) await db.ref('pushDelivery').update(removal);
        lastCleanup = Date.now();
      }
    } catch (error) {
      // Never log a service-account credential, token, URL with auth, or payload.
      console.error(`[push] ${error.code || 'connection-or-configuration-error'}; retrying`);
    }
    if (process.argv.includes('--once')) break;
    if (!stopped) await pause(15000);
  }
  await app.delete();
}
if (require.main === module) main().catch(() => {
  console.error('Cannot start: check FIREBASE_DATABASE_URL, PUSH_APP_URL (HTTPS), and GOOGLE_APPLICATION_CREDENTIALS.');
  process.exitCode = 1;
});
module.exports = {settings};
