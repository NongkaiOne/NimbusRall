// ภาพรวม: เครือข่าย Spark: ล็อกอินบัญชีบอร์ด แล้วซิงก์สถานะ/logs/alerts และรับคำสั่งผ่าน HTTPS
#ifdef SMARTLINE_FIREBASE
#include "app.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "firebase_roots.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif
#if __has_include("firebase_secrets.h")
#include "firebase_secrets.h"
#endif
#ifndef FIREBASE_DATABASE_URL
#define FIREBASE_DATABASE_URL ""
#define FIREBASE_API_KEY ""
#define FIREBASE_DEVICE_EMAIL ""
#define FIREBASE_DEVICE_PASSWORD ""
#define FIREBASE_DEVICE_UID ""
#endif

// Spark: Firebase Auth REST + Realtime Database REST. No Cloud Functions.
static const uint32_t SYNC_INTERVAL_MS=10000;
// คิวระหว่าง loop หลักกับงาน HTTPS ส่งชุดข้อมูลไปและรับผล/คำสั่งกลับ
struct Request {char* body;uint32_t logSeq,rainSeq;bool rainHigh;};
struct Reply {int status;bool uploaded;uint32_t logSeq,rainSeq;bool rainHigh;char* command;};
static QueueHandle_t requests=nullptr,replies=nullptr;
static bool configured=false,inFlight=false,everSynced=false,wifiWasConnected=false,rainHigh=false;
static uint32_t wifiAt=0,syncAt=0,lastSync=0,uploadedSeq=0,rewindSeq=0,rainSeq=0,syncBackoff=SYNC_INTERVAL_MS;
static String currentCommand,lastAck;
// Only the HTTPS worker accesses authentication state.
static String idToken,refreshToken,ackSent;
static uint32_t tokenUntil=0;

