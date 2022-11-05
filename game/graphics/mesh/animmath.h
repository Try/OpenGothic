#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <phoenix/animation.hh>

phoenix::animation_sample mix(const phoenix::animation_sample& x,const phoenix::animation_sample& y,float a);
Tempest::Matrix4x4        mkMatrix(const phoenix::animation_sample& s);
