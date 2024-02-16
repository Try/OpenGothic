#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <zenkit/ModelAnimation.hh>

zenkit::AnimationSample mix(const zenkit::AnimationSample& x, const zenkit::AnimationSample& y, float a);
Tempest::Matrix4x4      mkMatrix(const zenkit::AnimationSample& s);
