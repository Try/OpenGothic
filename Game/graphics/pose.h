#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include "animation.h"

class Skeleton;

class Pose {
  public:
    void bind(const Skeleton* sk);
    void update(const Animation::Sequence& s,uint64_t dt);

    std::vector<Tempest::Matrix4x4> tr;
    std::vector<Tempest::Matrix4x4> base;

  private:
    const Skeleton* skeleton=nullptr;

    void mkSkeleton();
    void mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent);
  };
