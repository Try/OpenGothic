#include "sound.h"

#include "world/world.h"
#include "utils/fileext.h"
#include "gothic.h"

Sound::Sound() {
  }

Sound::Sound(World& world, Sound::Type type, std::string_view s, const Tempest::Vec3& pos, float range, bool freeSlot) {
  if(range<=0.f)
    range = 3500.f;

  auto& owner = *world.sound();
  if(!owner.isInListenerRange(pos,range))
    return;

  const auto cname = std::string(s);
  if(freeSlot) {
    std::lock_guard<std::mutex> guard(owner.sync);
    auto slot = owner.freeSlot.find(cname);
    if(slot!=owner.freeSlot.end() && !slot->second->eff.isFinished())
      return;
    }

  SoundFx* snd = nullptr;
  if(FileExt::hasExt(s,"WAV"))
    snd = Gothic::inst().loadSoundWavFx(s); else
    snd = Gothic::inst().loadSoundFx(s);

  if(snd==nullptr)
    return;

  *this = owner.implAddSound(*snd,pos,range);
  if(isEmpty())
    return;

  std::lock_guard<std::mutex> guard(owner.sync);
  owner.initSlot(*val);
  switch(type) {
    case T_Regular:{
      if(freeSlot)
        owner.freeSlot[cname] = val; else
        owner.effect.emplace_back(val);
      break;
      }
    case T_3D:{
      owner.effect3d.emplace_back(val);
      break;
      }
    }
  }

Sound::Sound(Sound&& other)
  :val(other.val){
  other.val = nullptr;
  }

Sound& Sound::operator =(Sound&& other) {
  std::swap(val,other.val);
  return *this;
  }

Sound::~Sound() {
  }

Sound::Sound(const std::shared_ptr<WorldSound::Effect>& val)
  :val(val) {
  }

Tempest::Vec3 Sound::position() const {
  return pos;
  }

bool Sound::isEmpty() const {
  return val==nullptr ? true : val->eff.isEmpty();
  }

bool Sound::isFinished() const {
  return val==nullptr ? true : val->eff.isFinished();
  }

void Sound::setOcclusion(float occ) {
  if(val!=nullptr)
    val->setOcclusion(occ);
  }

void Sound::setVolume(float v) {
  if(val!=nullptr)
    val->setVolume(v);
  }

float Sound::volume() const {
  if(val!=nullptr)
    return val->vol;
  return 0;
  }

void Sound::setPosition(const Tempest::Vec3& pos) {
  setPosition(pos.x,pos.y,pos.z);
  }

void Sound::setPosition(float x, float y, float z) {
  if(pos.x==x && pos.y==y && pos.z==z)
    return;
  if(val!=nullptr)
    val->eff.setPosition(x,y,z);
  pos = {x,y,z};
  }

void Sound::setLooping(bool l) {
  if(val!=nullptr)
    val->loop = l;
  }

void Sound::setAmbient(bool a) {
  if(val!=nullptr)
    val->ambient = a;
  }

void Sound::setActive(bool a) {
  if(val!=nullptr)
    val->active = a;
  }

void Sound::play() {
  if(val!=nullptr)
    val->eff.play();
  }

uint64_t Sound::effectPrefferedTime() const {
  if(val!=nullptr)
    return val->eff.timeLength();
  return 0;
  }
