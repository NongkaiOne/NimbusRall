// ภาพรวม: เครือข่าย MQTT สำหรับ environment ที่ไม่ได้เปิด SMARTLINE_FIREBASE เป็นทางเลือกแยกจาก Firebase
#ifndef SMARTLINE_FIREBASE
#include "app.h"
#include <WiFi.h>
#include <mqtt_client.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
static esp_mqtt_client_handle_t client=nullptr;
static bool wifiWasConnected=false;
#ifdef SMARTLINE_NO_SERVO
static void diagnosticWiFiEvent(WiFiEvent_t event,WiFiEventInfo_t info){
  if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)Serial.printf("[WIFI] Disconnected | reason %d\n",info.wifi_sta_disconnected.reason);
}
#endif
static volatile bool online=false,announce=false,lost=false;
struct Event {int kind,id;bool retain;char payload[4097];};
static QueueHandle_t events=nullptr;static String incoming,topic;static bool retained=false;
static uint32_t wifiAt=0,mqttAt=0,wifiBackoff=30000,mqttBackoff=2000;
static uint32_t pubSeq=0,pendingSeq=0,backlogAt=0,pendingAt=0;static int pendingId=0;
static bool stream=false;static uint32_t streamSeq=0,streamEnd=0,streamRows=0,streamCrc=0xffffffff,chunk=0;
static int streamPending=0,streamStage=0;static String streamPayload;static uint32_t streamAt=0;
// รับ event จาก MQTT เข้าคิว ให้ loop หลักเป็นผู้ทำคำสั่งจริง
static void eventHandler(void*,esp_event_base_t,int32_t eventId,void* data){
  auto e=(esp_mqtt_event_handle_t)data;
  if(eventId==MQTT_EVENT_CONNECTED){online=true;announce=true;return;}
  if(eventId==MQTT_EVENT_DISCONNECTED){online=false;lost=true;return;}
  if(eventId==MQTT_EVENT_PUBLISHED){Event v={};v.kind=1;v.id=e->msg_id;xQueueSend(events,&v,0);return;}
  if(eventId!=MQTT_EVENT_DATA)return;
  if(e->current_data_offset==0){incoming="";topic="";topic.concat(e->topic,e->topic_len);retained=e->retain;}
  if(e->total_data_len>4096||e->current_data_offset!=incoming.length())return;
  incoming.concat(e->data,e->data_len);
  if(incoming.length()==e->total_data_len){Event v={};v.kind=topic==baseTopic+"/cmd"?2:topic==baseTopic+"/model/set"?3:0;v.retain=retained;
    memcpy(v.payload,incoming.c_str(),incoming.length()+1);if(v.kind)xQueueSend(events,&v,0);incoming="";}
}
bool connected(){return online;}
int publish(const char* suffix,const String& body,int qos,bool retain){
  if(!online||!client)return -1;
  return esp_mqtt_client_enqueue(client,(baseTopic+"/"+suffix).c_str(),body.c_str(),body.length(),qos,retain,true);
}
// กำหนด deviceId/topic ตั้ง Wi-Fi และ NTP แล้วรอเริ่ม MQTT เมื่อพร้อม
void networkBegin(){
  uint64_t mac=ESP.getEfuseMac();char id[16];snprintf(id,sizeof(id),"sl-%06lx",(unsigned long)((mac>>24)&0xffffff));deviceId=id;baseTopic=String(TOPIC_PREFIX)+"/"+deviceId;
  events=xQueueCreate(8,sizeof(Event));
  Serial.printf("[WIFI] Starting radio | SSID: %s | IP: pending\n",WIFI_SSID);Serial.flush();
#ifdef SMARTLINE_NO_SERVO
  WiFi.onEvent(diagnosticWiFiEvent);
#endif
  WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);
#ifdef SMARTLINE_WIFI_TX_POWER
  WiFi.disconnect(false);delay(100);
  bool powerSet=WiFi.setTxPower(SMARTLINE_WIFI_TX_POWER);
  Serial.printf("[WIFI] TX limit: %s | reported %.2f dBm\n",powerSet?"OK":"FAILED",int(WiFi.getTxPower())/4.0f);
#endif
#ifdef SMARTLINE_NO_SERVO
  Serial.println("[WIFI] Scanning networks once");Serial.flush();
  int networks=WiFi.scanNetworks();
  Serial.printf("[WIFI] Scan result: %d (negative = error)\n",networks);
  for(int i=0;i<networks;i++)Serial.printf("[WIFI] %s | RSSI %d dBm | CH %d\n",WiFi.SSID(i).c_str(),WiFi.RSSI(i),WiFi.channel(i));
  WiFi.scanDelete();
#endif
  pubSeq=prefs.getUInt("last_pub_seq",0);
  bool mqttConfigured=strlen(MQTT_PASSWORD)&&String(MQTT_URI).startsWith("mqtts://")&&strstr(MQTT_ROOT_CA,"PASTE_")==nullptr;
  Serial.println(mqttConfigured?"[MQTT] Configured; waiting for Wi-Fi and time":"[MQTT] Not configured; Serial/local operation only");
  Serial.println("Device: "+deviceId);configTime(0,0,"pool.ntp.org","time.google.com");
}
void readlogStart(uint32_t since){stream=true;streamSeq=max(logOldest(),since+1);streamEnd=seqHigh;streamRows=0;chunk=0;streamCrc=0xffffffff;streamStage=0;streamPending=0;
  streamPayload="#BEGIN rows="+String(logCount())+" since="+String(since);streamAt=millis();}
