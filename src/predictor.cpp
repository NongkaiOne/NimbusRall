// ภาพรวม: โหลดโมเดล Logistic Regression และทำนายฝนจากค่าปัจจุบันหรือประวัติตามชนิดโมเดล
#include "app.h"
static float means[FEATURE_COUNT],scales[FEATURE_COUNT],coefs[FEATURE_COUNT],intercept;
static String version="none";static bool installed=false;
static bool currentOnly=false;
bool modelUsesCurrentOnly(){return installed&&currentOnly;}
#ifdef PREDICTION_OFFLINE_TEST
static float modelPreview=NAN;
float diagnosticModelPreview(){return modelPreview;}
#endif
// ตรวจขนาด JSON ลำดับ feature และตัวเลขก่อนติดตั้งโมเดล
bool modelInstall(const String& payload,String& error,bool save){
  if(payload.length()>4096){error="model exceeds 4096 bytes";return false;}
  cJSON* j=cJSON_Parse(payload.c_str());if(!j){error="invalid JSON";return false;}
  auto ver=cJSON_GetObjectItemCaseSensitive(j,"version"),features=cJSON_GetObjectItemCaseSensitive(j,"features");
  auto mode=cJSON_GetObjectItemCaseSensitive(j,"input_mode");
  bool snapshot=cJSON_IsString(mode)&&!strcmp(mode->valuestring,"current");
  bool ok=cJSON_IsString(ver)&&strlen(ver->valuestring)>0&&strlen(ver->valuestring)<=48&&
    cJSON_GetArraySize(features)==FEATURE_COUNT&&isfinite(number(j,"intercept"))&&fabs(number(j,"intercept"))<1e9&&
    number(j,"horizon_h")>0&&number(j,"rain_mm")>0&&number(j,"threshold_suggest")>=0&&number(j,"threshold_suggest")<=1&&
    cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(j,"uses_rain_now"));
  float m[FEATURE_COUNT],s[FEATURE_COUNT],c[FEATURE_COUNT];
  const char* keys[]={"mean","scale","coef"};float* arrays[]={m,s,c};
  for(int i=0;i<FEATURE_COUNT&&ok;i++){
    auto f=cJSON_GetArrayItem(features,i);ok=cJSON_IsString(f)&&!strcmp(f->valuestring,FEATURE_NAMES[i]);
    for(int k=0;k<3&&ok;k++){auto a=cJSON_GetObjectItemCaseSensitive(j,keys[k]),v=cJSON_GetArrayItem(a,i);
      ok=cJSON_IsArray(a)&&cJSON_GetArraySize(a)==FEATURE_COUNT&&cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&fabs(v->valuedouble)<1e9;
      if(ok){arrays[k][i]=v->valuedouble;if(k==1&&arrays[k][i]<=0)ok=false;}
    }
  }
  if(ok&&mode&&!snapshot&&(!cJSON_IsString(mode)||strcmp(mode->valuestring,"history")))ok=false;
  for(int i=6;snapshot&&ok&&i<FEATURE_COUNT;i++)if(c[i]!=0)ok=false;
  if(ok&&save)ok=prefs.putString("model",payload)==payload.length();
  if(ok){memcpy(means,m,sizeof(m));memcpy(scales,s,sizeof(s));memcpy(coefs,c,sizeof(c));intercept=number(j,"intercept");version=ver->valuestring;installed=true;currentOnly=snapshot;}
  else error="invalid model schema or NVS write failed";
  cJSON_Delete(j);return ok;
}
// เลือกโมเดลจาก NVS ก่อน ถ้าไม่มี/ใช้ไม่ได้จึงลอง /model.json ใน LittleFS
void predictorBegin(){
  String err,raw=prefs.getString("model","");if(raw.length()&&modelInstall(raw,err,false))return;
  File f=LittleFS.open("/model.json","r");if(f&&f.size()<=4096){raw=f.readString();modelInstall(raw,err,false);}f.close();
}
String modelVersion(){return version;}
// จำความคืบหน้าการอ่านประวัติ เพื่อแบ่งคำนวณเป็นหลายรอบ loop
struct PredictionJob {
  Sample s;float dp1=0,dp3=0,dh1=0,dt1=0,peak=0,hour=0;
  double average=0,m2=0;int n=1,doy=1,localHour=0;
  uint32_t best1=UINT32_MAX,best3=UINT32_MAX,seq=0,end=0;
};
static PredictionJob job;
// เริ่มงานทำนายและคำนวณชั่วโมง/วันตามเวลาไทยเมื่อมี epoch
void predictionStart(const Sample& s){
  job=PredictionJob();job.s=s;job.average=s.press;job.seq=logOldest();job.end=seqHigh;
  if(s.epoch){time_t now=s.epoch+7*3600;tm t;gmtime_r(&now,&t);job.hour=t.tm_hour+t.tm_min/60.f;job.localHour=t.tm_hour;job.doy=t.tm_yday+1;}
}
// คืน false หากยังทำงานอยู่; เมื่อเสร็จคืน true พร้อมผลและสถานะ fallback
bool predictionTick(float& result,bool& usedFallback){
  auto& j=job;const Sample& s=j.s;
  // โมเดล current ใช้ค่าปัจจุบัน จึงไม่ต้องรอเวลาจริงหรือประวัติหลายชั่วโมง
  if(installed&&currentOnly){
    float x[FEATURE_COUNT];featureVector(s.temp,s.hum,s.press,s.lux,0,1,0,0,0,0,.5f,0,x);
    float p=logistic(x,means,scales,coefs,intercept);
    usedFallback=false;result=s.rain==1?1.f:p;
#ifdef PREDICTION_OFFLINE_TEST
    modelPreview=p;
#endif
    return true;
  }
  // Bounded work per loop; servo, STOP commands and network continue during history reads.
  for(int budget=0;s.epoch&&j.seq<=j.end&&budget<8;budget++){
    LogRecord r;if(!logRead(j.seq++,r)||(r.flags&1)||!r.epoch||r.epoch>=s.epoch)continue;
    uint32_t age=s.epoch-r.epoch;if(age>7*86400)continue;
    float p=r.press_x10/10.f;uint32_t d1=abs(int(age)-3600),d3=abs(int(age)-10800);
    if(d1<j.best1){j.best1=d1;j.dp1=s.press-p;j.dh1=s.hum-r.hum_x100/100.f;j.dt1=s.temp-r.temp_x100/100.f;}
    if(d3<j.best3){j.best3=d3;j.dp3=s.press-p;}
    if(age<=10800){j.n++;double delta=p-j.average;j.average+=delta/j.n;j.m2+=delta*(p-j.average);}
    time_t then=r.epoch+7*3600;tm t;gmtime_r(&then,&t);if(t.tm_hour==j.localHour)j.peak=max(j.peak,r.lux_x10/10.f);
  }
  if(s.epoch&&j.seq<=j.end)return false;
  // โมเดล history ต้องมีข้อมูลใกล้ย้อนหลัง 1 และ 3 ชั่วโมง ถ้าไม่พร้อมใช้กฎสำรอง
  bool history=j.best1<=900&&j.best3<=900;
  if(j.best1>900)j.dp1=j.dh1=j.dt1=0;if(j.best3>900)j.dp3=0;
  float x[FEATURE_COUNT];featureVector(s.temp,s.hum,s.press,s.lux,j.hour,j.doy,j.dp1,j.dp3,j.dh1,j.dt1,
    j.peak>0?min(2.f,s.lux/j.peak):.5f,sqrt(j.m2/j.n),x);
  usedFallback=!installed||!history||!s.epoch;
#ifdef PREDICTION_OFFLINE_TEST
  modelPreview=installed&&s.epoch?logistic(x,means,scales,coefs,intercept):NAN;
#endif
  result=s.rain==1?1.f:(usedFallback?fallbackProbability(x):logistic(x,means,scales,coefs,intercept));return true;
}
