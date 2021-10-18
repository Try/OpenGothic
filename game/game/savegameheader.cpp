#include "savegameheader.h"
#include "serialize.h"

const char SaveGameHeader::tag[]="OpenGothic/Save";

SaveGameHeader::SaveGameHeader(Serialize &fin) {
  char     buf[sizeof(tag)]={};
  fin.readBytes(buf,sizeof(buf));
  if(std::memcmp(tag,buf,sizeof(buf))!=0)
    throw std::runtime_error("invalid file format");
  fin.read(version,name,world,pcTime,wrldTime,isGothic2);
  }

void SaveGameHeader::save(Serialize &fout) const {
  fout.writeBytes(tag,sizeof(tag));
  fout.write(version,name,world,pcTime,wrldTime,isGothic2);
  }
