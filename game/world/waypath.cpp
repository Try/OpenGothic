#include "waypath.h"

#include "game/serialize.h"
#include <cstdint>

WayPath::WayPath() {
  }

void WayPath::load(Serialize &fin) {
  uint32_t sz=0;
  fin.read(sz);
  dat.resize(sz);

  for(auto& i:dat)
    fin.read(i);
  }

void WayPath::save(Serialize &fout) {
  fout.write(uint32_t(dat.size()));

  for(auto& i:dat)
    fout.write(i);
  }

void WayPath::clear() {
  dat.clear();
  }

void WayPath::reverse() {
  for(size_t i=0; i<dat.size()/2; ++i) {
    std::swap(dat[i], dat[dat.size()-i-1]);
    }
  }

const WayPoint *WayPath::pop() {
  if(dat.size()==0)
    return nullptr;
  auto ret = dat[dat.size()-1];
  dat.pop_back();
  return ret;
  }

const WayPoint *WayPath::last() const {
  if(dat.size()==0)
    return nullptr;
  return dat[0];
  }