bool readlogBusy(){return stream;}void readlogCancel(){stream=false;streamPending=0;streamPayload="";}
// ส่ง READLOG เป็น BEGIN/CHUNK/END พร้อม CRC32 และรอยืนยันแต่ละช่วง
static void streamTick(){
  if(!stream||!online)return;
  if(millis()-streamAt>120000){readlogCancel();ack("READLOG",false,"transfer timeout; retry");return;}
  if(streamPending)return;
  if(!streamPayload.length()){
    if(streamStage==1){String body=chunk==0?CSV_HEADER:"";
      while(streamSeq<=streamEnd&&body.length()<800){LogRecord r;if(logRead(streamSeq++,r)){body+=logCsv(r);streamRows++;}
        else {readlogCancel();ack("READLOG",false,"record missing or overwritten during transfer; retry");return;}}
      if(body.length()){streamCrc=crc32Update(streamCrc,body);streamPayload="#CHUNK "+String(chunk++)+"\n"+body;}
      else streamStage=2;
    }
    if(streamStage==2){char crc[9];snprintf(crc,sizeof(crc),"%08lX",(unsigned long)~streamCrc);streamPayload="#END rows="+String(streamRows)+" crc32="+crc;}
  }
  int id=publish("log/file",streamPayload);if(id>0)streamPending=id;
}
// จัดการ reconnect คำสั่ง โมเดลใหม่ และ log ที่ยังไม่ยืนยันจาก broker
void networkTick(){
  uint32_t now=millis();
  bool wifiConnected=WiFi.status()==WL_CONNECTED;
  if(wifiConnected&&!wifiWasConnected)Serial.printf("[WIFI] Connected | SSID: %s | IP: %s | RSSI: %d dBm\n",WiFi.SSID().c_str(),WiFi.localIP().toString().c_str(),WiFi.RSSI());
  if(!wifiConnected&&wifiWasConnected)Serial.println("[WIFI] Connection lost | IP unavailable");
  wifiWasConnected=wifiConnected;
  if(strlen(WIFI_SSID)&&WiFi.status()!=WL_CONNECTED&&due(now,wifiAt)){
    // Allow a full connection attempt before restarting the radio connection.
    Serial.printf("[WIFI] Connecting to %s (30 s minimum retry interval)\n",WIFI_SSID);
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD);wifiAt=now+wifiBackoff;wifiBackoff=min(60000u,wifiBackoff*2);
  }
  if(WiFi.status()==WL_CONNECTED){wifiBackoff=30000;wifiAt=now+wifiBackoff;
    if(!client&&epochNow()&&strlen(MQTT_PASSWORD)&&String(MQTT_URI).startsWith("mqtts://")&&strstr(MQTT_ROOT_CA,"PASTE_")==nullptr){
      esp_mqtt_client_config_t cfg={};cfg.uri=MQTT_URI;cfg.username=MQTT_USERNAME;cfg.password=MQTT_PASSWORD;cfg.cert_pem=MQTT_ROOT_CA;
      cfg.client_id=deviceId.c_str();static String will;will=baseTopic+"/availability";cfg.lwt_topic=will.c_str();cfg.lwt_msg="offline";cfg.lwt_qos=1;cfg.lwt_retain=1;
      cfg.keepalive=45;cfg.disable_clean_session=false;cfg.disable_auto_reconnect=true;cfg.buffer_size=5120;cfg.network_timeout_ms=5000;cfg.task_stack=12288;
      client=esp_mqtt_client_init(&cfg);if(client){esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,eventHandler,nullptr);esp_mqtt_client_start(client);mqttAt=now+6000;}
    }else if(client&&!online&&due(now,mqttAt)){esp_mqtt_client_reconnect(client);mqttAt=now+mqttBackoff;mqttBackoff=min(60000u,mqttBackoff*2);}
  }
  if(lost){lost=false;pendingId=0;readlogCancel();mqttAt=now+mqttBackoff;}
  if(announce){announce=false;mqttBackoff=2000;pendingId=0;
    esp_mqtt_client_subscribe(client,(baseTopic+"/cmd").c_str(),1);esp_mqtt_client_subscribe(client,(baseTopic+"/model/set").c_str(),1);
    publish("availability","online",1,true);publishConfig();publishStatus();
  }
  Event e;while(xQueueReceive(events,&e,0)==pdTRUE){
    if(e.kind==1){if(e.id==pendingId){pubSeq=pendingSeq;prefs.putUInt("last_pub_seq",pubSeq);pendingId=0;}
      if(e.id==streamPending){streamPending=0;streamPayload="";streamAt=now;if(streamStage==0)streamStage=1;else if(streamStage==2)readlogCancel();}}
    else if(e.retain)ack(e.kind==2?e.payload:"MODEL",false,"retained commands rejected");
    else if(e.kind==2)processCommand(e.payload);
    else if(e.kind==3){String err;bool ok=modelInstall(e.payload,err);ack("MODEL",ok,ok?"model installed":err);publishStatus();}
  }
  if(online&&pendingId&&now-pendingAt>30000)pendingId=0;
  if(online&&!pendingId&&due(now,backlogAt)){
    backlogAt=now+200;uint32_t n=max(pubSeq+1,logOldest());
    if(n<=seqHigh){LogRecord r;if(logRead(n,r)){int id=publish("log/record",logCsv(r));if(id>0){pendingId=id;pendingSeq=n;pendingAt=now;}}
      else {pubSeq=n;prefs.putUInt("last_pub_seq",pubSeq);}}
  }
  streamTick();
}

#endif
