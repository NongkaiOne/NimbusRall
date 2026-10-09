"use strict";
const {initializeApp}=require("firebase-admin/app");
const {getDatabase}=require("firebase-admin/database");
const {getMessaging}=require("firebase-admin/messaging");
const {onRequest,onCall,HttpsError}=require("firebase-functions/v2/https");
const {onValueWritten,onValueCreated}=require("firebase-functions/v2/database");
const {setGlobalOptions}=require("firebase-functions/v2");
const {createHash,timingSafeEqual,randomUUID}=require("node:crypto");
const domain=require("./domain");
initializeApp();
setGlobalOptions({region:"asia-southeast1",maxInstances:5,timeoutSeconds:30});
const db=()=>getDatabase();
// The Database Emulator supports v2 triggers only in us-central1.
const databaseTriggerRegion=process.env.FUNCTIONS_EMULATOR==="true"?"us-central1":"asia-southeast1";

// Devices use a unique 256-bit bearer secret over verified HTTPS. Clients cannot read it.
exports.deviceSync=onRequest({invoker:"public",cors:false},async(req,res)=>{
  if(req.method!=="POST") return res.status(405).end();
  const body=req.body;
  if(!body || !domain.id(body.deviceId) || JSON.stringify(body).length>16000) return res.status(400).json({error:"Invalid payload"});
  const key=(req.get("Authorization")||"").replace(/^Bearer /,"");
  if(!/^[a-f0-9]{64}$/.test(key)) return res.status(401).end();
  const access=(await db().ref(`access/${body.deviceId}`).get()).val();
  const digest=createHash("sha256").update(key).digest();
  const expected=Buffer.from(access?.secretHash||"","hex");
  if(expected.length!==digest.length || !timingSafeEqual(expected,digest)) return res.status(401).end();
  try {
    const now=Date.now();
    const status=domain.telemetry(body.telemetry);
    if(!Array.isArray(body.logs)||body.logs.length>4) throw new Error("Invalid log batch");
    const logs=body.logs.map(domain.logRecord);
    const base=`devices/${body.deviceId}`;
    const updates={[`${base}/telemetry`]:{...status,receivedAt:now}};
    for(const record of logs) updates[`${base}/logs/${String(record.seq).padStart(10,"0")}`]={...record,receivedAt:now};
    await db().ref().update(updates);
    const controlRef=db().ref(`${base}/control`);
    if(body.ack) {
      const result=await controlRef.transaction(current=>{
        if(!domain.canAcknowledge(current,body.ack)) return current;
        if(current.ack) return current;
        return {...current,status:body.ack.ok?"accepted":"rejected",ack:{ok:body.ack.ok,msg:body.ack.msg,receivedAt:now}};
      });
      if(result.committed && domain.canAcknowledge(result.snapshot.val(),body.ack)) {
        const c=result.snapshot.val();
        await db().ref(`${base}/commandHistory/${c.id}`).set(c);
      }
    }
    const delivery=await controlRef.transaction(current=>{
      // A cold RTDB transaction can first see null. Returning undefined here
      // would abort without fetching a pending command from the server.
      if(!current || !["pending","delivered"].includes(current.status)) return current;
      if(current.expiresAt<=now) return {...current,status:"expired"};
      return {...current,status:"delivered",deliveredAt:current.deliveredAt||now};
    });
    const control=delivery.snapshot.val();
    const command=control && ["pending","delivered"].includes(control.status) && control.expiresAt>now ? control : null;
    return res.json({serverTime:now,logAck:logs.reduce((n,r)=>Math.max(n,r.seq),0),command});
  } catch(error) {
    // Never log bearer secrets or raw requests.
    console.error("deviceSync failed",error.message);
    return res.status(400).json({error:"Sync failed"});
  }
});

exports.issueCommand=onCall(async(request)=>{
  if(!request.auth) throw new HttpsError("unauthenticated","Please sign in");
  const {deviceId,action}=request.data||{};
  if(!domain.id(deviceId)) throw new HttpsError("invalid-argument","Invalid device ID");
  const access=(await db().ref(`access/${deviceId}`).get()).val();
  if(access?.ownerUid!==request.auth.uid) throw new HttpsError("permission-denied","You do not own this device");
  const state=(await db().ref(`devices/${deviceId}/telemetry`).get()).val();
  let command;
  try {command=domain.createCommand(action,state,Date.now(),randomUUID());}
  catch(error) {throw new HttpsError("failed-precondition",error.message);}
  command.requestedBy=request.auth.uid;
  const result=await db().ref(`devices/${deviceId}/control`).transaction(current=>{
    if(action!=="STOP" && current && ["pending","delivered"].includes(current.status) && current.expiresAt>Date.now()) return;
    return command;
  });
  if(!result.committed) throw new HttpsError("aborted","Another command is pending");
  return {id:command.id,expiresAt:command.expiresAt};
});

// A deterministic alert ID prevents repeated heartbeats/retried events from duplicating alerts.
exports.detectRain=onValueWritten({region:databaseTriggerRegion,ref:"/devices/{deviceId}/telemetry",retry:true},async(event)=>{
  const before=event.data.before.val(),after=event.data.after.val();
  if(!domain.rainCandidate(before,after)) return;
  const alert={id:`rain-${after.seq}`,createdAt:after.receivedAt,seq:after.seq,p_rain:after.p_rain,
    fallback:after.fallback,model:after.model,title:"มีแนวโน้มฝนตก",
    body:`โอกาสฝนตก ${Math.round(after.p_rain*100)}% ตรวจสอบราวตากผ้า`};
  await db().ref(`devices/${event.params.deviceId}/alerts/${alert.id}`).transaction(current=>current?undefined:alert);
});
exports.sendRainPush=onValueCreated({region:databaseTriggerRegion,ref:"/devices/{deviceId}/alerts/{alertId}",retry:true},async(event)=>{
  const alert=event.data.val();
  const access=(await db().ref(`access/${event.params.deviceId}`).get()).val();
  if(!access?.ownerUid) return;
  const tokenRef=db().ref(`users/${access.ownerUid}/pushTokens`);
  const entries=Object.entries((await tokenRef.orderByChild("createdAt").limitToLast(10).get()).val()||{});
  if(!entries.length) return;
  const results=await getMessaging().sendEachForMulticast({tokens:entries.map(([,v])=>v.token),
    notification:{title:alert.title,body:alert.body},
    data:{deviceId:event.params.deviceId,alertId:event.params.alertId},
    android:{priority:"high",notification:{tag:event.params.alertId,sound:"default"}},
    apns:{headers:{"apns-priority":"10","apns-collapse-id":event.params.alertId},payload:{aps:{sound:"default"}}}});
  let transient=false;
  for(let i=0;i<results.responses.length;i++) {
    const error=results.responses[i].error;
    if(!error) continue;
    if(["messaging/registration-token-not-registered","messaging/invalid-registration-token"].includes(error.code)) await tokenRef.child(entries[i][0]).remove();
    else transient=true;
  }
  if(transient) throw new Error("Retry transient push delivery failure");
});
