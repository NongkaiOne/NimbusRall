#include "app.h"
static const char* keys[]={"interval_s","retract_threshold","extend_threshold","daylight_lux_min","manual_timeout_s","motor_timeout_s","heartbeat_s"};
static float* values[]={&settings.interval_s,&settings.retract_threshold,&settings.extend_threshold,&settings.daylight_lux_min,&settings.manual_timeout_s,&settings.motor_timeout_s,&settings.heartbeat_s};
bool configSet(const String& key,float v){
  if(!isfinite(v))return false;
  for(int i=0;i<7;i++)if(key==keys[i]){
    bool ok=i==0?(v>=60&&v<=86400):i==1?(v>settings.extend_threshold&&v<=1):i==2?(v>=0&&v<settings.retract_threshold):
      i==3?(v>=0&&v<=200000):i==4?(v>=60&&v<=86400):i==5?(v>=1&&v<=120):(v>=5&&v<=3600);
    if(ok)*values[i]=v;return ok;
  }return false;
}
bool configSave(){return prefs.putBytes("config",&settings,sizeof(settings))==sizeof(settings);}
void configLoad(){
  if(prefs.getBytesLength("config")!=sizeof(settings))return;
  Settings s;prefs.getBytes("config",&s,sizeof(s));float v[]={s.interval_s,s.retract_threshold,s.extend_threshold,s.daylight_lux_min,s.manual_timeout_s,s.motor_timeout_s,s.heartbeat_s};
  if(s.extend_threshold>=s.retract_threshold)return;
  settings.extend_threshold=0;settings.retract_threshold=1;
  for(int i=0;i<7;i++)configSet(keys[i],v[i]);
}
void publishConfig(){cJSON* j=cJSON_CreateObject();for(int i=0;i<7;i++)cJSON_AddNumberToObject(j,keys[i],*values[i]);publish("config",jsonText(j),1,true);cJSON_Delete(j);}
