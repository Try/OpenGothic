#pragma once

#include <vector>
#include <memory>

#include "animation.h"
#include "pose.h"

class Skeleton;

class PosePool final {
  public:
    static std::shared_ptr<Pose> get(const Skeleton* s,const Animation::Sequence *sq,uint64_t sT);

  private:
    struct Inst {
      Inst(const Skeleton &s, const Animation::Sequence *sq, uint64_t sTime);
      Pose     pose;
      uint64_t sTime=0;
      };

    struct Chunk {
      Chunk(const Skeleton& s,const Animation::Sequence* sq):skeleton(&s),sq(sq){}

      const Skeleton*                    skeleton=nullptr;
      const Animation::Sequence*         sq      =nullptr;
      std::vector<std::shared_ptr<Inst>> timed;
      };

    std::vector<Chunk> chunks;

    PosePool();
    static PosePool&      inst();
    Chunk&                findChunk(const Skeleton &s, const Animation::Sequence *sq);
    std::shared_ptr<Pose> find(const Skeleton &s, const Animation::Sequence &sq, uint64_t sT);
    std::shared_ptr<Pose> find(const Skeleton &s);
  };
