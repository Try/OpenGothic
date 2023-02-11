#include "waypoint.h"

#include <cmath>
#include <cstring>
#include <cctype>

using namespace Tempest;

WayPoint::WayPoint() {
  }

WayPoint::WayPoint(const phoenix::way_point &dat)
  : x(dat.position.x),y(dat.position.y),z(dat.position.z),
    dirX(dat.direction.x),dirY(dat.direction.y),dirZ(dat.direction.z),
    underWater(dat.under_water),
    name(upcaseof(dat.name)) {
  }

WayPoint::WayPoint(const Vec3& pos, std::string_view name)
  :x(pos.x),y(pos.y),z(pos.z),name(upcaseof(name)){
  }

WayPoint::WayPoint(const Vec3& pos, const Vec3& dir, std::string_view name)
  :x(pos.x),y(pos.y),z(pos.z),dirX(dir.x),dirY(dir.y),dirZ(dir.z),name(upcaseof(name)){
  }

bool WayPoint::isFreePoint() const {
  return conn.size()<1;
  }

bool WayPoint::checkName(std::string_view n) const {
  const char*  src = name.c_str();
  const size_t len = n.size();

  if(n.size()<=name.size())
    if(std::memcmp(name.data(),n.data(),n.size())==0)
      return true;

  for(size_t i=0, i0=0; ; ++i) {
    if(src[i]=='_' || src[i]=='\0') {
      const size_t len2 = i-i0;
      if(len==len2 && std::memcmp(&src[i0],n.data(),len2)==0)
        return true;
      i0=i+1;
      }
    if(src[i]=='\0')
      break;
    }

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

std::string WayPoint::upcaseof(std::string_view src) {
  auto ret = std::string(src);
  for(auto& i:ret)
    i=char(std::toupper(i));
  return ret;
  }
