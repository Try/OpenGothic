#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <vector>

#include "resources.h"

class MatrixStorage {
  private:
    struct Range {
      size_t begin = 0;
      size_t size  = 0;
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

        const size_t   size() const     { return rgn.size;            }
        const uint32_t offsetId() const { return uint32_t(rgn.begin); }
        void           set(const Tempest::Matrix4x4* anim);
        void           set(const Tempest::Matrix4x4& obj, size_t offset);

        const Tempest::StorageBuffer& ssbo(uint8_t fId) const;
        Tempest::BufferHeap           heap() const;

      private:
        Heap* heapPtr = nullptr;
        Range rgn;
      };

    MatrixStorage();

    Id   alloc(Tempest::BufferHeap heap, size_t nbones);
    auto ssbo (Tempest::BufferHeap heap, uint8_t fId) const -> const Tempest::StorageBuffer&;
    bool commit(uint8_t fId);

  private:
    bool commit(Heap& heap, uint8_t fId);
    void free(Heap& heap, const Range& r);

    struct Heap {
      MatrixStorage*                  owner = nullptr;
      std::vector<Range>              rgn;
      std::vector<Tempest::Matrix4x4> data;
      Tempest::StorageBuffer          gpu[Resources::MaxFramesInFlight];
      std::atomic_bool                durty[Resources::MaxFramesInFlight] = {};
      };
    Heap upload, device;
  };
