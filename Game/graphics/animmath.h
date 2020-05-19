#pragma once

#include <zenload/zTypes.h>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>

ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a);
Tempest::Matrix4x4        mkMatrix(const ZenLoad::zCModelAniSample& s);
