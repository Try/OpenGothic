#include "serialize.h"

#include <cstring>

const char Serialize::tag[]="OpenGothic/Save";

Serialize::Serialize(Tempest::ODevice & d):out(&d) {
  uint16_t v = Version;
  writeBytes(tag,sizeof(tag));
  writeBytes(&v,2);
  }

Serialize::Serialize(Tempest::IDevice &fin) : in(&fin){
  char     buf[sizeof(tag)]={};

  readBytes(buf,sizeof(buf));
  readBytes(&ver,2);

  if(std::memcmp(tag,buf,sizeof(buf))!=0 || ver>Version)
    throw std::runtime_error("invalid file format");
  }

Serialize Serialize::empty() {
  Serialize e;
  return e;
  }

Serialize::Serialize()
  :ver(Version){
  }

void Serialize::write(const std::string &s) {
  uint32_t sz=s.size();
  write(sz);
  writeBytes(s.data(),sz);
  }

void Serialize::read(std::string &s) {
  uint32_t sz=0;
  read(sz);
  s.resize(sz);
  if(sz>0)
    readBytes(&s[0],sz);
  }
