const {test,after}=require("node:test");
const assert=require("node:assert/strict");
const {randomBytes,createHash}=require("node:crypto");
const {initializeTestEnvironment}=require("@firebase/rules-unit-testing");
const {initializeApp,deleteApp}=require("firebase/app");
const {getAuth,connectAuthEmulator,createUserWithEmailAndPassword}=require("firebase/auth");
const {getFunctions,connectFunctionsEmulator,httpsCallable}=require("firebase/functions");
const {ref,set,get}=require("firebase/database");
let env,app;
const databaseURL=JSON.parse(process.env.FIREBASE_CONFIG||"{}").databaseURL||"https://demo-smartline.firebaseio.com";
after(async()=>{await env?.cleanup();if(app)await deleteApp(app);});
test("device HTTPS sync, owner commands, acknowledgement and rain crossing",async()=>{
 env=await initializeTestEnvironment({projectId:"demo-smartline",database:{host:"127.0.0.1",port:9000}});
 app=initializeApp({projectId:"demo-smartline",apiKey:"demo-key",authDomain:"demo-smartline.firebaseapp.com"});
 const auth=getAuth(app);connectAuthEmulator(auth,"http://127.0.0.1:9099",{disableWarnings:true});
 const user=(await createUserWithEmailAndPassword(auth,"owner@example.com","test-password-123")).user;
 const fn=getFunctions(app,"asia-southeast1");connectFunctionsEmulator(fn,"127.0.0.1",5001);
 const command=httpsCallable(fn,"issueCommand");
 const secret=randomBytes(32).toString("hex"),deviceId="sl-aabbcc";
 await env.withSecurityRulesDisabled(c=>set(ref(c.database(databaseURL),`access/${deviceId}`),
  {ownerUid:user.uid,secretHash:createHash("sha256").update(secret).digest("hex")}));
 const status={temp:30,hum:80,press:1000,lux:120,p_rain:.2,seq:1,uptime:10,t:1900000000,
  measurement_age_s:0,retract_threshold:.7,extend_threshold:.2,rssi:-51,sensor_err:false,
  fallback:false,servo_enabled:false,line:"UNKNOWN",mode:"MANUAL",model:"test",fw:"test"};
 const log={seq:1,epoch:1900000000,uptime:10,p_rain:.2,flags:0,temp:30,hum:80,press:1000,lux:120,line:"UNKNOWN"};
 const endpoint="http://127.0.0.1:5001/demo-smartline/asia-southeast1/deviceSync";
 async function sync(telemetry=status,extra={},key=secret) {
  return fetch(endpoint,{method:"POST",headers:{"Content-Type":"application/json",Authorization:`Bearer ${key}`},
   body:JSON.stringify({deviceId,telemetry,logs:[log],...extra})});
 }
 assert.equal((await sync(status,{},"a".repeat(64))).status,401);
 let response=await sync();assert.equal(response.status,200);assert.equal((await response.json()).logAck,1);
 await assert.rejects(command({deviceId,action:"EXTEND"}),e=>e.code==="functions/failed-precondition");
 await assert.rejects(command({deviceId:"sl-dddddd",action:"MEASURE"}),e=>e.code==="functions/permission-denied");
 const issued=(await command({deviceId,action:"MEASURE"})).data;
 await assert.rejects(command({deviceId,action:"MEASURE"}),e=>e.code==="functions/aborted");
 response=await sync();assert.equal((await response.json()).command.id,issued.id);
 response=await sync(status,{ack:{id:issued.id,ok:true,msg:"accepted"}});
 assert.equal((await response.json()).command,null);
 let ackTime;
 await env.withSecurityRulesDisabled(async c=>{
  const value=(await get(ref(c.database(databaseURL),`devices/${deviceId}/control`))).val();
  assert.equal(value.status,"accepted");ackTime=value.ack.receivedAt;
  assert.equal((await get(ref(c.database(databaseURL),`devices/${deviceId}/logs/0000000001`))).val().temp,30);
 });
 await sync(status,{ack:{id:issued.id,ok:true,msg:"accepted"}});
 await env.withSecurityRulesDisabled(async c=>{
  assert.equal((await get(ref(c.database(databaseURL),`devices/${deviceId}/control/ack/receivedAt`))).val(),ackTime);
 });
 await sync({...status,seq:2,p_rain:.8});
 let alerts={};
 for(let i=0;i<30;i++){
  await env.withSecurityRulesDisabled(async c=>{alerts=(await get(ref(c.database(databaseURL),`devices/${deviceId}/alerts`))).val()||{};});
  if(alerts["rain-2"])break;
  await new Promise(r=>setTimeout(r,200));
 }
 assert.ok(alerts["rain-2"],"threshold crossing must create a durable alert");
 await sync({...status,seq:3,p_rain:.9});
 await new Promise(r=>setTimeout(r,400));
 await env.withSecurityRulesDisabled(async c=>{
  assert.equal(Object.keys((await get(ref(c.database(databaseURL),`devices/${deviceId}/alerts`))).val()).length,1);
 });
});
