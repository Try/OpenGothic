#include "waypath.h"

#include <cstdint>

WayPath::WayPath() {
  }

void WayPath::clear() {
  dat.clear();
  }

const WayPoint *WayPath::pop() {
  if(dat.size()==0)
    return nullptr;
  auto ret = dat[dat.size()-1];
  dat.pop_back();
  return ret;
  }
