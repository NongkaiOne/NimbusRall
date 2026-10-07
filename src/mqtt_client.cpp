#include "app.h"
#include <WiFi.h>
#include <mqtt_client.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
static esp_mqtt_client_handle_t client=nullptr;
static volatile bool online=false,announce=false,lost=false;
struct Event {int kind,id;bool retain;char payload[4097];};
static QueueHandle_t events=nullptr;static String incoming,topic;static bool retained=false;
static uint32_t wifiAt=0,mqttAt=0,wifiBackoff=2000,mqttBackoff=2000;
static uint32_t pubSeq=0,pendingSeq=0,backlogAt=0,pendingAt=0;static int pendingId=0;
static bool stream=false;static uint32_t streamSeq=0,streamEnd=0,streamRows=0,streamCrc=0xffffffff,chunk=0;
static int streamPending=0,streamStage=0;static String streamPayload;static uint32_t streamAt=0;
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
void ack(const String& cmd,bool ok,const String& msg){cJSON* j=cJSON_CreateObject();cJSON_AddStringToObject(j,"cmd",cmd.c_str());cJSON_AddBoolToObject(j,"ok",ok);cJSON_AddStringToObject(j,"msg",msg.c_str());String s=jsonText(j);Serial.println(s);publish("ack",s);cJSON_Delete(j);}
void publishStatus(){
  cJSON* j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"v",1);cJSON_AddNumberToObject(j,"t",epochNow());
  cJSON_AddNumberToObject(j,"temp",current.temp);cJSON_AddNumberToObject(j,"hum",current.hum);cJSON_AddNumberToObject(j,"press",current.press);cJSON_AddNumberToObject(j,"lux",current.lux);
  cJSON_AddNumberToObject(j,"p_rain",probability);cJSON_AddStringToObject(j,"line",lineName(lineState));cJSON_AddStringToObject(j,"mode",autoMode?"AUTO":"MANUAL");
  cJSON_AddStringToObject(j,"model",modelVersion().c_str());cJSON_AddBoolToObject(j,"fallback",fallback);cJSON_AddBoolToObject(j,"sensor_err",sensorError);
  if(fault.length())cJSON_AddStringToObject(j,"fault",fault.c_str());else cJSON_AddNullToObject(j,"fault");
  cJSON_AddNumberToObject(j,"next_in",due(millis(),nextMeasure)?0:(nextMeasure-millis())/1000);
  cJSON_AddNumberToObject(j,"logCount",logCount());cJSON_AddNumberToObject(j,"seq",seqHigh);cJSON_AddNumberToObject(j,"rssi",WiFi.RSSI());
  cJSON_AddNumberToObject(j,"uptime",millis()/1000);cJSON_AddStringToObject(j,"fw",FW_VERSION);cJSON_AddBoolToObject(j,"demo",SMARTLINE_SENSOR_DEMO);
  String text=jsonText(j);publish("status",text,0,true);Serial.println(text);cJSON_Delete(j);
}
void networkBegin(){
  uint64_t mac=ESP.getEfuseMac();char id[16];snprintf(id,sizeof(id),"sl-%06lx",(unsigned long)((mac>>24)&0xffffff));deviceId=id;baseTopic=String(TOPIC_PREFIX)+"/"+deviceId;
  events=xQueueCreate(8,sizeof(Event));WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);pubSeq=prefs.getUInt("last_pub_seq",0);
  Serial.println("Device: "+deviceId);configTime(0,0,"pool.ntp.org","time.google.com");
}
void readlogStart(uint32_t since){stream=true;streamSeq=max(logOldest(),since+1);streamEnd=seqHigh;streamRows=0;chunk=0;streamCrc=0xffffffff;streamStage=0;streamPending=0;
  streamPayload="#BEGIN rows="+String(logCount())+" since="+String(since);streamAt=millis();}
bool readlogBusy(){return stream;}void readlogCancel(){stream=false;streamPending=0;streamPayload="";}
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
void networkTick(){
  uint32_t now=millis();
  if(strlen(WIFI_SSID)&&WiFi.status()!=WL_CONNECTED&&due(now,wifiAt)){
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD);wifiAt=now+wifiBackoff;wifiBackoff=min(60000u,wifiBackoff*2);
  }
  if(WiFi.status()==WL_CONNECTED){wifiBackoff=2000;
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
