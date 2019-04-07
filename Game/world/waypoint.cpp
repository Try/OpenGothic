#include "waypoint.h"

#include <cmath>
#include <cstring>

WayPoint::WayPoint() {
  }

WayPoint::WayPoint(const ZenLoad::zCWaypointData &dat)
  : x(dat.position.x),y(dat.position.y),z(dat.position.z),
    dirX(dat.direction.x),dirY(dat.direction.y),dirZ(dat.direction.z),
    name(upcaseof(dat.wpName)){
  }

WayPoint::WayPoint(float x, float y, float z, const char *name)
  :x(x),y(y),z(z),name(upcaseof(name)){
  }

WayPoint::WayPoint(float x, float y, float z, float dx, float dy, float dz, const char *name)
  :x(x),y(y),z(z),dirX(dx),dirY(dy),dirZ(dz),name(upcaseof(name)){
  }

bool WayPoint::checkName(const std::string &name) const {
  return checkName(name.c_str());
  }

bool WayPoint::checkName(const char *n) const {
  if(name.find(n)!=std::string::npos)
    return true;
  return std::strcmp(n,"STAND")==0;
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
  conn.push_back({&w,l});
  }

std::string WayPoint::upcaseof(const std::string &src) {
  auto ret = src;
  for(auto& i:ret)
    i=char(std::toupper(i));
  return ret;
  }
