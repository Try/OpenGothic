#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include "animation.h"

class Skeleton;

class Pose {
  public:
    void bind(const Skeleton* sk);
    void update(const Animation::Sequence& s,uint64_t dt);

    float translateY() const { return trY; }

    std::vector<Tempest::Matrix4x4> tr;
    std::vector<Tempest::Matrix4x4> base;

  private:
    const Skeleton* skeleton=nullptr;
    float trY=0;

    void mkSkeleton(const Animation::Sequence &s);
    void mkSkeleton(const Tempest::Matrix4x4 &mt);
    void mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent);
  };
