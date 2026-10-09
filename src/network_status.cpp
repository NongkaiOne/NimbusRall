// ภาพรวม: สร้าง JSON สถานะและผลตอบรับคำสั่ง (ACK) รูปแบบร่วมของ MQTT และ Firebase
#include "app.h"
#include <WiFi.h>
// ตอบรับหรือปฏิเสธคำสั่งพร้อมเหตุผล ไม่ใช่การยืนยันตำแหน่งทางกายภาพของราว
void ack(const String& cmd,bool ok,const String& msg){cJSON* j=cJSON_CreateObject();cJSON_AddStringToObject(j,"cmd",cmd.c_str());cJSON_AddBoolToObject(j,"ok",ok);cJSON_AddStringToObject(j,"msg",msg.c_str());String s=jsonText(j);Serial.println(s);publish("ack",s);cJSON_Delete(j);}
// รวมค่าล่าสุด ฝน โหมด fault ความสด และ servo_enabled ให้แอพแสดงผล
String statusPayload(){
  cJSON* j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"v",1);cJSON_AddNumberToObject(j,"t",epochNow());
  cJSON_AddNumberToObject(j,"temp",current.temp);cJSON_AddNumberToObject(j,"hum",current.hum);cJSON_AddNumberToObject(j,"press",current.press);cJSON_AddNumberToObject(j,"lux",current.lux);
  cJSON_AddNumberToObject(j,"p_rain",probability);cJSON_AddStringToObject(j,"line",lineName(lineState));cJSON_AddStringToObject(j,"mode",autoMode?"AUTO":"MANUAL");
  cJSON_AddStringToObject(j,"model",modelVersion().c_str());cJSON_AddBoolToObject(j,"fallback",fallback);cJSON_AddBoolToObject(j,"sensor_err",sensorError);
  if(fault.length())cJSON_AddStringToObject(j,"fault",fault.c_str());else cJSON_AddNullToObject(j,"fault");
  cJSON_AddNumberToObject(j,"next_in",due(millis(),nextMeasure)?0:(nextMeasure-millis())/1000);
  cJSON_AddNumberToObject(j,"logCount",logCount());cJSON_AddNumberToObject(j,"seq",seqHigh);cJSON_AddNumberToObject(j,"rssi",WiFi.RSSI());
  cJSON_AddNumberToObject(j,"uptime",millis()/1000);cJSON_AddStringToObject(j,"fw",FW_VERSION);cJSON_AddBoolToObject(j,"demo",SMARTLINE_SENSOR_DEMO);
  cJSON_AddNumberToObject(j,"measurement_age_s",millis()/1000-current.uptime);
  cJSON_AddNumberToObject(j,"retract_threshold",settings.retract_threshold);
  cJSON_AddNumberToObject(j,"extend_threshold",settings.extend_threshold);
#ifdef SMARTLINE_NO_SERVO
  cJSON_AddBoolToObject(j,"servo_enabled",false);
#else
  cJSON_AddBoolToObject(j,"servo_enabled",true);
#endif
  String text=jsonText(j);cJSON_Delete(j);return text;
}

void publishStatus(){String text=statusPayload();publish("status",text,0,true);Serial.println(text);}