static String stringField(cJSON* j,const char* key){
  cJSON* v=cJSON_GetObjectItemCaseSensitive(j,key);
  return cJSON_IsString(v)?String(v->valuestring):String();
}
static void serverTime(cJSON* j,const char* key){
  cJSON* value=cJSON_AddObjectToObject(j,key);cJSON_AddStringToObject(value,".sv","timestamp");
}
// เรียก HTTPS พร้อมตรวจใบรับรอง กำหนด timeout และจำกัดขนาดคำตอบ
static int requestHttp(const String& url,const char* method,const String& payload,String& response,bool form=false){
  WiFiClientSecure tls;tls.setCACert(FIREBASE_TRUST_ROOTS);tls.setHandshakeTimeout(5);
  HTTPClient http;http.setConnectTimeout(5000);http.setTimeout(5000);
  if(!http.begin(tls,url))return -1;
  if(strcmp(method,"GET"))http.addHeader("Content-Type",form?"application/x-www-form-urlencoded":"application/json");
  int code=http.sendRequest(method,(uint8_t*)payload.c_str(),payload.length());
  // Never print URLs, tokens, passwords or response bodies.
  if(code==200 && (http.getSize()<0 || http.getSize()<=16384)){
    response=http.getString();if(response.length()>16384){response="";code=-2;}
  }
  http.end();return code;
}
static String formEncode(const String& input){
  String out;const char hex[]="0123456789ABCDEF";
  for(size_t i=0;i<input.length();i++){unsigned char c=input[i];
    if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);
    else{out+='%';out+=hex[c>>4];out+=hex[c&15];}
  }return out;
}
// ใช้ ID token เดิมถ้ายังใช้ได้ ถ้าหมดให้ refresh หรือเข้าสู่ระบบด้วยบัญชีบอร์ด
static bool authenticate(){
  if(idToken.length()&&!due(millis(),tokenUntil))return true;
  String response;int code;
  if(refreshToken.length()){
    code=requestHttp(String("https://securetoken.googleapis.com/v1/token?key=")+FIREBASE_API_KEY,"POST",
      "grant_type=refresh_token&refresh_token="+formEncode(refreshToken),response,true);
  }else{
    cJSON* j=cJSON_CreateObject();cJSON_AddStringToObject(j,"email",FIREBASE_DEVICE_EMAIL);
    cJSON_AddStringToObject(j,"password",FIREBASE_DEVICE_PASSWORD);cJSON_AddBoolToObject(j,"returnSecureToken",true);
    String payload=jsonText(j);cJSON_Delete(j);
    code=requestHttp(String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=")+FIREBASE_API_KEY,"POST",payload,response);
  }
  if(code!=200){idToken="";refreshToken="";Serial.printf("[FIREBASE] Auth failed | HTTP %d\n",code);return false;}
  cJSON* j=cJSON_Parse(response.c_str());bool refreshing=refreshToken.length();
  String token=stringField(j,refreshing?"id_token":"idToken");
  String refresh=stringField(j,refreshing?"refresh_token":"refreshToken");
  String uid=stringField(j,refreshing?"user_id":"localId");
  long lifetime=stringField(j,refreshing?"expires_in":"expiresIn").toInt();cJSON_Delete(j);
  if(token.length()<20||refresh.length()<20||uid!=FIREBASE_DEVICE_UID||lifetime<120){
    idToken="";refreshToken="";Serial.println("[FIREBASE] Auth response or device UID mismatch");return false;
  }
  idToken=token;refreshToken=refresh;tokenUntil=millis()+uint32_t(min(lifetime-60,3500L))*1000;return true;
}
static String databasePath(const String& suffix,bool silent=false){
  String url=FIREBASE_DATABASE_URL;while(url.endsWith("/"))url.remove(url.length()-1);
  return url+"/devices/"+deviceId+suffix+".json?auth="+idToken+(silent?"&print=silent":"");
}
// งาน FreeRTOS แยก: รับคิว → อัปโหลด → อ่าน control → ส่ง ACK ไม่เรียก servo ในงานนี้
static void syncWorker(void*){
  Request job;
  for(;;){
    if(xQueueReceive(requests,&job,portMAX_DELAY)!=pdTRUE)continue;
    Reply result={401,false,job.logSeq,job.rainSeq,job.rainHigh,nullptr};
    cJSON* root=cJSON_Parse(job.body);cJSON_free(job.body);
    if(root&&authenticate()){
      cJSON* changes=cJSON_GetObjectItemCaseSensitive(root,"changes");String response;
      result.status=requestHttp(databasePath("",true),"PATCH",jsonText(changes),response);
      result.uploaded=result.status==204||result.status==200;
      if(result.uploaded){
        result.status=requestHttp(databasePath("/control"),"GET","",response);
        if(result.status==200){
          cJSON* control=cJSON_Parse(response.c_str());
          String commandId=stringField(control,"id");
          cJSON* ack=cJSON_GetObjectItemCaseSensitive(root,"ack");
          String ackId=stringField(ack,"id");
          if(commandId.length()&&ackId==commandId&&ackSent!=ackId){
            String ignored;int code=requestHttp(databasePath("/acks/"+ackId,true),"PUT",jsonText(ack),ignored);
            if(code==200||code==204)ackSent=ackId;
            // STOP can replace an old command between GET and ACK; permission_denied is harmless here.
          }
          if(cJSON_IsObject(control)&&commandId.length())result.command=strdup(response.c_str());
          cJSON_Delete(control);
        }
      }
      if(result.status==401){idToken="";refreshToken="";}
    }
    cJSON_Delete(root);xQueueSend(replies,&result,portMAX_DELAY);
  }
}
static bool allowed(const String& cmd){
  return cmd=="MEASURE"||cmd=="EXTEND"||cmd=="RETRACT"||cmd=="STOP"||cmd=="MODE AUTO"||cmd=="MODE MANUAL";
}
static void commandAck(const String& id,bool ok,const String& message){
  cJSON* j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id",id.c_str());
  cJSON_AddBoolToObject(j,"ok",ok);cJSON_AddStringToObject(j,"msg",message.c_str());
  serverTime(j,"receivedAt");lastAck=jsonText(j);cJSON_Delete(j);prefs.putString("sp_cmd_ack",lastAck);
}
// ตรวจหมดอายุและจำ command ID ลง NVS ก่อนทำคำสั่ง เพื่อไม่ทำซ้ำหลังรีบูต
static void handleCommand(cJSON* command){
  if(!cJSON_IsObject(command))return;
  String commandId=stringField(command,"id"),cmd=stringField(command,"action");
  if(commandId.length()!=20||!cmd.length())return;
  if(prefs.getString("sp_cmd_id","")==commandId){
    cJSON* cached=cJSON_Parse(lastAck.c_str());bool same=stringField(cached,"id")==commandId;cJSON_Delete(cached);
    if(!same)commandAck(commandId,false,"Previous outcome unknown; command not repeated after reboot");
    return;
  }
  double expires=number(command,"expiresAt",0);
  if(!epochNow()||!isfinite(expires)||expires<=double(epochNow())*1000)return;
  if(!allowed(cmd)){commandAck(commandId,false,"Unsupported command");return;}
  if(prefs.putString("sp_cmd_id",commandId)!=commandId.length()){commandAck(commandId,false,"Cannot persist command ID");return;}
  currentCommand=commandId;processCommand(cmd);currentCommand="";
}
// แปลง log ไบนารีเป็น JSON พร้อมเวลารับจากเซิร์ฟเวอร์
static cJSON* logObject(const LogRecord& r){
  cJSON* j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"seq",r.seq);cJSON_AddNumberToObject(j,"epoch",r.epoch);
  cJSON_AddNumberToObject(j,"uptime",r.uptime_s);bool valid=!(r.flags&1);
  const char* names[]={"temp","hum","press","lux"};
  double values[]={r.temp_x100/100.0,r.hum_x100/100.0,r.press_x10/10.0,r.lux_x10/10.0};
  for(int i=0;i<4;i++){if(valid)cJSON_AddNumberToObject(j,names[i],values[i]);else cJSON_AddNullToObject(j,names[i]);}
  cJSON_AddNumberToObject(j,"p_rain",r.p_rain_x100/100.0);cJSON_AddNumberToObject(j,"flags",r.flags);
  cJSON_AddStringToObject(j,"line",lineName((Line)r.line_state));serverTime(j,"receivedAt");return j;
}
bool connected(){return everSynced&&WiFi.status()==WL_CONNECTED&&millis()-lastSync<30000;}
// เชื่อม API publish เดิมเข้ากับ Firebase โดยเก็บ ACK และทำเครื่องหมาย log ที่ต้องซิงก์ใหม่
int publish(const char* suffix,const String& body,int,bool){
  if(!strcmp(suffix,"ack")&&currentCommand.length()){
    cJSON* j=cJSON_Parse(body.c_str());
    if(j){auto ok=cJSON_GetObjectItemCaseSensitive(j,"ok");commandAck(currentCommand,cJSON_IsTrue(ok),stringField(j,"msg"));cJSON_Delete(j);}
  }else if(!strcmp(suffix,"log/record")){
    uint32_t n=strtoul(body.c_str(),nullptr,10);
    if(n){rewindSeq=rewindSeq?min(rewindSeq,n):n;prefs.putUInt("sp_rewind",rewindSeq);}
  }return connected()?1:-1;
}
void readlogStart(uint32_t){ack("READLOG",false,"Logs synchronize to Firebase automatically");}
bool readlogBusy(){return false;}void readlogCancel(){}
// สร้าง deviceId จาก MAC เตรียมคิว/สถานะซิงก์เดิม และเริ่ม Wi-Fi + NTP
void networkBegin(){
  uint64_t mac=ESP.getEfuseMac();char id[16];snprintf(id,sizeof(id),"sl-%06lx",(unsigned long)((mac>>24)&0xffffff));
  deviceId=id;baseTopic=String(TOPIC_PREFIX)+"/"+deviceId;
  configured=String(FIREBASE_DATABASE_URL).startsWith("https://")&&strlen(FIREBASE_API_KEY)&&strlen(FIREBASE_DEVICE_EMAIL)&&strlen(FIREBASE_DEVICE_PASSWORD)&&strlen(FIREBASE_DEVICE_UID);
  if(configured){
    String destination=String(FIREBASE_DATABASE_URL)+"/"+FIREBASE_DEVICE_UID;uint32_t hash=2166136261u;
    for(size_t i=0;i<destination.length();i++)hash=(hash^uint8_t(destination[i]))*16777619u;
    if(prefs.getUInt("sp_target",0)!=hash){
      prefs.putUInt("sp_log_seq",0);prefs.putUInt("sp_rewind",0);prefs.putULong64("sp_rain",0);
      prefs.putString("sp_cmd_id","");prefs.putString("sp_cmd_ack","");prefs.putUInt("sp_target",hash);
    }
    requests=xQueueCreate(1,sizeof(Request));replies=xQueueCreate(1,sizeof(Reply));
    if(!requests||!replies||xTaskCreate(syncWorker,"firebase-spark",14336,nullptr,1,nullptr)!=pdPASS){configured=false;Serial.println("[FIREBASE] Worker allocation failed");}
  }
  uploadedSeq=prefs.getUInt("sp_log_seq",0);rewindSeq=prefs.getUInt("sp_rewind",0);
  uint64_t rainState=prefs.getULong64("sp_rain",0);rainSeq=rainState>>1;rainHigh=rainState&1;lastAck=prefs.getString("sp_cmd_ack","");
  Serial.printf("[FIREBASE] Spark | Device %s | %s\n",deviceId.c_str(),configured?"Auth + RTDB ready":"Set include/firebase_secrets.h; local operation only");
  WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.disconnect(false);delay(100);
  bool power=WiFi.setTxPower(WIFI_POWER_8_5dBm);Serial.printf("[WIFI] TX limit: %s\n",power?"OK":"FAILED");
  configTime(0,0,"pool.ntp.org","time.google.com");
}
// ต่อ Wi-Fi รับผล worker และสร้างชุดซิงก์ตามเวลา ถ้าล้มเหลวเพิ่มเวลารอก่อนลองใหม่
void networkTick(){
  uint32_t now=millis();bool wifi=WiFi.status()==WL_CONNECTED;
  if(wifi&&!wifiWasConnected)Serial.printf("[WIFI] Connected | SSID: %s | IP: %s\n",WiFi.SSID().c_str(),WiFi.localIP().toString().c_str());
  wifiWasConnected=wifi;
  if(!wifi&&strlen(WIFI_SSID)&&due(now,wifiAt)){
    Serial.printf("[WIFI] Connecting to %s\n",WIFI_SSID);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);wifiAt=now+30000;
  }
  if(!configured)return;
  Reply response;
  if(xQueueReceive(replies,&response,0)==pdTRUE){
    inFlight=false;
    if(response.uploaded){
      if(!connected())Serial.println("[FIREBASE] Spark database connected");
      everSynced=true;lastSync=now;uploadedSeq=response.logSeq;
      if(uploadedSeq!=prefs.getUInt("sp_log_seq",0))prefs.putUInt("sp_log_seq",uploadedSeq);
      // Recover a partially written NVS cursor after reboot by replaying idempotent log/alert writes.
      rainSeq=response.rainSeq;rainHigh=response.rainHigh;
      uint64_t rainState=(uint64_t(rainSeq)<<1)|(rainHigh?1:0);
      if(rainState!=prefs.getULong64("sp_rain",0))prefs.putULong64("sp_rain",rainState);
    }
    if(response.status==200){syncBackoff=SYNC_INTERVAL_MS;cJSON* command=response.command?cJSON_Parse(response.command):nullptr;handleCommand(command);cJSON_Delete(command);}
    else {Serial.printf("[FIREBASE] Sync failed | HTTP %d\n",response.status);syncBackoff=min(60000u,syncBackoff*2);}
    free(response.command);syncAt=now+syncBackoff;
  }
  if(inFlight||!wifi||!epochNow()||!due(now,syncAt))return;
  if(rewindSeq){
    if(rewindSeq<=uploadedSeq){uploadedSeq=rewindSeq-1;prefs.putUInt("sp_log_seq",uploadedSeq);}
    rewindSeq=0;prefs.putUInt("sp_rewind",0);
  }
  cJSON* root=cJSON_CreateObject();cJSON* changes=cJSON_AddObjectToObject(root,"changes");
  cJSON* status=cJSON_Parse(statusPayload().c_str());
  if(!status){cJSON_Delete(root);syncAt=now+SYNC_INTERVAL_MS;return;}
  serverTime(status,"receivedAt");cJSON_AddItemToObject(changes,"telemetry",status);
  uint32_t batchMax=uploadedSeq,nextRainSeq=rainSeq;bool nextRainHigh=rainHigh;int count=0;
  // ส่ง log ไม่เกิน 4 รายการต่อชุด และสร้าง alert เมื่อข้ามเกณฑ์ฝนจากต่ำเป็นสูง
  for(uint32_t n=max(min(uploadedSeq,rainSeq)+1,logOldest());n<=seqHigh&&count<4;n++){
    LogRecord r;if(!logRead(n,r))continue;count++;batchMax=max(batchMax,n);
    char key[40];snprintf(key,sizeof(key),"logs/%010lu",(unsigned long)n);cJSON_AddItemToObject(changes,key,logObject(r));
    if(n>nextRainSeq){
      bool high=!(r.flags&1)&&r.p_rain_x100/100.0>=settings.retract_threshold;
      if(high&&!nextRainHigh){
        cJSON* alert=cJSON_CreateObject();cJSON_AddNumberToObject(alert,"seq",n);
        cJSON_AddNumberToObject(alert,"p_rain",r.p_rain_x100/100.0);
        cJSON_AddNumberToObject(alert,"createdAt",r.epoch?double(r.epoch)*1000:0);
        cJSON_AddBoolToObject(alert,"fallback",r.flags&4);
        snprintf(key,sizeof(key),"alerts/rain-%010lu",(unsigned long)n);cJSON_AddItemToObject(changes,key,alert);
      }
      nextRainSeq=n;nextRainHigh=high;
    }
  }
  if(lastAck.length()){cJSON* a=cJSON_Parse(lastAck.c_str());if(a)cJSON_AddItemToObject(root,"ack",a);}
  Request job={cJSON_PrintUnformatted(root),batchMax,nextRainSeq,nextRainHigh};cJSON_Delete(root);
  if(job.body&&xQueueSend(requests,&job,0)==pdTRUE)inFlight=true;
  else{cJSON_free(job.body);syncAt=now+SYNC_INTERVAL_MS;}
}
#endif
