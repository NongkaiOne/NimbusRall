const {test,before,after,beforeEach}=require("node:test");
const {readFileSync}=require("node:fs");
const {initializeTestEnvironment,assertSucceeds,assertFails}=require("@firebase/rules-unit-testing");
const {ref,get,set}=require("firebase/database");
let env;
before(async()=>{
 env=await initializeTestEnvironment({projectId:"demo-smartline",database:{host:"127.0.0.1",port:9000,rules:readFileSync("database.rules.json","utf8")}});
});
after(async()=>{await env?.cleanup();});
beforeEach(async()=>{
 await env.clearDatabase();
 await env.withSecurityRulesDisabled(async(context)=>{
  await set(ref(context.database()),{
   access:{"sl-aaaaaa":{ownerUid:"alice",secretHash:"private"},"sl-bbbbbb":{ownerUid:"bob"}},
   devices:{"sl-aaaaaa":{telemetry:{temp:30},logs:{"0000000001":{seq:1}}},"sl-bbbbbb":{telemetry:{temp:20}}},
   users:{alice:{devices:{"sl-aaaaaa":{name:"Alice device"}}}}
  });
 });
});
test("owner can read own telemetry/logs but strangers and unauthenticated users cannot",async()=>{
 const alice=env.authenticatedContext("alice").database();
 await assertSucceeds(get(ref(alice,"devices/sl-aaaaaa/telemetry")));
 await assertSucceeds(get(ref(alice,"devices/sl-aaaaaa/logs")));
 await assertFails(get(ref(alice,"devices/sl-bbbbbb")));
 await assertFails(get(ref(env.unauthenticatedContext().database(),"devices/sl-aaaaaa")));
 await assertFails(get(ref(alice,"access/sl-aaaaaa")));
});
test("client cannot forge ownership, telemetry or actuator commands",async()=>{
 const alice=env.authenticatedContext("alice").database();
 for(const path of ["access/sl-aaaaaa","users/alice/devices/sl-bbbbbb","devices/sl-aaaaaa/telemetry","devices/sl-aaaaaa/control"]) {
  await assertFails(set(ref(alice,path),{ownerUid:"alice",action:"EXTEND"}));
 }
});
test("token registration is isolated by user and validates its shape",async()=>{
 const alice=env.authenticatedContext("alice").database();
 await assertSucceeds(set(ref(alice,"users/alice/pushTokens/token1"),{token:"a-valid-push-token",createdAt:100}));
 await assertFails(set(ref(alice,"users/bob/pushTokens/token1"),{token:"a-valid-push-token",createdAt:100}));
 await assertFails(get(ref(alice,"users/bob/pushTokens")));
 await assertFails(set(ref(alice,"users/alice/pushTokens/token2"),{token:"a-valid-push-token",createdAt:100,ownerUid:"bob"}));
 await assertFails(set(ref(alice,"users/alice/pushTokens/token2"),{token:"short",createdAt:100}));
 await assertSucceeds(set(ref(alice,"users/alice/pushTokens/token1"),null));
});
