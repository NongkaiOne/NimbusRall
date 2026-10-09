// ภาพรวม: ทดสอบสิทธิ์ Database ใน Emulator เช่น เจ้าของ/บอร์ด คำสั่งหมดอายุ และ Push token ส่วนตัว
const {test,before,after,beforeEach}=require("node:test");
const {readFileSync}=require("node:fs");
const {initializeTestEnvironment,assertSucceeds,assertFails}=require("@firebase/rules-unit-testing");
const {ref,get,set,update,serverTimestamp}=require("firebase/database");
let env;
const id="sl-aaaaaa",path="devices/"+id;
const telemetry=(extra={})=>({seq:1,receivedAt:serverTimestamp(),servo_enabled:false,sensor_err:false,p_rain:.7,measurement_age_s:1,line:"UNKNOWN",mode:"MANUAL",...extra});
const command=(key="a".repeat(20),action="MEASURE")=>({id:key,action,requestedBy:"owner",createdAt:serverTimestamp(),expiresAt:Date.now()+25000,status:"pending"});
const db=(uid)=>uid?env.authenticatedContext(uid).database():env.unauthenticatedContext().database();
before(async()=>{env=await initializeTestEnvironment({projectId:"demo-smartline",database:{host:"127.0.0.1",port:9000,rules:readFileSync("database.rules.json","utf8")}});});
after(async()=>{await env?.cleanup();});
beforeEach(async()=>{
 await env.clearDatabase();
 await env.withSecurityRulesDisabled(c=>set(ref(c.database()),{
  access:{[id]:{ownerUid:"owner",deviceUid:"device"},"sl-bbbbbb":{ownerUid:"other",deviceUid:"other-device"}},
  users:{owner:{devices:{[id]:{name:"test"}}}}
 }));
 await set(ref(db("device"),path+"/telemetry"),telemetry());
});
test("only owner and assigned device can read; pairing and credentials remain private",async()=>{
 await assertSucceeds(get(ref(db("owner"),path)));
 await assertSucceeds(get(ref(db("device"),path+"/control")));
 for(const user of [null,"other","other-device"])await assertFails(get(ref(db(user),path)));
 for(const user of ["owner","device"]) {
  await assertFails(get(ref(db(user),"access/"+id)));
  await assertFails(set(ref(db(user),"access/"+id),{ownerUid:user,deviceUid:user}));
 }
 await assertFails(set(ref(db("owner"),"users/owner/devices/sl-bbbbbb"),{name:"takeover"}));
});
test("only the assigned device can write validated telemetry and logs",async()=>{
 await assertFails(set(ref(db("owner"),path+"/telemetry"),telemetry()));
 await assertFails(set(ref(db("other-device"),path+"/telemetry"),telemetry()));
 await assertFails(set(ref(db("device"),path+"/telemetry"),telemetry({receivedAt:1000})));
 await assertFails(set(ref(db("device"),path+"/telemetry"),telemetry({p_rain:2})));
 const record={seq:1,epoch:0,uptime:1,p_rain:.7,flags:0,line:"UNKNOWN",receivedAt:serverTimestamp(),temp:30,hum:80,press:1000,lux:5};
 await assertSucceeds(update(ref(db("device"),path),{"telemetry":telemetry(),"logs/0000000001":record,"alerts/rain-0000000001":{seq:1,p_rain:.7,createdAt:Date.now(),fallback:false}}));
 await assertFails(set(ref(db("owner"),path+"/alerts/rain-0000000002"),{seq:2,p_rain:.9,createdAt:Date.now(),fallback:false}));
 await assertFails(set(ref(db("device"),path+"/logs/0000000002"),{...record,hum:101}));
});
test("owner can MEASURE with servo disabled but cannot move, forge ACK or use unknown actions",async()=>{
 for(const action of ["EXTEND","STOP","MODE AUTO","DELLOG"])await assertFails(set(ref(db("owner"),path+"/control"),command("b".repeat(20),action)));
 await assertFails(set(ref(db("device"),path+"/control"),command()));
 await assertFails(set(ref(db("other"),path+"/control"),command()));
 await assertSucceeds(set(ref(db("owner"),path+"/control"),command()));
 await assertFails(set(ref(db("owner"),path+"/acks/"+"a".repeat(20)),{id:"a".repeat(20),ok:true,msg:"fake",receivedAt:serverTimestamp()}));
});
test("commands reject stale devices, stale/future requests, spoofed UID and extra fields",async()=>{
 for(const extra of [{expiresAt:Date.now()-1},{expiresAt:Date.now()+60000},{createdAt:100},{requestedBy:"other"},{surprise:true}]) {
  await assertFails(set(ref(db("owner"),path+"/control"),{...command(),...extra}));
 }
 await env.withSecurityRulesDisabled(c=>update(ref(c.database(),path+"/telemetry"),{receivedAt:Date.now()-31000}));
 await assertFails(set(ref(db("owner"),path+"/control"),command()));
});
test("one command at a time; STOP supersedes; device ACK cannot acknowledge a replaced command",async()=>{
 await set(ref(db("device"),path+"/telemetry"),telemetry({servo_enabled:true}));
 await assertSucceeds(set(ref(db("owner"),path+"/control"),command("a".repeat(20),"EXTEND")));
 await assertFails(set(ref(db("owner"),path+"/control"),command("b".repeat(20),"RETRACT")));
 await assertSucceeds(set(ref(db("owner"),path+"/control"),command("c".repeat(20),"STOP")));
 await assertFails(set(ref(db("device"),path+"/acks/"+"a".repeat(20)),{id:"a".repeat(20),ok:true,msg:"old",receivedAt:serverTimestamp()}));
 await assertSucceeds(set(ref(db("device"),path+"/acks/"+"c".repeat(20)),{id:"c".repeat(20),ok:true,msg:"stopped",receivedAt:serverTimestamp()}));
 await assertSucceeds(set(ref(db("owner"),path+"/control"),command("d".repeat(20),"RETRACT")));
 await assertFails(set(ref(db("owner"),path+"/control"),command("c".repeat(20),"STOP")));
});

test("push tokens are private to their owner; devices and strangers cannot register or read them",async()=>{
 const key='a'.repeat(64), path='users/owner/pushTokens/'+key;
 const token={token:'test-token-long-enough-12345',platform:'web',updatedAt:serverTimestamp()};
 await assertSucceeds(set(ref(db('owner'),path),token));
 await assertSucceeds(get(ref(db('owner'),path)));
 for(const user of [null,'other','device']) {
  await assertFails(get(ref(db(user),path)));
  await assertFails(set(ref(db(user),path),token));
 }
 await assertFails(set(ref(db('device'),'users/device/pushTokens/'+key),token));
 for(const extra of [{updatedAt:0},{token:'short'},{platform:'bad'},{secret:true}])
  await assertFails(set(ref(db('owner'),path),{...token,...extra}));
 await assertFails(set(ref(db('owner'),'users/owner/pushTokens/bad-key'),token));
 await assertFails(get(ref(db('owner'),'pushDelivery')));
 await assertFails(set(ref(db('owner'),'pushDelivery/forged'),{done:true}));
 await assertSucceeds(set(ref(db('owner'),path),null));
});
