#include "sound.h"

Sound::Sound() {
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

void Sound::setMaxDistance(float v) {
  if(val!=nullptr)
    val->eff.setMaxDistance(v);
  }

void Sound::setRefDistance(float v) {
  if(val!=nullptr)
    val->eff.setRefDistance(v);
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

void Sound::setActive(bool a) {
  if(val!=nullptr)
    val->active = a;
  }

void Sound::play() {
  if(val!=nullptr)
    val->eff.play();
  }
