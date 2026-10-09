// ภาพรวม: ทดสอบ Auth และ REST Database ครบเส้นทางด้วย Emulator รวมข้อมูล คำสั่ง และ ACK
const {test,after}=require("node:test");
const assert=require("node:assert/strict");
const {readFileSync}=require("node:fs");
const {initializeTestEnvironment}=require("@firebase/rules-unit-testing");
const {ref,set,get}=require("firebase/database");
let env;
after(async()=>{await env?.cleanup();});
test("Spark REST: device sign-in, token refresh, rules-protected sync, command and ACK without Functions",async()=>{
 env=await initializeTestEnvironment({projectId:"demo-smartline",database:{host:"127.0.0.1",port:9000,rules:readFileSync("database.rules.json","utf8")}});
 const authRoot="http://127.0.0.1:9099/identitytoolkit.googleapis.com/v1/accounts:";
 async function auth(method,email,password){
  const response=await fetch(authRoot+method+"?key=demo-key",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({email,password,returnSecureToken:true})});
  assert.equal(response.status,200);return response.json();
 }
 const password="test-only-password-123",owner=await auth("signUp","owner@example.com",password);
 const device=await auth("signUp","device@example.com",password),stranger=await auth("signUp","stranger@example.com",password);
 const signed=await auth("signInWithPassword","device@example.com",password);assert.equal(signed.localId,device.localId);
 const refreshedResponse=await fetch("http://127.0.0.1:9099/securetoken.googleapis.com/v1/token?key=demo-key",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams({grant_type:"refresh_token",refresh_token:signed.refreshToken})});
 assert.equal(refreshedResponse.status,200);const refreshed=await refreshedResponse.json();assert.equal(refreshed.user_id,device.localId);
 const id="sl-aabbcc";
 await env.withSecurityRulesDisabled(c=>set(ref(c.database(),"access/"+id),{ownerUid:owner.localId,deviceUid:device.localId}));
 async function rest(path,method,token,body){
  return fetch("http://127.0.0.1:9000/devices/"+id+path+".json?ns=demo-smartline&auth="+token,{method,headers:{"Content-Type":"application/json"},...(body===undefined?{}:{body:JSON.stringify(body)})});
 }
 const now={".sv":"timestamp"};
 const telemetry={seq:1,receivedAt:now,servo_enabled:false,sensor_err:false,p_rain:.8,measurement_age_s:0,line:"UNKNOWN",mode:"MANUAL"};
 const changes={telemetry,"logs/0000000001":{seq:1,epoch:1,uptime:1,p_rain:.8,flags:0,line:"UNKNOWN",receivedAt:now,temp:30,hum:70,press:1000,lux:12},
  "alerts/rain-0000000001":{seq:1,p_rain:.8,createdAt:Date.now(),fallback:false}};
 assert.equal((await rest("","PATCH",refreshed.id_token,changes)).status,200);
 assert.equal((await rest("/telemetry","GET",owner.idToken)).status,200);
 assert.equal((await rest("/telemetry","GET",stranger.idToken)).status,401);
 assert.equal((await rest("/telemetry","PUT",owner.idToken,telemetry)).status,401);
 const command={id:"a".repeat(20),action:"MEASURE",requestedBy:owner.localId,createdAt:now,expiresAt:Date.now()+25000,status:"pending"};
 assert.equal((await rest("/control","PUT",owner.idToken,{...command,action:"EXTEND"})).status,401);
 assert.equal((await rest("/control","PUT",owner.idToken,command)).status,200);
 assert.equal((await (await rest("/control","GET",refreshed.id_token)).json()).id,command.id);
 const ack={id:command.id,ok:true,msg:"accepted",receivedAt:now};
 assert.equal((await rest("/acks/"+command.id,"PUT",refreshed.id_token,ack)).status,200);
 assert.equal((await (await rest("/acks/"+command.id,"GET",owner.idToken)).json()).ok,true);
 // Re-sending the same batch after a lost HTTP response replaces keys, without duplicate logs/alerts.
 assert.equal((await rest("","PATCH",refreshed.id_token,changes)).status,200);
 for(const path of ["/logs","/alerts"])assert.equal(Object.keys(await (await rest(path,"GET",owner.idToken)).json()).length,1);
 await env.withSecurityRulesDisabled(async c=>{
  const stored=(await get(ref(c.database(),"devices/"+id+"/telemetry/receivedAt"))).val();
  assert.equal(typeof stored,"number");assert.ok(Math.abs(stored-Date.now())<5000);
 });
});
