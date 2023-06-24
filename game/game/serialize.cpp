#include "serialize.h"

#include <cstring>

#include "savegameheader.h"
#include "world/world.h"
#include "world/fplock.h"
#include "world/waypoint.h"

#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <Tempest/Log>
#include <Tempest/Application>

size_t Serialize::writeFunc(void* pOpaque, uint64_t file_ofs, const void* pBuf, size_t n) {
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

size_t Serialize::readFunc(void* pOpaque, uint64_t file_ofs, void* pBuf, size_t n) {
  auto& self = *reinterpret_cast<Serialize*>(pOpaque);
  file_ofs += mz_zip_get_archive_file_start_offset(&self.impl);
  if(self.curOffset<file_ofs) {
    size_t diff = size_t(file_ofs-self.curOffset);
    size_t mv   = self.fin->seek(diff);
    self.curOffset += uint64_t(mv);
    if(diff!=mv) {
      self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
      return 0;
      }
    }
  if(self.curOffset>file_ofs) {
    size_t diff = size_t(self.curOffset-file_ofs);
    size_t mv   = self.fin->unget(diff);
    self.curOffset -= uint64_t(mv);
    if(diff!=mv) {
      self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
      return 0;
      }
    }
  size_t ret = self.fin->read(pBuf,n);
  self.curOffset+=ret;
  return ret;
  }

Serialize::Serialize() {}

//static uint64_t time0 = 0;

Serialize::Serialize(Tempest::ODevice& fout) : fout(&fout) {
  //time0 = Tempest::Application::tickCount();
  entryBuf .resize(1*1024*1024);
  entryName.reserve(256);

  impl.m_pWrite           = Serialize::writeFunc;
  impl.m_pIO_opaque       = this;
  impl.m_zip_type         = MZ_ZIP_TYPE_USER;
  mz_zip_writer_init_v2(&impl, 0, 0);
  }

Serialize::Serialize(Tempest::IDevice& fin) : fin(&fin) {
  entryBuf .resize(1*1024*1024);
  entryName.reserve(256);

  impl.m_pRead            = Serialize::readFunc;
  impl.m_pIO_opaque       = this;
  impl.m_zip_type         = MZ_ZIP_TYPE_USER;
  mz_zip_reader_init(&impl, fin.size(), 0);
  }

Serialize::~Serialize() {
  closeEntry();
  if(fout!=nullptr) {
    mz_zip_writer_finalize_archive(&impl);
    mz_zip_writer_end(&impl);
    //Tempest::Log::d("save time = ", Tempest::Application::tickCount()-time0);
    }
  }

std::string_view Serialize::worldName() const {
  if(ctx!=nullptr)
    return ctx->name();
  return "_";
  }

void Serialize::closeEntry() {
  if(fout==nullptr)
    return;
  if(entryBuf.empty())
    return;

  mz_uint level  = entryBuf.size()>256 ? MZ_BEST_COMPRESSION : MZ_NO_COMPRESSION;
  mz_bool status = mz_zip_writer_add_mem(&impl, entryName.c_str(), entryBuf.data(), entryBuf.size(), level);
  entryBuf .clear();
  entryName.clear();
  if(!status)
    throw std::runtime_error("unable to write entry in game archive");
  }

bool Serialize::implSetEntry(std::string_view fname) {
  size_t prefix = 0;
  if(fout!=nullptr) {
    while(prefix<fname.size() && prefix<entryName.size()) {
      if(entryName[prefix]!=fname[prefix])
        break;
      ++prefix;
      }
    }
  closeEntry();
  entryName = fname;
  if(fout!=nullptr) {
    for(size_t i=prefix; i<entryName.size(); ++i) {
      if(entryName[i]=='/' && i+1<entryName.size()) {
        const char prev = entryName[i+1];
        entryName[i+1] = '\0';
        mz_uint32 id = mz_uint32(-1);
        if(!mz_zip_reader_locate_file_v2(&impl, entryName.c_str(), nullptr, 0, &id)) {
          mz_bool status = mz_zip_writer_add_mem(&impl, entryName.c_str(), NULL, 0, MZ_NO_COMPRESSION);
          if(!status)
            throw std::runtime_error("unable to allocate entry in game archive");
          }
        entryName[i+1] = prev;
        }
      }
    return true;
    }
  if(fin!=nullptr) {
    mz_uint32 id = mz_uint32(-1);
    if(mz_zip_reader_locate_file_v2(&impl, entryName.c_str(), nullptr, 0, &id)) {
      mz_zip_archive_file_stat stat = {};
      mz_zip_reader_file_stat(&impl,id,&stat);
      entryBuf.resize(size_t(stat.m_uncomp_size));
      mz_zip_reader_extract_file_to_mem(&impl,entryName.c_str(),entryBuf.data(),entryBuf.size(),0);
      } else {
      entryBuf.clear();
      }
    readOffset = 0;
    return !entryBuf.empty();
    }
  return false;
  }

uint32_t Serialize::implDirectorySize(std::string_view e) {
  // Get and print information about each file in the archive.
  uint32_t cnt = 0;
  for(mz_uint i = 0; i<mz_zip_reader_get_num_files(&impl); i++) {
    mz_zip_archive_file_stat stat = {};
    if(!mz_zip_reader_file_stat(&impl, i, &stat))
      throw std::runtime_error("unable to locate entry in game archive");
    auto len = std::strlen(stat.m_filename);
    if(len>e.size() && std::memcmp(e.data(),stat.m_filename,e.size())==0) {
      auto sep = std::strchr(stat.m_filename+e.size(),'/');
      if(sep==nullptr || (sep+1)==(stat.m_filename+len))
        ++cnt;
      }
    }
  return cnt;
  }

void Serialize::writeBytes(const void* buf, size_t sz) {
  if(sz==0)
    return;
  size_t at = entryBuf.size();
  entryBuf.resize(entryBuf.size()+sz);
  std::memcpy(&entryBuf[at],buf,sz);
  }

void Serialize::readBytes(void* buf, size_t sz) {
  if(fin==nullptr || readOffset+sz>entryBuf.size())
    throw std::runtime_error("unable to read save-game file");
  std::memcpy(buf,&entryBuf[size_t(readOffset)],sz);
  readOffset+=sz;
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
  wptr = ctx==nullptr ? nullptr : ctx->findPoint(tmpStr,false);
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
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(const Npc *&npc) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  npc = ctx==nullptr ? nullptr : ctx->npcById(id);
  }

void Serialize::implWrite(Npc *npc) {
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(Npc *&npc) {
  uint32_t id=uint32_t(-1);
  read(id);
  npc = ctx==nullptr ? nullptr : ctx->npcById(id);
  }

void Serialize::implWrite(Interactive* mobsi) {
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->mobsiId(mobsi);
  implWrite(id);
  }

void Serialize::implRead(Interactive*& mobsi) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  mobsi = ctx==nullptr ? nullptr : ctx->mobsiById(id);
  }

void Serialize::implWrite(const SaveGameHeader& p) {
  p.save(*this);
  }

void Serialize::implRead(SaveGameHeader& p) {
  p = SaveGameHeader(*this);
  }

void Serialize::implWrite(const Tempest::Pixmap& p) {
  std::vector<uint8_t> tmp;
  tmp.reserve(4*1024*1024);
  Tempest::MemWriter w{tmp};
  p.save(w);
  writeBytes(tmp.data(),tmp.size());
  }

void Serialize::implRead(Tempest::Pixmap& p) {
  Tempest::MemReader r{&entryBuf[size_t(readOffset)],size_t(entryBuf.size()-readOffset)};
  p = Tempest::Pixmap(r);
  readOffset += r.cursorPosition();
  }

void Serialize::implWrite(const phoenix::c_npc& h) {
  write(uint32_t(h.symbol_index()));
  write(h.id,h.name,h.slot,h.effect,int32_t(h.type));
  write(int32_t(h.flags));
  write(h.attribute,h.hitchance,h.protection,h.damage);
  write(h.damage_type,h.guild,h.level);
  write(h.mission);
  write(h.fight_tactic,h.weapon,h.voice,h.voice_pitch,h.body_mass);
  write(h.daily_routine,h.start_aistate);
  write(h.spawnpoint,h.spawn_delay,h.senses,h.senses_range);
  write(h.aivar);
  write(h.wp,h.exp,h.exp_next,h.lp,h.bodystate_interruptable_override,h.no_focus);
  }

void Serialize::readNpc(phoenix::vm& vm, std::shared_ptr<phoenix::c_npc>& h) {
  uint32_t instanceSymbol=0;
  read(instanceSymbol);

  auto sym = vm.find_symbol_by_index(instanceSymbol);

  if (sym != nullptr) {
    vm.allocate_instance(h, sym);
    } else {
    Tempest::Log::e("Cannot load serialized NPC ", instanceSymbol, ": Symbol not found.");
    }

  read(h->id,h->name,h->slot,h->effect, reinterpret_cast<int32_t&>(h->type));
  read(reinterpret_cast<int32_t&>(h->flags));
  read(h->attribute,h->hitchance,h->protection,h->damage);
  read(h->damage_type,h->guild,h->level);
  read(h->mission);
  read(h->fight_tactic,h->weapon,h->voice,h->voice_pitch,h->body_mass);
  read(h->daily_routine,h->start_aistate);
  read(h->spawnpoint,h->spawn_delay,h->senses,h->senses_range);
  read(h->aivar);
  read(h->wp,h->exp,h->exp_next,h->lp,h->bodystate_interruptable_override,h->no_focus);
  }

void Serialize::implWrite(const FpLock &fp) {
  fp.save(*this);
  }

void Serialize::implRead(FpLock &fp) {
  fp.load(*this);
  }

void Serialize::implWrite(const phoenix::animation_sample &i) {
  writeBytes(&i,sizeof(i));
  }

void Serialize::implRead(phoenix::animation_sample &i) {
  if (version() < 40) {
    read(i.position.x,i.position.y,i.position.z);
    read(i.rotation.x,i.rotation.y,i.rotation.z,i.rotation.w);
    } else {
    readBytes (&i,sizeof(i));
    }
  }
