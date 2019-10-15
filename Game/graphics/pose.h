#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include <mutex>
#include <memory>

#include "animation.h"

class Skeleton;
class Serialize;
class AnimationSolver;
class Npc;

class Pose final {
  public:
    Pose()=default;
    Pose(const Skeleton& sk,const Animation::Sequence* sq0,const Animation::Sequence* sq1);

    void               save(Serialize& fout);
    void               load(Serialize& fin, const AnimationSolver &solver);

    void               setSkeleton(const Skeleton *sk);
    bool               startAnim(const AnimationSolver &solver, const Animation::Sequence* sq, uint64_t tickCount);
    bool               stopAnim(const char* name);
    void               reset(const Skeleton& sk,uint64_t tickCount,const Animation::Sequence* sq0,const Animation::Sequence* sq1);
    void               update(uint64_t tickCount);

    ZMath::float3      animMoveSpeed(uint64_t tickCount, uint64_t dt) const;
    void               emitSfx(Npc &npc, uint64_t tickCount);
    void               processEvents(uint64_t& barrier, uint64_t now, Animation::EvCount &ev) const;
    bool               isParWindow(uint64_t tickCount) const;
    bool               isAtackFinished(uint64_t tickCount) const;
    bool               isFlyAnim() const;
    bool               isIdle() const;
    bool               isInAnim(const char *sq) const;
    bool               isInAnim(const Animation::Sequence* sq) const;
    uint64_t           animationTotalTime() const;

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

    void addLayer(const Animation::Sequence* seq, uint64_t tickCount);
    template<class T,class F>
    void removeIf(T& t,F f);

    struct Layer final {
      const Animation::Sequence* seq   = nullptr;
      uint64_t                   frame = uint64_t(-1);
      uint64_t                   sAnim = 0;
      };
    const Skeleton*            skeleton=nullptr;
    std::vector<Layer>         lay;
    float                      trY=0;
  };
