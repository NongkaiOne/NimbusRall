#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <cJSON.h>
#include "config.h"
#include "features.h"
extern Settings settings;
extern Preferences prefs;
extern Line lineState;
extern bool autoMode, fallback, sensorError;
extern uint32_t nextMeasure, manualUntil, seqHigh;
extern float probability;
extern String baseTopic, deviceId, fault;
struct Sample { float temp=NAN, hum=NAN, press=NAN, lux=NAN; int rain=-1; uint32_t epoch=0,uptime=0; };
extern Sample current;
inline uint32_t epochNow(){time_t t=time(nullptr);return t>1700000000?uint32_t(t):0;}
inline bool due(uint32_t now,uint32_t deadline){return int32_t(now-deadline)>=0;}
inline String jsonText(cJSON* j){char* s=cJSON_PrintUnformatted(j);String out=s?s:"{}";cJSON_free(s);return out;}
inline double number(cJSON* j,const char* k,double def=NAN){auto v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsNumber(v)?v->valuedouble:def;}
void sensorsBegin();void measureStart();bool sensorsTick(Sample& s,bool& ok);
void motorBegin();bool motorMove(bool out);void motorStop();bool motorTick();
bool storeBegin();void storeTick();uint32_t logCount();uint32_t logOldest();
struct __attribute__((packed)) LogRecord {
  uint32_t seq,epoch,uptime_s;
  int16_t temp_x100; uint16_t hum_x100,press_x10;uint32_t lux_x10;
  uint8_t p_rain_x100,line_state,action,flags;int8_t rain_label,rain_now;uint8_t reserved[2];uint16_t crc16;
};
bool logRead(uint32_t seq,LogRecord& r);bool logAppend(uint8_t action);bool logLabel(uint32_t seq,int label);bool logDelete();
String logCsv(const LogRecord& r);extern const char* CSV_HEADER;
uint32_t crc32Update(uint32_t crc,const String& text);
void predictorBegin();bool modelInstall(const String& payload,String& error,bool save=true);
bool modelUsesCurrentOnly();
void predictionStart(const Sample& s);bool predictionTick(float& p,bool& usedFallback);String modelVersion();
void networkBegin();void networkTick();bool connected();int publish(const char* suffix,const String& body,int qos=1,bool retain=false);
void publishStatus();void publishConfig();void ack(const String& cmd,bool ok,const String& msg);
void processCommand(String cmd);void readlogStart(uint32_t since);void readlogCancel();bool readlogBusy();
void configLoad();bool configSet(const String& key,float value);bool configSave();
