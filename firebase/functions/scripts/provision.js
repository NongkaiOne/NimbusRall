// ภาพรวม: ตัวจับคู่บอร์ดของ Blaze เดิม ไม่ใช้กับ Spark; ปัจจุบันใช้ scripts/configure_firebase_device.py
"use strict";
const {initializeApp,applicationDefault}=require("firebase-admin/app");
const {getAuth}=require("firebase-admin/auth");
const {getDatabase}=require("firebase-admin/database");
const {randomBytes,createHash}=require("node:crypto");
const {mkdirSync,writeFileSync,readFileSync,existsSync}=require("node:fs");
const {resolve}=require("node:path");
const [projectId,databaseURL,ownerUid,deviceId]=process.argv.slice(2);
if(!/^[a-z][a-z0-9-]+$/.test(projectId||"") || !/^https:\/\/[^/]+\.(firebasedatabase\.app|firebaseio\.com)\/?$/.test(databaseURL||"") || !ownerUid || !/^sl-[a-f0-9]{6}$/.test(deviceId||"")) {
  console.error("Usage: npm run provision -- PROJECT_ID https://DATABASE.firebasedatabase.app OWNER_UID sl-xxxxxx");process.exit(1);
}
initializeApp({credential:applicationDefault(),projectId,databaseURL});
(async()=>{
  await getAuth().getUser(ownerUid);
  const directory=resolve(__dirname,"../../.private");mkdirSync(directory,{recursive:true});
  const path=resolve(directory,`${deviceId}.json`);
  const endpoint=`https://asia-southeast1-${projectId}.cloudfunctions.net/deviceSync`;
  let local;
  if(existsSync(path)){
    local=JSON.parse(readFileSync(path,"utf8"));
    if(local.deviceId!==deviceId || local.endpoint!==endpoint || !/^[a-f0-9]{64}$/.test(local.deviceSecret||"")) {
      throw new Error("Existing local credential does not match this project/device");
    }
  } else {
    local={deviceId,deviceSecret:randomBytes(32).toString("hex"),endpoint};
    // Keep the credential if a network failure interrupts provisioning; a retry can resume.
    writeFileSync(path,JSON.stringify(local,null,2),{mode:0o600,flag:"wx"});
  }
  const record={ownerUid,secretHash:createHash("sha256").update(local.deviceSecret).digest("hex")};
  const result=await getDatabase().ref(`access/${deviceId}`).transaction(old=>{
    if(old && (old.ownerUid!==record.ownerUid || old.secretHash!==record.secretHash))return;
    return old || record;
  });
  if(!result.committed) throw new Error("Device has another owner or credential. Nothing was overwritten.");
  await getDatabase().ref(`users/${ownerUid}/devices/${deviceId}`).update({name:"Smart clothesline",registeredAt:Date.now()});
  console.log(`Device registered. Private configuration: .private/${deviceId}.json`);
  process.exit(0);
})().catch(error=>{console.error(error.message);process.exit(1);});
