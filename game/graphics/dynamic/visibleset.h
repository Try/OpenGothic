#pragma once

#include <cstdint>
#include <atomic>

#include "graphics/sceneglobals.h"

class VisibleSet {
  public:
    VisibleSet();

    enum {
      CAPACITY     = 255,
      };

    void reset();
    void push(size_t id, SceneGlobals::VisCamera v);

    size_t        count(SceneGlobals::VisCamera v) const { return size_t(cnt[v].load()); }
    const size_t* index(SceneGlobals::VisCamera v) const { return id[v];                 }

    void          erase(size_t id);
    void          sort(SceneGlobals::VisCamera v);
    void          minmax(SceneGlobals::VisCamera v);

  private:
    std::atomic_uint_least8_t cnt[SceneGlobals::V_Count]           = {};
    size_t                    id [SceneGlobals::V_Count][CAPACITY] = {};
  };

