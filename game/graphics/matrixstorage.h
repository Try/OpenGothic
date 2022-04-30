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

  public:
    class Id {
      public:
        Id(MatrixStorage& owner, Range rgn):owner(&owner), rgn(rgn){}
        Id(Id&& other);
        Id& operator = (Id&& other);
        ~Id();

        const uint32_t offsetId() const { return uint32_t(rgn.begin); }
        void           set(const Tempest::Matrix4x4& obj, const Tempest::Matrix4x4* anim);

      private:
        MatrixStorage* owner = nullptr;
        Range          rgn;
      };

    Id                            alloc(size_t nbones);
    const Tempest::StorageBuffer& ssbo(uint8_t fId) const;
    bool                          commitUbo(uint8_t fId);

  private:
    void free(const Range& r);
    void set (const Range& rgn, const Tempest::Matrix4x4& obj, const Tempest::Matrix4x4* mat);

    std::vector<Range>              rgn;
    std::vector<Tempest::Matrix4x4> data;

    Tempest::StorageBuffer          gpu[Resources::MaxFramesInFlight];
  };
