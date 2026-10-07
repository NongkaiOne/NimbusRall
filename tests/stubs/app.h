#pragma once
#include <cstdint>
#include <string>
#include "config.h"
using String=std::string;
extern Settings settings;extern Line lineState;extern String fault;
extern uint32_t clockMs;extern int writes;extern bool pwmAttached;
inline uint32_t millis(){return clockMs;}
inline bool due(uint32_t n,uint32_t d){return int32_t(n-d)>=0;}
inline void ledcSetup(int,int,int){}
inline void ledcAttachPin(int,int){pwmAttached=true;}
inline void ledcDetachPin(int){pwmAttached=false;}
inline void ledcWrite(int,uint32_t){writes++;}
void motorBegin();bool motorMove(bool);void motorStop();bool motorTick();
