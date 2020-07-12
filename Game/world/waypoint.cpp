#include "waypoint.h"

#include <cmath>
#include <cstring>
#include <cctype>

using namespace Tempest;

WayPoint::WayPoint() {
  }

WayPoint::WayPoint(const ZenLoad::zCWaypointData &dat)
  : x(dat.position.x),y(dat.position.y),z(dat.position.z),
    dirX(dat.direction.x),dirY(dat.direction.y),dirZ(dat.direction.z),
    name(upcaseof(dat.wpName)){
  }

WayPoint::WayPoint(const Vec3& pos, const char *name)
  :x(pos.x),y(pos.y),z(pos.z),name(upcaseof(name)){
  }

WayPoint::WayPoint(const Vec3& pos, const Vec3& dir, const char *name)
  :x(pos.x),y(pos.y),z(pos.z),dirX(dir.x),dirY(dir.y),dirZ(dir.z),name(upcaseof(name)){
  }

bool WayPoint::isFreePoint() const {
  return conn.size()<1;
  }

bool WayPoint::checkName(const std::string &name) const {
  return checkName(name.c_str());
  }

bool WayPoint::checkName(const char *n) const {
  if(name.find(n)!=std::string::npos)
    return true;
  if(std::strcmp(n,"STAND")==0) {
    return name.find("_PATH_")==std::string::npos;
    }
  return false;
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

std::string WayPoint::upcaseof(const std::string &src) {
  auto ret = src;
  for(auto& i:ret)
    i=char(std::toupper(i));
  return ret;
  }
