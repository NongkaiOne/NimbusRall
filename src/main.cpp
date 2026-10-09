#include "app.h"
#include <errno.h>
#include <esp_system.h>
Settings settings;Preferences prefs;Sample current;
Line lineState=UNKNOWN;bool autoMode=true,fallback=true,sensorError=true;
uint32_t nextMeasure=0,manualUntil=0,seqHigh=0;float probability=.1;
String baseTopic,deviceId,fault;
static bool measuring=false,predicting=false;static uint32_t heartbeatAt=0,rebootAt=0;
static unsigned dryRounds=0,errorRounds=0;static uint8_t lastAction=0;
static String serialBuffer;
static bool numeric(const String& s,float& v){char* end;v=strtof(s.c_str(),&end);return s.length()&&end==s.c_str()+s.length()&&isfinite(v);}
static bool sequenceNumber(const String& s,uint32_t& v){
  if(!s.length())return false;for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return false;
  errno=0;char* end;unsigned long n=strtoul(s.c_str(),&end,10);if(errno||*end||n==UINT32_MAX)return false;v=n;return true;
}
static void manual(){autoMode=false;manualUntil=millis()+uint32_t(settings.manual_timeout_s*1000);dryRounds=0;}
void processCommand(String cmd){
  cmd.trim();bool ok=true;String msg="ok";
#ifdef SMARTLINE_NO_SERVO
  if(cmd=="EXTEND"||cmd=="RETRACT"){
    ack(cmd,false,"servo disabled in this build");publishStatus();return;
  }
#endif
  if(cmd=="EXTEND"||cmd=="RETRACT"){manual();ok=motorMove(cmd=="EXTEND");if(ok)lastAction=cmd=="EXTEND"?4:3;msg=ok?"accepted (servo demo)":"retract first to initialize";}
  else if(cmd=="STOP"){manual();motorStop();}
  else if(cmd=="MODE AUTO"){autoMode=true;dryRounds=0;}
  else if(cmd=="MODE MANUAL")manual();
  else if(cmd=="MEASURE"){if(measuring){ok=false;msg="measurement busy";}else{measureStart();measuring=true;}}
  else if(cmd.startsWith("SET ")){
    int p=cmd.indexOf(' ',4);float v;
    ok=p>4&&numeric(cmd.substring(p+1),v)&&configSet(cmd.substring(4,p),v);
    if(ok){if(cmd.substring(4,p)=="interval_s")nextMeasure=millis()+uint32_t(settings.interval_s*1000);publishConfig();}
    else msg="unknown key or invalid value / threshold ordering";
  }
  else if(cmd=="SAVE"){ok=configSave();if(!ok)msg="NVS write failed";}
  else if(cmd=="READLOG"||cmd.startsWith("READLOG ")){
    uint32_t since=0;ok=!readlogBusy()&&(cmd=="READLOG"||sequenceNumber(cmd.substring(8),since));
    if(ok)readlogStart(since);else msg="busy or invalid since_seq";
  }
  else if(cmd=="DELLOG"){ok=!readlogBusy()&&logDelete();if(!ok)msg="busy or filesystem error";}
  else if(cmd.startsWith("LABEL ")){
    int p=cmd.indexOf(' ',6);uint32_t seq=0;float label=0;
    ok=!readlogBusy()&&p>6&&sequenceNumber(cmd.substring(6,p),seq)&&numeric(cmd.substring(p+1),label)&&seq>=1&&(label==0||label==1)&&logLabel(seq,int(label));
    if(ok){LogRecord r;if(logRead(uint32_t(seq),r))publish("log/record",logCsv(r));}else msg="record not found, busy, or invalid label";
  }
  else if(cmd=="REBOOT"){motorStop();rebootAt=millis()+1500;}
  else {ok=false;msg="unknown command";}
  ack(cmd,ok,msg);publishStatus();
}
static void decide(){
  if(!autoMode)return;
  if(sensorError){dryRounds=0;if(++errorRounds>2&&lineState!=UNKNOWN&&lineState!=FAULT_TIMEOUT){motorMove(false);lastAction=1;}return;}
  errorRounds=0;
  if(lineState==UNKNOWN||lineState==FAULT_TIMEOUT)return;
  if(probability>=settings.retract_threshold){dryRounds=0;if(lineState!=RETRACTED&&lineState!=MOVING_IN){motorMove(false);lastAction=1;}}
  else if(probability<settings.extend_threshold&&current.lux>=settings.daylight_lux_min){
    if(++dryRounds>=2&&lineState!=EXTENDED&&lineState!=MOVING_OUT){motorMove(true);lastAction=2;}
  }else dryRounds=0;
}
void setup(){
  Serial.begin(115200);delay(2000);
  Serial.printf("[BOOT] Reset reason: %d (brownout=%d)\n", int(esp_reset_reason()), int(ESP_RST_BROWNOUT));
  Serial.println("[BOOT] Loading settings");Serial.flush();
  bool nvsOk=prefs.begin("smartline",false);
  Serial.printf("[NVS] %s\n",nvsOk?"OK":"FAILED");
  configLoad();motorBegin();
#ifdef SMARTLINE_NO_SERVO
  Serial.println("[SERVO] Disabled; PWM output compiled out");
#endif
  Serial.println("[BOOT] Mounting log storage");Serial.flush();
  bool storageOk=storeBegin();
  Serial.printf("[STORE] %s | records %lu\n",storageOk?"OK":"FAILED",(unsigned long)logCount());
  predictorBegin();Serial.println("[MODEL] "+modelVersion());
  Serial.println("[BOOT] Starting sensors");Serial.flush();sensorsBegin();
  Serial.println("[BOOT] Starting network");Serial.flush();networkBegin();
  measureStart();measuring=true;nextMeasure=millis()+uint32_t(settings.interval_s*1000);
#ifdef SMARTLINE_NO_SERVO
  Serial.println("[READY] Full system, servo disabled. MEASURE via Serial or MQTT.");
#else
  Serial.println("Smart Clothesline: RETRACT first; then MODE AUTO. Commands via Serial or MQTT.");
#endif
}
void loop(){
  if(motorTick())publishStatus();networkTick();storeTick();
  uint32_t now=millis();if(rebootAt&&due(now,rebootAt))ESP.restart();
  if(!autoMode&&due(now,manualUntil)){autoMode=true;dryRounds=0;publishStatus();}
  if(due(now,nextMeasure)){nextMeasure=now+uint32_t(settings.interval_s*1000);if(!measuring){measureStart();measuring=true;}}
  bool ok=false;
  if(measuring&&sensorsTick(current,ok)){
    sensorError=!ok;
    if(ok){predictionStart(current);predicting=true;if(fault=="sensor_err")fault="";}
    else {fallback=true;probability=.1;fault="sensor_err";}
    if(!ok){measuring=false;decide();if(!logAppend(lastAction))fault="log_write_failed";lastAction=0;publishStatus();}
  }
  if(predicting&&predictionTick(probability,fallback)){
    predicting=measuring=false;decide();if(!logAppend(lastAction))fault="log_write_failed";lastAction=0;publishStatus();
  }
  if(due(now,heartbeatAt)){heartbeatAt=now+uint32_t(settings.heartbeat_s*1000);publishStatus();}
  for(int i=0;i<64&&Serial.available();i++){
    char c=Serial.read();if(c=='\n'){processCommand(serialBuffer);serialBuffer="";}
    else if(c!='\r'){if(serialBuffer.length()<256)serialBuffer+=c;else serialBuffer="";}
  }
  delay(1);
}
