// ภาพรวม: แปลง Web config/Public VAPID key ให้ Flutter และ Service Worker ใช้โปรเจกต์เดียวกัน
const {readFileSync, writeFileSync, mkdirSync} = require('node:fs');
const {resolve} = require('node:path');
const fields = {
  apiKey: 'FIREBASE_API_KEY', appId: 'FIREBASE_APP_ID',
  messagingSenderId: 'FIREBASE_MESSAGING_SENDER_ID', projectId: 'FIREBASE_PROJECT_ID',
  authDomain: 'FIREBASE_AUTH_DOMAIN', databaseURL: 'FIREBASE_DATABASE_URL',
  vapidKey: 'FIREBASE_VAPID_KEY',
};
// เลือกเฉพาะค่าที่อนุญาต ไม่คัดลอก private key ที่อาจปะปนมากับ config
function configure(config) {
  const defines = {}, firebase = {};
  for (const [key, define] of Object.entries(fields)) {
    const value = config[key];
    if (typeof value !== 'string' || !value.trim() || value.includes('YOUR_')) {
      throw new Error(`Fill in ${key} from Firebase Console.`);
    }
    defines[define] = value.trim();
    if (key !== 'vapidKey') firebase[key] = value.trim();
  }
  if (!/^[A-Za-z0-9_-]{80,100}$/.test(config.vapidKey)) throw new Error('Use the public VAPID key.');
  const url = new URL(config.databaseURL);
  if (url.protocol !== 'https:' || !/\.(firebaseio\.com|firebasedatabase\.app)$/.test(url.hostname)) {
    throw new Error('Use an HTTPS Realtime Database URL.');
  }
  return {defines, firebase};
}
if (require.main === module) {
  try {
    if (!process.argv[2]) throw new Error('Usage: node scripts/configure_web_push.cjs firebase/.private/web-push.json');
    const {defines, firebase} = configure(JSON.parse(readFileSync(process.argv[2], 'utf8')));
    const root = resolve(__dirname, '..');
    mkdirSync(resolve(root, 'firebase/.private'), {recursive: true});
    writeFileSync(resolve(root, 'firebase/.private/web-defines.json'), JSON.stringify(defines, null, 2)+'\n');
    writeFileSync(resolve(root, 'mobile/web/firebase-push-config.js'),
      'self.NIMBUS_FIREBASE_CONFIG = '+JSON.stringify(firebase, null, 2)+';\n');
    console.log('Created firebase/.private/web-defines.json and mobile/web/firebase-push-config.js');
  } catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = {configure};
