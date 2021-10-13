#include "serialize.h"

#include <cstring>

#include "savegameheader.h"
#include "world/world.h"
#include "world/fplock.h"
#include "world/waypoint.h"

#include <Tempest/MemReader>
#include <Tempest/MemWriter>

size_t Serialize::writeFunc(void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) {
  auto& self = *reinterpret_cast<Serialize*>(pOpaque);
  file_ofs += mz_zip_get_archive_file_start_offset(&self.impl);
  if(file_ofs!=self.curOffset) {
    self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
    return 0;
    }
  size_t ret = self.fout->write(pBuf,n);
  self.curOffset+=ret;
  return ret;
  }

Serialize::Serialize() {}

Serialize::Serialize(Tempest::ODevice& fout) : fout(&fout) {
  impl.m_pWrite           = Serialize::writeFunc;
  impl.m_pIO_opaque       = this;
  impl.m_zip_type         = MZ_ZIP_TYPE_USER;
  mz_zip_writer_init_v2(&impl, 0, 0);
  }

Serialize::Serialize(Tempest::IDevice& fin) : fin(&fin) {

  }

Serialize::~Serialize() {
  closeEntry();
  mz_zip_writer_finalize_archive(&impl);
  mz_zip_writer_end(&impl);
  }

Serialize Serialize::empty() {
  return Serialize();
  }

void Serialize::closeEntry() {
  if(entryName.empty())
    return;

  mz_bool status = mz_zip_writer_add_mem(&impl, entryName.c_str(), emtryBuf.data(), emtryBuf.size(), MZ_BEST_COMPRESSION);
  emtryBuf.clear();
  entryName.clear();
  if(!status)
    throw std::runtime_error("unable to write entry in game archive");
  }

void Serialize::implSetEntry(const char* fname) {
  closeEntry();
  entryName = fname;
  }

void Serialize::writeBytes(const void* buf, size_t sz) {
  size_t at = emtryBuf.size();
  emtryBuf.resize(emtryBuf.size()+sz);
  std::memcpy(&emtryBuf[at],buf,sz);
  }

void Serialize::readBytes(void* buf, size_t sz) {
  //if(zip_entry_read(impl,buf,sz)!=0)
  throw std::runtime_error("unable to write save-game file");
  }

void Serialize::implWrite(const std::string& s) {
  uint32_t sz=uint32_t(s.size());
  implWrite(sz);
  writeBytes(s.data(),sz);
  }

void Serialize::implRead(std::string& s) {
  uint32_t sz=0;
  implRead(sz);
  s.resize(sz);
  if(sz>0)
    readBytes(&s[0],sz);
  }

void Serialize::implWrite(std::string_view s) {
  uint32_t sz=uint32_t(s.size());
  implWrite(sz);
  writeBytes(s.data(),sz);
  }

void Serialize::implWrite(const Daedalus::ZString& s) {
  uint32_t sz=uint32_t(s.size());
  implWrite(sz);
  writeBytes(s.c_str(),sz);
  }

void Serialize::implRead(Daedalus::ZString& s) {
  std::string rs;
  implRead(rs);
  s = Daedalus::ZString(std::move(rs));
  }

void Serialize::implWrite(WeaponState w) {
  implWrite(uint8_t(w));
  }

void Serialize::implRead(WeaponState &w) {
  implRead(reinterpret_cast<uint8_t&>(w));
  }


void Serialize::implWrite(const WayPoint* wptr) {
  implWrite(wptr ? wptr->name : "");
  }

void Serialize::implRead(const WayPoint*& wptr) {
  implRead(tmpStr);
  wptr = ctx->findPoint(tmpStr,false);
  }

void Serialize::implWrite(const ScriptFn& fn) {
  uint32_t v = uint32_t(-1);
  if(fn.ptr<uint64_t(std::numeric_limits<uint32_t>::max()))
    v = uint32_t(fn.ptr);
  implWrite(v);
  }

void Serialize::implRead(ScriptFn& fn) {
  uint32_t v=0;
  implRead(v);
  if(v==std::numeric_limits<uint32_t>::max())
    fn.ptr = size_t(-1); else
    fn.ptr = v;
  }

void Serialize::implWrite(const Npc* npc) {
  uint32_t id = ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(const Npc *&npc) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  npc = ctx->npcById(id);
  }

void Serialize::implWrite(Npc *npc) {
  uint32_t id = ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(Npc *&npc) {
  uint32_t id=uint32_t(-1);
  read(id);
  npc = ctx->npcById(id);
  }

void Serialize::implWrite(Interactive* mobsi) {
  uint32_t id = ctx->mobsiId(mobsi);
  implWrite(id);
  }

void Serialize::implRead(Interactive*& mobsi) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  mobsi = ctx->mobsiById(id);
  }

void Serialize::implWrite(const SaveGameHeader& p) {
  p.save(*this);
  }

void Serialize::implRead(SaveGameHeader& p) {
  p = SaveGameHeader(*this);
  }

void Serialize::implWrite(const Tempest::Pixmap& p) {
  std::vector<uint8_t> tmp;
  Tempest::MemWriter w{tmp};
  p.save(w);
  writeBytes(tmp.data(),tmp.size());
  }

void Serialize::implRead(Tempest::Pixmap& p) {
  std::vector<uint8_t> tmp;
  readBytes(tmp.data(),tmp.size());
  Tempest::MemReader r{tmp};
  p = Tempest::Pixmap(r);
  }

void Serialize::implWrite(const Daedalus::GEngineClasses::C_Npc& h) {
  write(uint32_t(h.instanceSymbol));
  write(h.id,h.name,h.slot,h.effect,int32_t(h.npcType));
  write(int32_t(h.flags));
  write(h.attribute,h.hitChance,h.protection,h.damage);
  write(h.damagetype,h.guild,h.level);
  write(h.mission);
  write(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  write(h.daily_routine,h.start_aistate);
  write(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  write(h.aivar);
  write(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);
  }

void Serialize::implRead(Daedalus::GEngineClasses::C_Npc& h) {
  uint32_t instanceSymbol=0;

  read(instanceSymbol); h.instanceSymbol = instanceSymbol;
  read(h.id,h.name,h.slot,h.effect, reinterpret_cast<int32_t&>(h.npcType));
  read(reinterpret_cast<int32_t&>(h.flags));
  read(h.attribute,h.hitChance,h.protection,h.damage);
  read(h.damagetype,h.guild,h.level);
  read(h.mission);
  read(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  read(h.daily_routine,h.start_aistate);
  read(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  read(h.aivar);
  read(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);
  }

void Serialize::implWrite(const FpLock &fp) {
  fp.save(*this);
  }

void Serialize::implRead(FpLock &fp) {
  fp.load(*this);
  }
