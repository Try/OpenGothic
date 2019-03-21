#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include <mutex>
#include <memory>

#include "animation.h"

class Skeleton;

class Pose final {
  public:
    Pose()=default;
    Pose(const Skeleton& sk,const Animation::Sequence* sq0,const Animation::Sequence* sq1);

    void update(uint64_t dt);

    float              translateY() const { return trY; }
    Tempest::Matrix4x4 cameraBone() const;

    std::vector<Tempest::Matrix4x4> tr;
    std::vector<Tempest::Matrix4x4> base;

  private:
    void mkSkeleton(const Animation::Sequence &s);
    void mkSkeleton(const Tempest::Matrix4x4 &mt);
    void mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent);
    void zeroSkeleton();

    bool update(const Animation::Sequence &s, uint64_t dt, uint64_t &fr);
    void updateFrame(const Animation::Sequence &s,uint64_t fr);

    const Skeleton*            skeleton=nullptr;
    const Animation::Sequence* sequence=nullptr;
    const Animation::Sequence* baseSq  =nullptr;
    uint64_t                   frSequence=uint64_t(-1);
    uint64_t                   frBase    =uint64_t(-1);
    float                      trY=0;

    uint64_t                   lastT=uint64_t(-1);
    uint32_t                   numFrames=0;

    std::mutex                 sync;
  };
