#include "serialize.h"

#include <cstring>

#include "savegameheader.h"
#include "world/world.h"
#include "world/fplock.h"
#include "world/waypoint.h"

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

  if(std::memcmp(tag,buf,sizeof(buf))!=0)
    throw std::runtime_error("invalid file format");
  if(ver<MinVersion || Version<ver)
    throw std::runtime_error("unsupported save file version");
  }

Serialize Serialize::empty() {
  Serialize e;
  return e;
  }

Serialize::Serialize()
  :ver(Version){
  }

void Serialize::write(const std::string &s) {
  uint32_t sz=uint32_t(s.size());
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

void Serialize::write(const Daedalus::ZString& s) {
  uint32_t sz=uint32_t(s.size());
  write(sz);
  writeBytes(s.c_str(),sz);
  }

void Serialize::read(Daedalus::ZString& s) {
  std::string rs;
  read(rs);
  s = Daedalus::ZString(std::move(rs));
  }

void Serialize::write(const SaveGameHeader &p) {
  p.save(*this);
  }

void Serialize::read(SaveGameHeader &p) {
  p = SaveGameHeader(*this);
  }

void Serialize::write(const Tempest::Pixmap &p) {
  p.save(*out);
  }

void Serialize::read(Tempest::Pixmap &p) {
  p = Tempest::Pixmap(*in);
  }

void Serialize::write(const WayPoint *wptr) {
  write(wptr ? wptr->name : "");
  }

void Serialize::read(const WayPoint *&wptr) {
  read(tmpStr);
  wptr = ctx->findPoint(tmpStr,false);
  }

void Serialize::write(const ScriptFn& fn) {
  uint32_t v = uint32_t(-1);
  if(fn.ptr<uint64_t(std::numeric_limits<uint32_t>::max()))
    v = uint32_t(fn.ptr);
  write(v);
  }

void Serialize::read(ScriptFn& fn) {
  uint32_t v=0;
  read(v);
  if(v==std::numeric_limits<uint32_t>::max())
    fn.ptr = size_t(-1); else
    fn.ptr = v;
  }

void Serialize::write(const Npc* npc) {
  uint32_t id = ctx->npcId(npc);
  write(id);
  }

void Serialize::read(const Npc *&npc) {
  uint32_t id=uint32_t(-1);
  read(id);
  npc = ctx->npcById(id);
  }

void Serialize::write(Npc *npc) {
  uint32_t id = ctx->npcId(npc);
  write(id);
  }

void Serialize::read(Npc *&npc) {
  uint32_t id=uint32_t(-1);
  read(id);
  npc = ctx->npcById(id);
  }

void Serialize::write(WeaponState w) {
  write(uint8_t(w));
  }

void Serialize::read(WeaponState &w) {
  read(reinterpret_cast<uint8_t&>(w));
  }

void Serialize::write(const FpLock &fp) {
  fp.save(*this);
  }

void Serialize::read(FpLock &fp) {
  fp.load(*this);
  }
