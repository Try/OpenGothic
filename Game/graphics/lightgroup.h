#pragma once

#include <memory>

#include "bounds.h"
#include "light.h"

class LightGroup final {
  public:
    LightGroup();

    size_t size() const;

    void   set(const std::vector<Light>& light);
    size_t get(const Bounds& area, const Light** out, size_t maxOut) const;
    void   tick(uint64_t time);

  private:
    struct Bvh {
      std::unique_ptr<Bvh> next[2];
      Bounds               bbox;
      const Light*         b = nullptr;
      size_t               count=0;
      };

    size_t      implGet(const Bvh& index, const Bounds& area, const Light** out, size_t maxOut) const;
    void        mkIndex(Bvh& id, Light* b, size_t count, int depth);
    static bool isIntersected(const Bounds& a,const Bounds& b);

    std::vector<Light>  light;
    std::vector<Light*> dynamicState;
    Bvh                 index;
  };

