#pragma once

#include <Tempest/Point>

#include <cstdint>
#include <unordered_map>
#include <vector>

// Diathic: walking on a click around furniture (tables, chairs, benches, crates, shelves).
// A grid of small cells around the hero, tested only where the search goes (A*): a cell is free when there is ground
// under it and nothing where the hero's body would be (a sphere swept down over it); two cells are joined when the
// ground does not jump between them and nothing stands between them at knee and chest height.
// The straight walk is tested on the same cells, so a table at 80 cm is no longer "clear" for a ray at 90 cm.
class ArpgNav final {
  public:
    static constexpr float Cell   = 25.f;  // cm
    static constexpr float Radius = 32.f;  // cm: room the hero needs around his middle
    static constexpr float Step   = 40.f;  // cm: the most the ground may go up / down between two cells

    // what the grid asks the world (the physics; a test scene in tests)
    struct Scene {
      virtual ~Scene() = default;
      virtual bool        ray (const Tempest::Vec3& from, const Tempest::Vec3& to, Tempest::Vec3& hit) const = 0;
      virtual bool        sweepDown(const Tempest::Vec3& from, const Tempest::Vec3& to, float r) const = 0;
      virtual uint64_t    time() const = 0;   // ms
      virtual const void* id()   const = 0;   // the world
      };

    // the cells are tested again (a new click, another world, now and then: doors, things moved)
    void reset();
    // a straight walk without bumping into anything
    bool lineClear(const Scene& w, const Tempest::Vec3& from, const Tempest::Vec3& to);
    // a way to a place within 'reach' of dest (flat distance), around things in the way; false: none close by
    bool findPath(const Scene& w, const Tempest::Vec3& from, const Tempest::Vec3& dest, float reach,
                  std::vector<Tempest::Vec3>& out);
    // something the hero bumped into, the grid did not see: the cell is closed for this click
    void block(const Tempest::Vec3& at);

  private:
    struct CellInfo {
      float   y     = 0;
      uint8_t state = 0; // 0 not tested, 1 free, 2 blocked, 3 no ground
      };
    std::unordered_map<int64_t, CellInfo> cells;
    const void*  owner = nullptr;
    uint64_t     made  = 0;

    static int64_t key(int i, int j) { return (int64_t(i)<<32) ^ int64_t(uint32_t(j)); }
    static int     cellOf(float v);
    static float   center(int i);

    const CellInfo& probe(const Scene& w, int i, int j, float nearY);
    bool            link (const Scene& w, int i0, int j0, float y0, int i1, int j1, bool soft);
    void            sync (const Scene& w);
  };
