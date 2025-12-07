#include "waypoint.h"

#include <cmath>
#include <cstring>
#include <cctype>

using namespace Tempest;

WayPoint::WayPoint() {
  }

WayPoint::WayPoint(const zenkit::WayPoint &dat)
  : x(dat.position.x),y(dat.position.y),z(dat.position.z),
    dir(dat.direction.x, dat.direction.y, dat.direction.z),
    underWater(dat.under_water),
    name(upcaseof(dat.name)) {
  }

WayPoint::WayPoint(const Vec3& pos, std::string_view name)
  :x(pos.x),y(pos.y),z(pos.z),freePoint(true),name(upcaseof(name)){
  }

WayPoint::WayPoint(const Vec3& pos, const Vec3& dir, std::string_view name, bool isFp)
  :x(pos.x), y(pos.y), z(pos.z), dir(dir), freePoint(isFp), name(upcaseof(name)){
  }

bool WayPoint::isFreePoint() const {
  return freePoint;
  }

bool WayPoint::isConnected() const {
  return conn.size()<1;
  }

bool WayPoint::checkName(std::string_view n, bool inexact) const {
  if(n.empty())
    return false;
  if(name==n)
    return true;
  if(inexact && name.find(n)!=std::string::npos)
    return true;
  return false;
  }

Vec3 WayPoint::position() const {
  return {x,y,z};
  }

float WayPoint::qDistTo(float ix, float iy, float iz) const {
  float dx = x-ix;
  float dy = y-iy;
  float dz = z-iz;
  return dx*dx+dy*dy+dz*dz;
  }

void WayPoint::connect(WayPoint &w) {
  int32_t l = int32_t(std::sqrt(qDistTo(w.x,w.y,w.z)));
  if(l<=0)
    return;
  Conn c;
  c.point = &w;
  c.len   = l;
  conn.push_back(c);
  }

bool WayPoint::hasLadderConn(const WayPoint* w) const {
  if(w!=nullptr && w->ladder!=nullptr && w->ladder==ladder)
    return true;
  return false;
  }

std::string WayPoint::upcaseof(std::string_view src) {
  auto ret = std::string(src);
  for(auto& i:ret)
    i=char(std::toupper(i));
  return ret;
  }
