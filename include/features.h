// ภาพรวม: สูตรสร้างอินพุต 16 ค่าและคำนวณโมเดลบน ESP32 ต้องตรงกับ training/features.py
#pragma once
#include <cmath>
#include <algorithm>
constexpr int FEATURE_COUNT=16;
static const char* const FEATURE_NAMES[]={"temp","hum","press","lux_log","dew_point","temp_minus_dew",
  "d_press_1h","d_press_3h","d_hum_1h","d_temp_1h","lux_ratio","hour_sin","hour_cos","doy_sin","doy_cos","press_std_3h"};
// สร้างเวกเตอร์ตาม FEATURE_NAMES: ค่าปัจจุบัน จุดน้ำค้าง แนวโน้ม แสง และวัฏจักรเวลา
inline void featureVector(float t,float h,float p,float lux,float hour,int doy,
  float dp1,float dp3,float dh1,float dt1,float ratio,float sd,float* x) {
  const float pi=3.14159265358979323846f;
  float g=logf(std::max(.01f,h)/100)+17.62f*t/(243.12f+t),dew=243.12f*g/(17.62f-g);
  float a[]={t,h,p,log10f(1+std::max(0.f,lux)),dew,t-dew,dp1,dp3,dh1,dt1,ratio,
    sinf(2*pi*hour/24),cosf(2*pi*hour/24),sinf(2*pi*doy/365.25f),cosf(2*pi*doy/365.25f),sd};
  for(int i=0;i<FEATURE_COUNT;i++) x[i]=a[i];
}
// กฎสำรองเมื่อโมเดล/ประวัติไม่พร้อม คืน 0.8 หรือ 0.1 ตามเงื่อนไขอากาศ
inline float fallbackProbability(const float* x) {
  return ((x[7]<=-2 && x[1]>=80)||x[6]<=-1.5f||(x[1]>=92 && x[10]<.25f))?.8f:.1f;
}
// ปรับมาตรฐานด้วย mean/scale คูณน้ำหนัก แล้วผ่าน sigmoid ให้ผลอยู่ช่วง 0–1
inline float logistic(const float* x,const float* mean,const float* scale,const float* coef,float intercept) {
  float z=intercept;for(int i=0;i<FEATURE_COUNT;i++) z+=coef[i]*(x[i]-mean[i])/scale[i];
  return 1/(1+expf(-std::max(-40.f,std::min(40.f,z))));
}
