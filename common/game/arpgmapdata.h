#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Serialize;

// Diathic: what the hero has seen of each world (4 m cells) and his own pins on the map; saved with the game ("game/arpgmap")
class ArpgMapData final {
  public:
    static constexpr float Cell = 400.f; // cm

    struct Pin {
      float       x = 0, z = 0; // world position (cm)
      std::string note;         // UTF-8
      };

    void clear();
    void save(Serialize& fout) const;
    void load(Serialize& fin);

    // everything within 'radius' of (x,z) becomes explored
    void explore(std::string_view world, float x, float z, float radius);
    bool isExplored(std::string_view world, int i, int j) const;
    const std::unordered_set<int64_t>* explored(std::string_view world) const;

    std::vector<Pin>& pins(std::string_view world);

    static int64_t key(int i, int j) { return (int64_t(i)<<32) ^ int64_t(uint32_t(j)); }

  private:
    std::unordered_map<std::string, std::unordered_set<int64_t>> cells;
    std::unordered_map<std::string, std::vector<Pin>>            pinMap;
    // last explore() call: the same cell again does nothing
    std::string lastWorld;
    int64_t     lastKey = -1;
  };
