#include "app.h"
#include <cassert>
#include <cstdio>
Settings settings;Line lineState;String fault;uint32_t clockMs=0;int writes=0;bool pwmAttached=false;
int main(){
 motorBegin();assert(lineState==UNKNOWN);assert(!motorMove(true));assert(writes==0);
 assert(motorMove(false));clockMs=299;motorTick();assert(writes==0);
 for(clockMs=300;clockMs<2500;clockMs+=20)motorTick();assert(lineState==RETRACTED);assert(pwmAttached);
 assert(motorMove(true));int before=writes;clockMs+=299;motorTick();assert(writes==before);clockMs++;
 for(int i=0;i<160;i++){motorTick();clockMs+=20;}assert(lineState==EXTENDED);
 motorMove(false);motorStop();assert(lineState==UNKNOWN);assert(!pwmAttached);
 settings.motor_timeout_s=1;motorMove(false);clockMs+=1001;motorTick();assert(lineState==FAULT_TIMEOUT);assert(!pwmAttached);assert(fault=="servo_timeout");
 settings.motor_timeout_s=40;assert(motorMove(false));assert(fault.empty());
 std::puts("PASS: actual motor.cpp boot interlock, dead-time, sweep, stop, timeout, recovery");
}
