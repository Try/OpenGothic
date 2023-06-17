#include "visibleset.h"

VisibleSet::VisibleSet() {
  }

void VisibleSet::reset() {
  for(auto& i:cnt)
    i.store(0);
  }

void VisibleSet::push(size_t index, SceneGlobals::VisCamera v) {
  int i = cnt[v].fetch_add(1, std::memory_order_acq_rel);
  id[v][i] = index;
  }

void VisibleSet::erase(size_t objId) {
  for(size_t v=0; v<SceneGlobals::V_Count; ++v) {
    size_t count = size_t(cnt[v].load());
    for(size_t i=0; i<count; ++i) {
      if(id[v][i]!=objId)
        continue;
      id[v][i] = id[v][count-1];
      count--;
      }
    cnt[v].store(std::uint_least8_t(count));
    }
  }

void VisibleSet::sort(SceneGlobals::VisCamera v) {
  uint8_t indSz = uint8_t(cnt[v].load());
  size_t* index = id[v];
  std::sort(index,index+indSz);
  }

void VisibleSet::minmax(SceneGlobals::VisCamera v) {
  uint8_t indSz = uint8_t(cnt[v].load());
  size_t* index = id[v];
  size_t& last  = index[indSz-1];

  for(size_t i=0; i<indSz; ++i) {
    if(index[0]>index[i])
      std::swap(index[0],index[i]);
    if(last<index[i])
      std::swap(last,index[i]);
    }
  }
