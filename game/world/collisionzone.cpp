#include "collisionzone.h"

#include "worldobjects.h"
#include "world.h"

CollisionZone::CollisionZone() {
  }

CollisionZone::CollisionZone(World& w, const Tempest::Vec3& pos, float R)
  :owner(&w), pos(pos), size(R,0,0), type(T_Capsule) {
  owner->enableCollizionZone(*this);
  }

CollisionZone::CollisionZone(World& w, const Tempest::Vec3& pos, const Tempest::Vec3& size)
  :owner(&w), pos(pos), size(size), type(T_BBox) {
  owner->enableCollizionZone(*this);
  }

CollisionZone::CollisionZone(CollisionZone&& other)
  :owner(other.owner), cb(std::move(other.cb)), pos(other.pos), size(other.size), type(other.type) {
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

  std::swap(owner, other.owner);
  std::swap(cb,    other.cb);
  std::swap(pos,   other.pos);
  std::swap(size,  other.size);
  std::swap(type,  other.type);

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

bool CollisionZone::checkPos(const Tempest::Vec3& p) const {
  auto dp = p - pos;
  if(type==T_BBox) {
    if(std::fabs(dp.x)<size.x &&
       std::fabs(dp.y)<size.y &&
       std::fabs(dp.z)<size.z)
      return true;
    }
  else if(type==T_Capsule) {
    if(dp.quadLength()<size.x*size.x)
      return true;
    }
  return false;
  }

void CollisionZone::onIntersect(Npc& npc) {
  if(cb)
    cb(npc);
  }

void CollisionZone::setCallback(std::function<void(Npc&)> f) {
  cb = f;
  }

void CollisionZone::setPosition(const Tempest::Vec3& p) {
  pos = p;
  }
