#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <future>
#include <vector>

#include "resources.h"

class InstanceStorage {
  private:
    struct Range {
      size_t begin = 0;
      size_t size  = 0;
      size_t asize = 0;
      };

    static constexpr size_t blockSz   = 64;
    static constexpr size_t alignment = 64;

  public:
    class Id {
      public:
        Id() = default;
        Id(InstanceStorage& owner, Range rgn):owner(&owner), rgn(rgn){}
        Id(Id&& other) noexcept;
        Id& operator = (Id&& other) noexcept;
        ~Id();

        const size_t   size() const { return rgn.asize; }
        void           set(const Tempest::Matrix4x4* anim);
        void           set(const Tempest::Matrix4x4& obj, size_t offset);
        void           set(const void* data, size_t offset, size_t size);

        template<class T>
        const uint32_t offsetId() const { return uint32_t(rgn.begin/sizeof(T)); }

        bool           isEmpty() const { return rgn.asize==0; }

      private:
        InstanceStorage* owner = nullptr;
        Range            rgn;
      friend class InstanceStorage;
      };

    InstanceStorage();
    ~InstanceStorage();

    Id   alloc(const size_t size);
    bool realloc(Id& id, const size_t size);
    auto ssbo () const -> const Tempest::StorageBuffer&;
    bool commit(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void join();

  private:
    void free(const Range& r);
    void uploadMain();
    void prepareUniforms();

    struct Path {
      uint32_t dst;
      uint32_t src;
      uint32_t size;
      };

    struct DeleteLater {
      std::vector<Tempest::StorageBuffer> ssbo;
      };

    std::vector<Range>      rgn;
    std::vector<uint32_t>   durty;
    size_t                  blockCnt = 0;

    Tempest::StorageBuffer  patchGpu[Resources::MaxFramesInFlight];
    std::vector<uint8_t>    patchCpu;
    std::vector<Path>       patchBlock;

    Tempest::StorageBuffer  dataGpu;
    std::vector<uint8_t>    dataCpu;

    Tempest::DescriptorSet  desc[Resources::MaxFramesInFlight];

    std::thread             uploadTh;
    std::mutex              sync;
    std::condition_variable uploadCnd;
    int32_t                 uploadFId = -1;
  };
