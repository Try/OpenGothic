#pragma once

#include <zenload/zTypes.h>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>

ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a);
Tempest::Matrix4x4        mkMatrix(const ZenLoad::zCModelAniSample& s);

inline Tempest::Vec3 crossVec3(const Tempest::Vec3& a,const Tempest::Vec3& b){
  Tempest::Vec3 c = {
    a.y*b.z - a.z*b.y,
    a.z*b.x - a.x*b.z,
    a.x*b.y - a.y*b.x
    };
  return c;
  }
