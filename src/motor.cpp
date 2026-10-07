#include "app.h"
static int angle=90,target=90;static uint32_t startAt=0,stepAt=0;static bool attached=false;
void motorBegin(){ledcSetup(7,50,16);lineState=UNKNOWN;}
static void output(){if(!attached){ledcAttachPin(SERVO_PIN,7);attached=true;}ledcWrite(7,uint32_t((500+angle*2000.f/180)*65535/20000));}
bool motorMove(bool out){
  if(out&&lineState==UNKNOWN)return false;
  if((out&&lineState==EXTENDED)||(!out&&lineState==RETRACTED))return true;
  if(lineState==(out?MOVING_OUT:MOVING_IN))return true;
  target=out?SERVO_EXTEND_DEG:SERVO_RETRACT_DEG;startAt=millis();stepAt=startAt+300;
  lineState=out?MOVING_OUT:MOVING_IN;fault="";return true;
}
void motorStop(){if(attached){ledcDetachPin(SERVO_PIN);attached=false;}lineState=UNKNOWN;}
bool motorTick(){
  if(lineState!=MOVING_OUT&&lineState!=MOVING_IN)return false;
  if(millis()-startAt>=uint32_t(settings.motor_timeout_s*1000)){motorStop();lineState=FAULT_TIMEOUT;fault="servo_timeout";return true;}
  if(!due(millis(),stepAt))return false;stepAt=millis()+SERVO_STEP_MS;
  if(angle<target)angle++;else if(angle>target)angle--;output();
  if(angle==target){lineState=target==SERVO_EXTEND_DEG?EXTENDED:RETRACTED;return true;}return false;
}
