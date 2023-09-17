#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <vector>

#include "resources.h"

class InstanceStorage {
  private:
    struct Range {
      size_t begin = 0;
      size_t size  = 0;
      size_t asize = 0;
      };
    struct Heap;

  public:
    class Id {
      public:
        Id() = default;
        Id(Heap& heap, Range rgn):heapPtr(&heap), rgn(rgn){}
        Id(Id&& other) noexcept;
        Id& operator = (Id&& other) noexcept;
        ~Id();

        const size_t   size() const     { return rgn.asize;           }
        const uint32_t offsetId() const { return uint32_t(rgn.begin)/sizeof(Tempest::Matrix4x4); }
        void           set(const Tempest::Matrix4x4* anim);
        void           set(const Tempest::Matrix4x4& obj, size_t offset);

        const Tempest::StorageBuffer& ssbo(uint8_t fId) const;
        Tempest::BufferHeap           heap() const;

      private:
        Heap* heapPtr = nullptr;
        Range rgn;
      };

    InstanceStorage();

    Id   alloc(Tempest::BufferHeap heap, const size_t size);
    auto ssbo (Tempest::BufferHeap heap, uint8_t fId) const -> const Tempest::StorageBuffer&;
    bool commit(uint8_t fId);

  private:
    static constexpr size_t alignment = 64;

    bool commit(Heap& heap, uint8_t fId);
    void free(Heap& heap, const Range& r);

    struct Heap {
      InstanceStorage*                  owner = nullptr;
      std::vector<Range>              rgn;
      std::vector<uint8_t>            data;
      Tempest::StorageBuffer          gpu[Resources::MaxFramesInFlight];
      std::atomic_bool                durty[Resources::MaxFramesInFlight] = {};
      };
    Heap upload, device;
  };
