#include "app.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
static Adafruit_BME280 bme;
static bool bmeOk=false,active=false;
static unsigned tries=0,good=0;static uint32_t sampleAt=0;static Sample total;
void sensorsBegin(){
  Wire.begin(I2C_SDA,I2C_SCL);Wire.setTimeOut(30);
  bmeOk=bme.begin(0x76,&Wire)||bme.begin(0x77,&Wire);
  Serial.printf("[BME280] %s | SDA %d | SCL %d\n",bmeOk?"Found":"NOT FOUND",I2C_SDA,I2C_SCL);
  Wire.beginTransmission(0x23);Wire.write(0x10);
  Serial.printf("[BH1750] %s at 0x23\n",Wire.endTransmission()==0?"Responding":"NOT FOUND");
}
void measureStart(){if(active)return;active=true;tries=good=0;total=Sample();total.temp=total.hum=total.press=total.lux=0;sampleAt=millis()+200;}
bool sensorsTick(Sample& s,bool& ok){
  if(!active||!due(millis(),sampleAt))return false;
  sampleAt=millis()+200;tries++;
  float t,h,p,l;
  if(SMARTLINE_SENSOR_DEMO){float v=millis()/60000.f;t=29+2*sinf(v);h=78+16*sinf(v);p=1008-2*sinf(v);l=12000*(1-sinf(v))+.1f;}
  else {
    if(!bmeOk)bmeOk=bme.begin(0x76,&Wire)||bme.begin(0x77,&Wire);
    t=bmeOk?bme.readTemperature():NAN;h=bmeOk?bme.readHumidity():NAN;p=bmeOk?bme.readPressure()/100:NAN;
    l=NAN;if(Wire.requestFrom(0x23,2)==2)l=((Wire.read()<<8)|Wire.read())/1.2f;
    else {Wire.beginTransmission(0x23);Wire.write(0x10);Wire.endTransmission();}
  }
  if(isfinite(t)&&isfinite(h)&&isfinite(p)&&isfinite(l)&&t>=-40&&t<=85&&h>=0&&h<=100&&p>=300&&p<=1100&&l>=0){
    total.temp+=t;total.hum+=h;total.press+=p;total.lux+=l;good++;
  }
  if(good<3&&tries<6)return false;
  active=false;ok=good==3;s=Sample();s.epoch=epochNow();s.uptime=millis()/1000;
  if(ok){s.temp=total.temp/good;s.hum=total.hum/good;s.press=total.press/good;s.lux=total.lux/good;}
  s.rain=RAIN_ENABLED?(analogRead(RAIN_PIN)<RAIN_WET_ADC?1:0):-1;
  return true;
}
