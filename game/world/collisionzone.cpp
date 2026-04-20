#include "collisionzone.h"

#include <Tempest/Log>

#include "world/objects/npc.h"
#include "worldobjects.h"
#include "world.h"

#include "game/serialize.h"
#include "graphics/pfx/particlefx.h"

#include "utils/dbgpainter.h"

CollisionZone::CollisionZone() {
  }

CollisionZone::CollisionZone(World& w, const Tempest::Vec3& pos, const ParticleFx& pfx)
  :owner(&w), time0(w.tickCount()), type(T_Capsule), pos(pos), size(0,0,0), pfx(&pfx) {
  owner->enableCollizionZone(*this);
  }

CollisionZone::CollisionZone(World& w, const Tempest::Vec3& pos, const Tempest::Vec3& size)
  :owner(&w), time0(w.tickCount()), type(T_BBox), pos(pos), size(size) {
  owner->enableCollizionZone(*this);
  }

CollisionZone::CollisionZone(CollisionZone&& other)
  : owner(other.owner), cb(std::move(other.cb)), time0(other.time0), type(other.type), pos(other.pos), size(other.size),
    pfx(other.pfx), intersect(std::move(other.intersect)) {
  other.owner = nullptr;
  if(owner!=nullptr) {
    owner->enableCollizionZone (*this);
    owner->disableCollizionZone(other);
    }
  }

CollisionZone& CollisionZone::operator =(CollisionZone&& other) {
  if(other.owner!=nullptr)
    other.owner->disableCollizionZone(other);
  if(owner!=nullptr)
    owner->disableCollizionZone(*this);

  std::swap(owner,     other.owner);
  std::swap(cb,        other.cb);
  std::swap(time0,     other.time0);
  std::swap(type,      other.type);
  std::swap(pos,       other.pos);
  std::swap(size,      other.size);
  std::swap(pfx,       other.pfx);
  std::swap(intersect, other.intersect);

  if(other.owner!=nullptr)
    other.owner->enableCollizionZone(other);
  if(owner!=nullptr)
    owner->enableCollizionZone(*this);
  return *this;
  }

CollisionZone::~CollisionZone() {
  if(owner!=nullptr)
    owner->disableCollizionZone(*this);
  }

void CollisionZone::drawVobBox(DbgPainter& p) const {
  switch(type) {
    case T_BBox:
      p.setPen(Tempest::Color(1,0,1));
      p.drawAabb(pos-size, pos+size);
      break;
    case T_Capsule:
      break;
    }

  }

void CollisionZone::save(Serialize& fout) const {
  fout.write(uint32_t(intersect.size()));
  for(auto i:intersect)
    fout.write(i);
  }

void CollisionZone::load(Serialize& fin) {
  uint32_t size=0;
  fin.read(size);
  intersect.resize(size);
  for(auto& i:intersect)
    fin.read(i);
  for(size_t i=0;i<intersect.size();)
    if(intersect[i]==nullptr) {
      intersect[i] = intersect.back();
      intersect.pop_back();
      } else {
      ++i;
      }
  }

bool CollisionZone::checkPos(const Npc& npc) const {
  auto bbox = npc.bBoxCol();
  auto pos  = npc.centerPosition();
  return checkPos(pos, bbox);
  }

bool CollisionZone::checkPos(const Tempest::Vec3& pos, const Tempest::Vec3* bbox) const {
  if(bbox==nullptr)
    return checkPos(pos, Tempest::Vec3(0));
  const auto sz = (bbox[1] - bbox[0])*0.5f;
  return checkPos(pos, sz);
  }

bool CollisionZone::checkPos(const Tempest::Vec3& p, const Tempest::Vec3& npcSz) const {
  auto dp = p - pos;
  auto sz = size + npcSz;
  if(type==T_BBox) {
    if(std::fabs(dp.x)<sz.x &&
       std::fabs(dp.y)<sz.y &&
       std::fabs(dp.z)<sz.z)
      return true;
    }
  else if(type==T_Capsule) {
    // spells (fire rain)
    static bool once = false;
    if(!once) {
      once = true;
      Tempest::Log::d("TODO: CollisionZone capsule collision");
      // assert(false);
      }
    if(dp.x*dp.x+dp.z*dp.z<sz.x*sz.x &&
       std::fabs(dp.y)<std::fabs(sz.y))
      return true;
    }
  return false;
  }

void CollisionZone::onIntersect(Npc& npc) {
  for(auto i:intersect)
    if(i==&npc)
      return;
  intersect.push_back(&npc);

  if(cb)
    cb(npc);
  }

void CollisionZone::tick(uint64_t /*dt*/) {
  for(size_t i=0;i<intersect.size();) {
    Npc& npc  = *intersect[i];
    auto bbox = npc.bBoxCol();
    auto pos  = npc.centerPosition();
    if(!checkPos(pos, bbox)) {
      intersect[i] = intersect.back();
      intersect.pop_back();
      } else {
      ++i;
      }
    }
  if(pfx!=nullptr) {
    auto dim = pfx->shpDim*pfx->shpScale(owner->tickCount()-time0);
    size = dim;
    }
  /*
  if(intersect.size()==0) {
    disableTicks();
    }*/
  }

void CollisionZone::setCallback(std::function<void(Npc&)> f) {
  cb = f;
  }

void CollisionZone::setPosition(const Tempest::Vec3& p) {
  pos = p;
  }
