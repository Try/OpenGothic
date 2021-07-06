#pragma once

#include <cstdint>
#include <atomic>

#include "graphics/sceneglobals.h"

class VisibleSet {
  public:
    VisibleSet();

    enum {
      CAPACITY     = 256,
      };

    void reset();
    void push(size_t id, SceneGlobals::VisCamera v);

    size_t        count(SceneGlobals::VisCamera v) const { return cnt[v].load(); }
    const size_t* index(SceneGlobals::VisCamera v) const { return id[v]; }

  private:
    std::atomic_int cnt[SceneGlobals::V_Count]           = {};
    size_t          id [SceneGlobals::V_Count][CAPACITY] = {};
  };

