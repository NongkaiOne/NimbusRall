#ifdef PREDICTION_OFFLINE_TEST
#include "app.h"
#include "test_model.h"
#include <sys/time.h>
Preferences prefs;
uint32_t seqHigh=0;
static constexpr uint32_t CAPACITY=1800;
static LogRecord historyRows[CAPACITY];
static uint32_t nextAt=0,lastStored=0;
static bool measuring=false,predicting=false;
static Sample sample;
static String command;
float diagnosticModelPreview();
uint32_t logOldest(){return seqHigh>=CAPACITY?seqHigh-CAPACITY+1:1;}
bool logRead(uint32_t seq,LogRecord& r){
  if(seq<logOldest()||seq>seqHigh)return false;
  r=historyRows[seq%CAPACITY];return r.seq==seq;
}
static void saveSample(){
  if(modelUsesCurrentOnly())return;
  if(!sample.epoch || (lastStored && sample.epoch-lastStored<30))return;
  LogRecord r={};r.seq=++seqHigh;r.epoch=sample.epoch;
  r.temp_x100=lroundf(sample.temp*100);r.hum_x100=lroundf(sample.hum*100);
  r.press_x10=lroundf(sample.press*10);r.lux_x10=lroundf(sample.lux*10);
  historyRows[r.seq%CAPACITY]=r;lastStored=sample.epoch;
}
void setup(){
  Serial.begin(115200);delay(200);
  Serial.println("OFFLINE TRAINED MODEL TEST: no Wi-Fi, no servo");
  String error;
  if(!modelInstall(TEST_MODEL,error,false)){
    Serial.println("MODEL ERROR: "+error);while(true)delay(1000);
  }
  Serial.println("MODEL: "+modelVersion());
  Serial.println(modelUsesCurrentOnly()?"CURRENT INPUTS: predict immediately, then every 30 minutes; MEASURE for manual test. No clock/history required.":"History model: send TIME <Unix seconds>; needs about 3 hours.");
  sensorsBegin();
}
void loop(){
  while(Serial.available()){
    char ch=Serial.read();
    if(ch=='\n'){
      if(command=="MEASURE"&&!measuring&&!predicting)nextAt=millis();
      if(command.startsWith("TIME ")){
        char* end;unsigned long epoch=strtoul(command.c_str()+5,&end,10);
        if(*end=='\0'&&epoch>1700000000UL&&epoch<4102444800UL){
          timeval tv={(time_t)epoch,0};settimeofday(&tv,nullptr);
          seqHigh=0;lastStored=0;Serial.println("TIME OK; RAM history cleared");
        }
      }
      command="";
    }else if(ch!='\r'&&command.length()<64)command+=ch;
  }
  if(!measuring&&!predicting&&due(millis(),nextAt)){
    measureStart();measuring=true;nextAt=millis()+1800000UL;
  }
  bool ok=false;
  if(measuring&&sensorsTick(sample,ok)){
    measuring=false;
    if(ok){predictionStart(sample);predicting=true;}
    else Serial.println("SENSOR ERROR: prediction skipped");
  }
  float probability;bool fallback;
  if(predicting&&predictionTick(probability,fallback)){
    predicting=false;
    Serial.printf("LIVE T=%.2f C RH=%.2f %% P=%.2f hPa Lux=%.2f epoch=%lu\n",
      sample.temp,sample.hum,sample.press,sample.lux,(unsigned long)sample.epoch);
    if(!sample.epoch&&!modelUsesCurrentOnly())Serial.println("WAITING FOR TIME: model prediction unavailable");
    else Serial.printf("MODEL rain_next_2h=%.2f%% state=%s history_rows=%lu operational=%.2f%% fallback=%s\n",
      diagnosticModelPreview()*100,fallback?"PREVIEW_INCOMPLETE_HISTORY":"READY",
      (unsigned long)seqHigh,probability*100,fallback?"true":"false");
    saveSample();
  }
  delay(1);
}
#endif
