#include "app.h"
#include <stddef.h>
// Each logical slot has two CRC-protected generations. Torn writes leave the other valid.
struct __attribute__((packed)) Slot {uint32_t revision;LogRecord r;uint32_t crc;};
static uint32_t count=0,oldest=0;static bool ready=false;
static uint32_t bootFirst=0,backfillNext=0;
const char* CSV_HEADER="seq,epoch,temp_c,hum_pct,press_hpa,lux,p_rain,line,action,flags,rain_label,rain_now,uptime_s\n";
static uint32_t crcBytes(const uint8_t* b,size_t n,uint32_t c=0xffffffff){while(n--){c^=*b++;for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}return c;}
uint32_t crc32Update(uint32_t c,const String& s){return crcBytes((const uint8_t*)s.c_str(),s.length(),c);}
static uint16_t crc16(const LogRecord& r){uint16_t c=0xffff;auto b=(const uint8_t*)&r;for(size_t i=0;i<offsetof(LogRecord,crc16);i++){c^=uint16_t(b[i])<<8;for(int k=0;k<8;k++)c=(c<<1)^((c&0x8000)?0x1021:0);}return c;}
static bool slotRead(File& f,uint32_t index,Slot& s){return f.seek(index*sizeof(Slot))&&f.read((uint8_t*)&s,sizeof(s))==sizeof(s)&&s.r.seq&&s.r.crc16==crc16(s.r)&&s.crc==~crcBytes((uint8_t*)&s,offsetof(Slot,crc));}
static bool best(File& f,uint32_t seq,Slot& s,int& side){Slot a,b;uint32_t ix=(seq%LOG_CAPACITY)*2;bool av=slotRead(f,ix,a),bv=slotRead(f,ix+1,b);if(!av&&!bv)return false;side=(!av||(bv&&b.revision>a.revision))?1:0;s=side?b:a;return true;}
bool storeBegin(){
  ready=LittleFS.begin(false);if(!ready){fault="filesystem_mount: use uploadfs once";return false;}
  seqHigh=prefs.getUInt("seqHigh",0);oldest=prefs.getUInt("floor",1);
  File f=LittleFS.open("/records.bin","r");
  if(f){for(uint32_t i=0;i<LOG_CAPACITY*2;i++){Slot s;if(slotRead(f,i,s)&&s.r.seq>=oldest)seqHigh=max(seqHigh,s.r.seq);}f.close();}
  oldest=max(oldest,seqHigh>=LOG_CAPACITY?seqHigh-LOG_CAPACITY+1:1u);
  for(uint32_t n=oldest;n<=seqHigh;n++){LogRecord r;if(logRead(n,r))count++;}
  bootFirst=seqHigh+1;backfillNext=bootFirst;return true;
}
uint32_t logCount(){return count;}uint32_t logOldest(){return oldest;}
bool logRead(uint32_t seq,LogRecord& r){if(!ready||seq<oldest||seq>seqHigh)return false;File f=LittleFS.open("/records.bin","r");Slot s;int side;bool ok=f&&best(f,seq,s,side)&&s.r.seq==seq;f.close();if(ok)r=s.r;return ok;}
static bool writeRecord(LogRecord r){
  File f=LittleFS.open("/records.bin",LittleFS.exists("/records.bin")?"r+":"w+");if(!f)return false;
  Slot prev={},s={};int side=1;bool exists=best(f,r.seq,prev,side);
  s.revision=exists?prev.revision+1:1;s.r=r;s.r.crc16=crc16(s.r);s.crc=~crcBytes((uint8_t*)&s,offsetof(Slot,crc));
  bool ok=f.seek(((r.seq%LOG_CAPACITY)*2+(1-side))*sizeof(Slot))&&f.write((uint8_t*)&s,sizeof(s))==sizeof(s);f.flush();f.close();return ok;
}
bool logAppend(uint8_t action){
  if(!ready||seqHigh==UINT32_MAX)return false;
  LogRecord r={};r.seq=seqHigh+1;r.epoch=current.epoch;r.uptime_s=current.uptime;
  if(!sensorError){r.temp_x100=lroundf(current.temp*100);r.hum_x100=lroundf(current.hum*100);r.press_x10=lroundf(current.press*10);r.lux_x10=lroundf(current.lux*10);}
  r.p_rain_x100=lroundf(probability*100);r.line_state=lineState;r.action=action;
  r.flags=(sensorError?1:0)|(current.epoch?0:2)|(fallback?4:0);r.rain_label=-1;r.rain_now=current.rain;
  if(!writeRecord(r))return false;seqHigh=r.seq;prefs.putUInt("seqHigh",seqHigh);
  if(count<LOG_CAPACITY)count++;oldest=max(oldest,seqHigh>=LOG_CAPACITY?seqHigh-LOG_CAPACITY+1:1u);return true;
}
bool logLabel(uint32_t seq,int label){LogRecord r;if(!logRead(seq,r)||label<0||label>1)return false;r.rain_label=label;return writeRecord(r);}
void storeTick(){
  // Only this boot has a trustworthy uptime anchor. Never invent timestamps for older boots.
  if(!ready||!epochNow()||readlogBusy()||backfillNext>seqHigh)return;
  LogRecord r;uint32_t seq=backfillNext++;
  if(seq>=bootFirst&&logRead(seq,r)&&!r.epoch&&r.uptime_s<=millis()/1000){
    r.epoch=epochNow()-(millis()/1000-r.uptime_s);r.flags&=~2;
    if(writeRecord(r))publish("log/record",logCsv(r));else fault="log_backfill_failed";
  }
}
bool logDelete(){if(!ready)return false;if(!prefs.putUInt("floor",seqHigh+1))return false;oldest=seqHigh+1;count=0;return !LittleFS.exists("/records.bin")||LittleFS.remove("/records.bin");}
String logCsv(const LogRecord& r){
  String s=String(r.seq)+","+String(r.epoch)+",";
  if(r.flags&1)s+=",,,,";else s+=String(r.temp_x100/100.f,2)+","+String(r.hum_x100/100.f,2)+","+String(r.press_x10/10.f,1)+","+String(r.lux_x10/10.f,1)+",";
  return s+String(r.p_rain_x100/100.f,2)+","+lineName((Line)r.line_state)+","+String(r.action)+","+String(r.flags)+","+String(r.rain_label)+","+String(r.rain_now)+","+String(r.uptime_s)+"\n";
}
