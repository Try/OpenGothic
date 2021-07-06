#include "visibleset.h"

VisibleSet::VisibleSet() {
  }

void VisibleSet::reset() {
  for(auto& i:cnt)
    i.store(0);
  }

void VisibleSet::push(size_t index, SceneGlobals::VisCamera v) {
  int i = cnt[v].fetch_add(1);
  id[v][i] = index;
  }
