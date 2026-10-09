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
#else
#define FIREBASE_SYNC_URL ""
#define FIREBASE_DEVICE_SECRET ""
#endif

static QueueHandle_t requests=nullptr,replies=nullptr;
struct Reply {int status;char* body;};
static bool configured=false,inFlight=false,everSynced=false,wifiWasConnected=false;
static uint32_t wifiAt=0,syncAt=0,lastSync=0,uploadedSeq=0,batchMax=0,rewindSeq=0,syncBackoff=5000;
static String currentCommand,lastAck;

// HTTPS runs on a worker: sensor sampling, motorTick and STOP handling keep running.
static void syncWorker(void*){
  char* payload=nullptr;
  for(;;){
    if(xQueueReceive(requests,&payload,portMAX_DELAY)!=pdTRUE)continue;
    Reply result={-1,nullptr};
    WiFiClientSecure tls;tls.setCACert(FIREBASE_TRUST_ROOTS);tls.setHandshakeTimeout(5);
    HTTPClient http;http.setConnectTimeout(5000);http.setTimeout(5000);
    if(http.begin(tls,FIREBASE_SYNC_URL)){
      http.addHeader("Content-Type","application/json");
      http.addHeader("Authorization",String("Bearer ")+FIREBASE_DEVICE_SECRET);
      result.status=http.POST((uint8_t*)payload,strlen(payload));
      if(result.status==200){String body=http.getString();if(body.length()<=4096)result.body=strdup(body.c_str());}
      http.end();
    }
    cJSON_free(payload);
    xQueueSend(replies,&result,portMAX_DELAY);
  }
}
static bool allowed(const String& cmd){return cmd=="MEASURE"||cmd=="EXTEND"||cmd=="RETRACT"||cmd=="STOP"||cmd=="MODE AUTO"||cmd=="MODE MANUAL";}
static void commandAck(const String& id,bool ok,const String& message){
  cJSON* j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id",id.c_str());cJSON_AddBoolToObject(j,"ok",ok);cJSON_AddStringToObject(j,"msg",message.c_str());
  lastAck=jsonText(j);cJSON_Delete(j);prefs.putString("fb_cmd_ack",lastAck);
}
static void handleCommand(cJSON* command){
  if(!cJSON_IsObject(command))return;
  auto id=cJSON_GetObjectItemCaseSensitive(command,"id"),action=cJSON_GetObjectItemCaseSensitive(command,"action");
  if(!cJSON_IsString(id)||strlen(id->valuestring)>64||!cJSON_IsString(action))return;
  String commandId=id->valuestring,cmd=action->valuestring;
  if(prefs.getString("fb_cmd_id","")==commandId){
    cJSON* cached=cJSON_Parse(lastAck.c_str());auto oldId=cJSON_GetObjectItemCaseSensitive(cached,"id");
    bool same=cJSON_IsString(oldId)&&commandId==oldId->valuestring;cJSON_Delete(cached);
    if(!same)commandAck(commandId,false,"Previous outcome unknown; command not repeated after reboot");
    return;
  }
  double expires=number(command,"expiresAt",0);
  if(!epochNow()||!isfinite(expires)||expires<=double(epochNow())*1000){commandAck(commandId,false,"Command expired");return;}
  if(!allowed(cmd)){commandAck(commandId,false,"Unsupported command");return;}
  // Persist before acting, so a reset/retry cannot replay an actuator command.
  if(prefs.putString("fb_cmd_id",commandId)!=commandId.length()){commandAck(commandId,false,"Cannot persist command ID");return;}
  currentCommand=commandId;processCommand(cmd);currentCommand="";
}
static cJSON* logObject(const LogRecord& r){
  cJSON* j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"seq",r.seq);cJSON_AddNumberToObject(j,"epoch",r.epoch);cJSON_AddNumberToObject(j,"uptime",r.uptime_s);
  bool valid=!(r.flags&1);
  const char* names[]={"temp","hum","press","lux"};
  double values[]={r.temp_x100/100.0,r.hum_x100/100.0,r.press_x10/10.0,r.lux_x10/10.0};
  for(int i=0;i<4;i++){if(valid)cJSON_AddNumberToObject(j,names[i],values[i]);else cJSON_AddNullToObject(j,names[i]);}
  cJSON_AddNumberToObject(j,"p_rain",r.p_rain_x100/100.0);cJSON_AddNumberToObject(j,"flags",r.flags);cJSON_AddStringToObject(j,"line",lineName((Line)r.line_state));return j;
}
bool connected(){return everSynced&&WiFi.status()==WL_CONNECTED&&millis()-lastSync<30000;}
int publish(const char* suffix,const String& body,int,bool){
  if(!strcmp(suffix,"ack")&&currentCommand.length()){
    cJSON* j=cJSON_Parse(body.c_str());if(j){cJSON_AddStringToObject(j,"id",currentCommand.c_str());lastAck=jsonText(j);prefs.putString("fb_cmd_ack",lastAck);cJSON_Delete(j);}
  }else if(!strcmp(suffix,"log/record")){uint32_t n=strtoul(body.c_str(),nullptr,10);if(n){rewindSeq=rewindSeq?min(rewindSeq,n):n;prefs.putUInt("fb_rewind",rewindSeq);}}
  return connected()?1:-1;
}
void readlogStart(uint32_t){ack("READLOG",false,"Logs synchronize to Firebase automatically");}
bool readlogBusy(){return false;}void readlogCancel(){}
void networkBegin(){
  uint64_t mac=ESP.getEfuseMac();char id[16];snprintf(id,sizeof(id),"sl-%06lx",(unsigned long)((mac>>24)&0xffffff));deviceId=id;baseTopic=String(TOPIC_PREFIX)+"/"+deviceId;
  uploadedSeq=prefs.getUInt("fb_log_seq",0);rewindSeq=prefs.getUInt("fb_rewind",0);lastAck=prefs.getString("fb_cmd_ack","");
  configured=String(FIREBASE_SYNC_URL).startsWith("https://")&&strlen(FIREBASE_DEVICE_SECRET)==64;
  if(configured){
    requests=xQueueCreate(1,sizeof(char*));replies=xQueueCreate(1,sizeof(Reply));
    if(!requests||!replies||xTaskCreate(syncWorker,"firebase-sync",12288,nullptr,1,nullptr)!=pdPASS){configured=false;Serial.println("[FIREBASE] Worker allocation failed");}
  }
  Serial.printf("[FIREBASE] Device %s | %s\n",deviceId.c_str(),configured?"HTTPS sync ready":"Set include/firebase_secrets.h; local operation only");
  WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.disconnect(false);delay(100);
  bool power=WiFi.setTxPower(WIFI_POWER_8_5dBm);Serial.printf("[WIFI] TX limit: %s\n",power?"OK":"FAILED");
  configTime(0,0,"pool.ntp.org","time.google.com");
}
void networkTick(){
  uint32_t now=millis();bool wifi=WiFi.status()==WL_CONNECTED;
  if(wifi&&!wifiWasConnected)Serial.printf("[WIFI] Connected | SSID: %s | IP: %s\n",WiFi.SSID().c_str(),WiFi.localIP().toString().c_str());
  wifiWasConnected=wifi;
  if(!wifi&&strlen(WIFI_SSID)&&due(now,wifiAt)){WiFi.begin(WIFI_SSID,WIFI_PASSWORD);wifiAt=now+30000;}
  if(!configured)return;
  Reply response;
  if(xQueueReceive(replies,&response,0)==pdTRUE){
    inFlight=false;cJSON* data=response.body?cJSON_Parse(response.body):nullptr;
    if(response.status==200&&cJSON_IsObject(data)){
      everSynced=true;lastSync=now;syncBackoff=5000;
      uint32_t confirmed=uint32_t(number(data,"logAck",0));
      if(confirmed>uploadedSeq&&confirmed<=batchMax){uploadedSeq=confirmed;prefs.putUInt("fb_log_seq",uploadedSeq);}
      handleCommand(cJSON_GetObjectItemCaseSensitive(data,"command"));
    }else {Serial.printf("[FIREBASE] Sync failed | HTTP %d\n",response.status);syncBackoff=min(60000u,syncBackoff*2);}
    cJSON_Delete(data);free(response.body);syncAt=now+syncBackoff;
  }
  if(inFlight||!wifi||!epochNow()||!due(now,syncAt))return;
  cJSON* root=cJSON_CreateObject();cJSON_AddStringToObject(root,"deviceId",deviceId.c_str());
  cJSON* status=cJSON_Parse(statusPayload().c_str());if(!status){cJSON_Delete(root);syncAt=now+5000;return;}
  cJSON_AddItemToObject(root,"telemetry",status);cJSON* logs=cJSON_AddArrayToObject(root,"logs");batchMax=uploadedSeq;
  if(rewindSeq){
    if(rewindSeq<=uploadedSeq){uploadedSeq=rewindSeq-1;prefs.putUInt("fb_log_seq",uploadedSeq);}
    rewindSeq=0;prefs.putUInt("fb_rewind",0);batchMax=uploadedSeq;
  }
  for(uint32_t n=max(uploadedSeq+1,logOldest());n<=seqHigh&&cJSON_GetArraySize(logs)<4;n++){
    LogRecord r;if(logRead(n,r)){cJSON_AddItemToArray(logs,logObject(r));batchMax=n;}
  }
  if(lastAck.length()){cJSON* a=cJSON_Parse(lastAck.c_str());if(a)cJSON_AddItemToObject(root,"ack",a);}
  char* body=cJSON_PrintUnformatted(root);cJSON_Delete(root);
  if(body&&xQueueSend(requests,&body,0)==pdTRUE)inFlight=true;
  else {cJSON_free(body);syncAt=now+5000;}
}
#endif
