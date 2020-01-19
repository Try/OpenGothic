#pragma once

#include <zenload/zTypes.h>
#include <Tempest/Matrix4x4>

ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a);
Tempest::Matrix4x4        mkMatrix(const ZenLoad::zCModelAniSample& s);

inline std::array<float,3> crossVec3(const std::array<float,3>& a,const std::array<float,3>& b){
  std::array<float,3> c = {
    a[1]*b[2] - a[2]*b[1],
    a[2]*b[0] - a[0]*b[2],
    a[0]*b[1] - a[1]*b[0]
    };
  return c;
  }
