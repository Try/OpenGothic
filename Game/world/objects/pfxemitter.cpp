#include "pfxemitter.h"

#include "graphics/pfx/pfxobjects.h"
#include "graphics/pfx/pfxbucket.h"
#include "graphics/pfx/particlefx.h"

using namespace Tempest;

PfxEmitter::PfxEmitter(PfxBucket& b, size_t id)
  :bucket(&b), id(id) {
  }

PfxEmitter::~PfxEmitter() {
  if(bucket!=nullptr) {
    std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
    bucket->freeEmitter(id);
    }
  }

PfxEmitter::PfxEmitter(PfxEmitter && b)
  :bucket(b.bucket), id(b.id) {
  b.bucket = nullptr;
  }

PfxEmitter& PfxEmitter::operator=(PfxEmitter &&b) {
  std::swap(bucket,b.bucket);
  std::swap(id,b.id);
  return *this;
  }

void PfxEmitter::setPosition(float x, float y, float z) {
  setPosition(Vec3(x,y,z));
  }

void PfxEmitter::setPosition(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.pos = pos;
  if(v.next!=nullptr)
    v.next->setPosition(pos);
  if(v.block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.pos = pos;
  }

void PfxEmitter::setTarget(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.target    = pos;
  v.hasTarget = true;
  }

void PfxEmitter::setDirection(const Matrix4x4& d) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.direction[0] = Vec3(d.at(0,0),d.at(0,1),d.at(0,2));
  v.direction[1] = Vec3(d.at(1,0),d.at(1,1),d.at(1,2));
  v.direction[2] = Vec3(d.at(2,0),d.at(2,1),d.at(2,2));

  if(v.next!=nullptr)
    v.next->setDirection(d);

  if(v.block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.direction[0] = v.direction[0];
  p.direction[1] = v.direction[1];
  p.direction[2] = v.direction[2];
  }

void PfxEmitter::setActive(bool act) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  if(act==v.active)
    return;
  v.active = act;
  if(v.next!=nullptr)
    v.next->setActive(act);
  if(act==true)
    v.waitforNext = bucket->owner->ppsCreateEmDelay;
  }

bool PfxEmitter::isActive() const {
  if(bucket==nullptr)
    return false;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  return v.active;
  }

void PfxEmitter::setLooped(bool loop) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.isLoop = loop;
  if(v.next!=nullptr)
    v.next->setLooped(loop);
  }

uint64_t PfxEmitter::effectPrefferedTime() const {
  if(bucket==nullptr)
    return 0;
  return bucket->owner->effectPrefferedTime();
  }

void PfxEmitter::setObjMatrix(const Matrix4x4 &mt) {
  setPosition (mt.at(3,0),mt.at(3,1),mt.at(3,2));
  setDirection(mt);
  }
