#include "arpgmapdata.h"

#include <cmath>

#include "game/serialize.h"

void ArpgMapData::clear() {
  cells.clear();
  pinMap.clear();
  lastWorld.clear();
  lastKey = -1;
  }

void ArpgMapData::save(Serialize& fout) const {
  const uint8_t ver = 1;
  fout.write(ver, uint32_t(cells.size()));
  for(auto& w:cells) {
    std::vector<uint64_t> keys;
    keys.reserve(w.second.size());
    for(auto k:w.second)
      keys.push_back(uint64_t(k));
    fout.write(w.first, keys);
    }
  fout.write(uint32_t(pinMap.size()));
  for(auto& w:pinMap) {
    fout.write(w.first, uint32_t(w.second.size()));
    for(auto& p:w.second)
      fout.write(p.x, p.z, p.note);
    }
  }

void ArpgMapData::load(Serialize& fin) {
  clear();
  uint8_t  ver = 0;
  uint32_t n   = 0;
  fin.read(ver, n);
  for(uint32_t i=0; i<n; ++i) {
    std::string           name;
    std::vector<uint64_t> keys;
    fin.read(name, keys);
    auto& set = cells[name];
    set.reserve(keys.size());
    for(auto k:keys)
      set.insert(int64_t(k));
    }
  fin.read(n);
  for(uint32_t i=0; i<n; ++i) {
    std::string name;
    uint32_t    cnt = 0;
    fin.read(name, cnt);
    auto& v = pinMap[name];
    v.resize(cnt);
    for(auto& p:v)
      fin.read(p.x, p.z, p.note);
    }
  }

void ArpgMapData::explore(std::string_view world, float x, float z, float radius) {
  const int ci = int(std::floor(x/Cell)), cj = int(std::floor(z/Cell));
  if(lastWorld==world && lastKey==key(ci,cj))
    return;
  lastWorld = std::string(world);
  lastKey   = key(ci,cj);

  auto&     set = cells[lastWorld];
  const int n   = int(std::ceil(radius/Cell));
  for(int j=-n; j<=n; ++j)
    for(int i=-n; i<=n; ++i) {
      const float dx = (float(ci+i)+0.5f)*Cell - x;
      const float dz = (float(cj+j)+0.5f)*Cell - z;
      if(dx*dx+dz*dz<=radius*radius)
        set.insert(key(ci+i, cj+j));
      }
  }

bool ArpgMapData::isExplored(std::string_view world, int i, int j) const {
  auto s = explored(world);
  return s!=nullptr && s->find(key(i,j))!=s->end();
  }

const std::unordered_set<int64_t>* ArpgMapData::explored(std::string_view world) const {
  auto it = cells.find(std::string(world));
  return it==cells.end() ? nullptr : &it->second;
  }

std::vector<ArpgMapData::Pin>& ArpgMapData::pins(std::string_view world) {
  return pinMap[std::string(world)];
  }
