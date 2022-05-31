#pragma once

#include <zenload/zTypes.h>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <phoenix/animation.hh>

ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a);
phoenix::animation_sample mix(const phoenix::animation_sample& x,const phoenix::animation_sample& y,float a);
Tempest::Matrix4x4        mkMatrix(const ZenLoad::zCModelAniSample& s);
Tempest::Matrix4x4        mkMatrix(const phoenix::animation_sample& s);
