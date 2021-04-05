#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <vector>

#include "resources.h"

class Pose;

class SkeletalStorage {
  struct Ubo96 {
    Tempest::Matrix4x4 mat[Resources::MAX_NUM_SKELETAL_NODES];
    };
  public:
    SkeletalStorage();
    ~SkeletalStorage();

    class AnimationId final {
      public:
        AnimationId() = default;
        AnimationId(SkeletalStorage& owner, size_t id, size_t boneCnt);
        AnimationId(AnimationId&& other);
        AnimationId& operator = (AnimationId&& other);
        ~AnimationId();

        void   setPose(const Pose& p);
        void   bind(Tempest::Uniforms& desc, uint8_t bind, uint8_t fId) const;

      private:
        SkeletalStorage* owner   = nullptr;
        size_t           id      = 0;
        size_t           boneCnt = 0;
      };

    bool                     commitUbo(uint8_t fId);
    size_t                   alloc(size_t bonesCount);
    void                     free(const size_t objId, size_t bonesCount);

    void                     bind(Tempest::Uniforms& desc, uint8_t bind, uint8_t fId, size_t id, size_t boneCnt);

    void                     markAsChanged(size_t elt);
    Tempest::Matrix4x4&      element(size_t i);

    void                     reserve(size_t sz);

  private:
    struct Impl;

    template<size_t BlkSz>
    struct TImpl;

    template<size_t sz, size_t BlkSz>
    struct FreeList;

    template<size_t sz>
    bool                            tryInit();

    std::unique_ptr<Impl>           impl;
    size_t                          blockSize;
    size_t                          updatesTotal=0; // perf statistic
  };

