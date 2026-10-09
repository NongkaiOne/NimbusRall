// ภาพรวม: ทดสอบตัวส่งด้วย FCM จำลอง ครอบคลุมเหตุการณ์เก่า ส่งซ้ำ retry และเปลี่ยนเจ้าของ
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {deliver, fresh, payload, digest} = require('../push-sender/delivery.cjs');
const {configure} = require('../../scripts/configure_web_push.cjs');
function fixture() {
  let now = 1000000, sends = 0;
  const token = 'token-for-test-only-123456789';
  const tokenKey = digest(token), values = new Map();
  values.set('access/sl-aaaaaa/ownerUid', 'owner');
  values.set(`users/owner/pushTokens/${tokenKey}`, {token});
  const db = {ref(path) {return {
    async get() {return {val: () => values.get(path) ?? null};},
    async transaction(fn) {
      const value = fn(values.get(path) ?? null);
      if (value === undefined) return {committed: false};
      if (value === null) values.delete(path); else values.set(path, value);
      return {committed: true};
    },
  };}};
  const messaging = {async send(message) {assert.equal(message.token, token); sends++;}};
  const args = {db, messaging, deviceId:'sl-aaaaaa', key:'rain-0000000001',
    alert:{p_rain:.8, createdAt:now}, ownerUid:'owner', tokens:{[tokenKey]:{token}},
    appUrl:'https://demo.web.app/', clock:() => now};
  return {args, values, tokenKey, sends:() => sends, advance:ms => {now += ms;}};
}
test('freshness ignores stale, invalid and future readings; TTL bounds all platforms', () => {
  assert.equal(fresh({p_rain:.8,createdAt:1000}, 2000), true);
  for (const a of [{p_rain:2,createdAt:1000},{p_rain:.8,createdAt:0},
    {p_rain:.8,createdAt:500000},{p_rain:NaN,createdAt:1000}]) assert.equal(fresh(a, 2000), false);
  assert.equal(fresh({p_rain:.8,createdAt:1000}, 400000), false);
  const p = payload('secret', 'sl-aaaaaa', 'rain-1', {p_rain:.8,createdAt:1000},'https://demo.web.app/',2000);
  assert.equal(p.android.ttl,299000);
  assert.equal(p.webpush.headers.TTL,'299');
  assert.equal(p.apns.headers['apns-expiration'],'301');
  assert.equal(p.notification.title,'NimbusRail: มีแนวโน้มฝนตก');
});
test('persisted receipts suppress concurrent and repeated delivery across restarts', async () => {
  const f = fixture();
  await Promise.all([deliver(f.args),deliver(f.args)]);
  await deliver({...f.args});
  assert.equal(f.sends(),1);
});
test('logout and ownership changes stop delivery even after tokens were read', async () => {
  for (const reason of ['logout','transfer']) {
    const f=fixture();
    if(reason==='logout') f.values.delete(`users/owner/pushTokens/${f.tokenKey}`);
    else f.values.set('access/sl-aaaaaa/ownerUid','other');
    await deliver(f.args);
    assert.equal(f.sends(),0);
  }
});
test('transient failures retry after lease; permanent invalid tokens are removed', async () => {
  const f=fixture(); let attempts=0;
  f.args.messaging.send=async()=>{attempts++; if(attempts===1)throw Object.assign(new Error(),{code:'messaging/server-unavailable'});};
  await deliver(f.args); await deliver(f.args);
  assert.equal(attempts,1);
  f.advance(120001); await deliver(f.args); await deliver(f.args);
  assert.equal(attempts,2);
  const bad=fixture();
  bad.args.messaging.send=async()=>{throw Object.assign(new Error(),{code:'messaging/registration-token-not-registered'});};
  await deliver(bad.args);
  assert.equal(bad.values.has(`users/owner/pushTokens/${bad.tokenKey}`),false);
});
test('tokens with mismatched keys are ignored and expired retries stop', async () => {
  const f=fixture(); f.args.tokens={wrong:{token:'unknown'}}; await deliver(f.args);
  assert.equal(f.sends(),0);
  const old=fixture(); old.advance(300001); await deliver(old.args); assert.equal(old.sends(),0);
});
test('web setup shares public options between Flutter and service worker without copying secrets', () => {
  const c={apiKey:'test-api-key',appId:'1:123:web:abc',messagingSenderId:'123',projectId:'demo',
    authDomain:'demo.firebaseapp.com',databaseURL:'https://demo.firebaseio.com',vapidKey:'A'.repeat(87),private_key:'NEVER COPY'};
  const result=configure(c);
  assert.equal(result.defines.FIREBASE_PROJECT_ID,result.firebase.projectId);
  assert.equal(JSON.stringify(result).includes('NEVER COPY'),false);
  assert.throws(()=>configure({...c,vapidKey:'YOUR_KEY'}));
  assert.throws(()=>configure({...c,databaseURL:'http://demo.firebaseio.com'}));
});
