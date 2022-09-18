#include "savegameheader.h"
#include "serialize.h"

const char SaveGameHeader::tag[]="OpenGothic/Save";

struct tm36 {
  int32_t tm_sec;
  int32_t tm_min;
  int32_t tm_hour;
  int32_t tm_mday;
  int32_t tm_mon;
  int32_t tm_year;
  int32_t tm_wday;
  int32_t tm_yday;
  int32_t tm_isdst;
  };

static void readTm(Serialize& fin, std::tm& i, uint16_t version) {
  if(version<=38) {
    // NOTE: apparently std::tm is not binary same across different C++ compillers
    // 20 instead of 36
    fin.readBytes(&i,sizeof(i));
    return;
    }
  tm36 tx = {};
  fin.readBytes(&tx,sizeof(tx));

  i.tm_sec   = tx.tm_sec;
  i.tm_min   = tx.tm_min;
  i.tm_hour  = tx.tm_hour;
  i.tm_mday  = tx.tm_mday;
  i.tm_mon   = tx.tm_mon;
  i.tm_year  = tx.tm_year;
  i.tm_wday  = tx.tm_wday;
  i.tm_yday  = tx.tm_yday;
  i.tm_isdst = tx.tm_isdst;
  }

static void writeTm(Serialize& fout, const std::tm& i) {
  tm36 tx = {};
  tx.tm_sec   = i.tm_sec;
  tx.tm_min   = i.tm_min;
  tx.tm_hour  = i.tm_hour;
  tx.tm_mday  = i.tm_mday;
  tx.tm_mon   = i.tm_mon;
  tx.tm_year  = i.tm_year;
  tx.tm_wday  = i.tm_wday;
  tx.tm_yday  = i.tm_yday;
  tx.tm_isdst = i.tm_isdst;

  fout.writeBytes(&tx,sizeof(tx));
  }

SaveGameHeader::SaveGameHeader(Serialize &fin) {
  char     buf[sizeof(tag)]={};
  fin.readBytes(buf,sizeof(buf));
  if(std::memcmp(tag,buf,sizeof(buf))!=0)
    throw std::runtime_error("invalid file format");
  fin.read(version,name,world);
  readTm(fin,pcTime,version);
  //char b[40]; fin.readBytes(b,20);
  fin.read(wrldTime,playTime,isGothic2);
  }

void SaveGameHeader::save(Serialize &fout) const {
  fout.writeBytes(tag,sizeof(tag));
  fout.write(version,name,world);
  writeTm(fout,pcTime);
  fout.write(wrldTime,playTime,isGothic2);
  }
