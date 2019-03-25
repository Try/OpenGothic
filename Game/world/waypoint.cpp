#include "waypoint.h"

#include <cmath>

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

float WayPoint::qDistTo(float ix, float iy, float iz) const {
  float dx = x-ix;
  float dy = y-iy;
  float dz = z-iz;
  return dx*dx+dy*dy+dz*dz;
  }

void WayPoint::connect(WayPoint &w) {
  float l = std::sqrt(qDistTo(w.x,w.y,w.z));
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
