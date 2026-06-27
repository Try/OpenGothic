# Wld_DetectItem uses a sphere instead of the original's perception cube

**Confidence:** Medium

## Original function + address
- `Wld_DetectItem` handler `FUN_006e0e40` pops the `flags` argument and the implicit
  `self` C_Npc, then calls `oCNpc::DetectItem(self, flags, 0)`.
- `oCNpc::DetectItem` @ `0x0073fd40` does **not** take or apply any radius. It walks the
  NPC's perception vob-list (array at `oCNpc+0x998`, count at `oCNpc+0x9a0`), and for each
  entry that is an `oCItem`, skips it when `oCItem::HasFlag(0x800000)` is set, accepts it when
  `(flags & (item.flags | item.main_flag)) != 0`, and keeps the one with the smallest
  `zCVob::GetDistanceToVob2`. The set it scans is whatever currently sits in the perception
  list — there is no distance test inside `DetectItem` itself.
- That perception list is built by `oCNpc::CreateVobList(float)` @ `0x0075da40` (driven by
  `oCNpc::PerceiveAll` @ `0x0075dbe0`, which passes the perception range from `oCNpc+0x284`,
  i.e. the int `senses_range` cast to float). `CreateVobList` collects vobs via
  `zCBspBase::CollectVobsInBBox3D` over the **axis-aligned box** `[pos - range, pos + range]`
  on every axis — a cube of half-extent `senses_range`, not a sphere.

Net effect of the original: an item is a detection candidate when it lies inside the cube of
half-side `senses_range` centered on the NPC.

## OpenGothic file:line
- `game/game/gamescript.cpp:1891` — `GameScript::wld_detectitem` calls
  `world().detectItem(npc->position(), float(npc->handle().senses_range), ...)`.
- `game/world/worldobjects.cpp:506-514` — `WorldObjects::detectItem` accepts/forwards items
  whose center is within a **sphere**: `(item.pos - p).quadLength() < r*r`.

## Divergence
OpenGothic limits Wld_DetectItem to a sphere of radius `senses_range`, while the original
considers a cube of half-extent `senses_range`. The sphere is strictly inscribed in the cube,
so items located in the cube's corners — at distances between `senses_range` and
`senses_range * sqrt(3)` along diagonal directions — are detected by `Gothic2.exe` but missed
by OpenGothic. For a typical `senses_range` of ~1500-2000, that is a band roughly 1500-3460
units out in diagonal directions where OG under-detects. `World::detectItem`'s sphere
broadphase is used only by this external (sole caller is `gamescript.cpp:1891`), so the change
is contained.

Secondary, intentionally-out-of-scope differences (do not "fix" here): the original picks the
nearest by bounding-box distance (`GetDistanceToVob2`) rather than center distance, and its
perception list is rebuilt only periodically by `PerceiveAll` (so it can be momentarily stale)
whereas OG evaluates the live item set per call. These are approximations on both sides and are
not addressed by this patch; only the search-volume shape is corrected.

## Proposed patch
File `game/game/gamescript.cpp`, in `GameScript::wld_detectitem` (the existing flag-handling
`// NOTE` block is kept verbatim).

OLD:
```cpp
  Item* ret =nullptr;
  float dist=std::numeric_limits<float>::max();
  world().detectItem(npc->position(), float(npc->handle().senses_range), [npc,&ret,&dist,flags](Item& it) {
    // NOTE: in original-game oCNpc::DetectItem (Gothic2.exe 0x0073fd40) tests the mask against
    // (main_flag | flags) -- category bits live in main_flag, weapon/wear subtype bits (SWD,
    // BOW, TORCH, RING, ...) live in flags -- and skips items carrying the 0x800000 no-detect
    // flag. OpenGothic checked only main_flag, so subtype-mask detections never matched.
    if((uint32_t(it.handle().flags) & 0x800000u)!=0)
      return;
    if(((uint32_t(it.handle().main_flag)|uint32_t(it.handle().flags)) & uint32_t(flags))==0)
      return;
    float d = (npc->position()-it.position()).quadLength();
    if(d<dist) {
      ret = &it;
      dist= d;
      }
    });
```

NEW:
```cpp
  Item* ret =nullptr;
  float dist=std::numeric_limits<float>::max();
  const float range = float(npc->handle().senses_range);
  // NOTE: in original-game oCNpc::DetectItem (Gothic2.exe 0x0073fd40) scans the npc's perception
  // vob-list, which oCNpc::CreateVobList (0x0075da40, driven by PerceiveAll 0x0075dbe0 with the
  // range from npc+0x284 == senses_range) gathers via CollectVobsInBBox3D over the axis-aligned
  // cube [pos-range, pos+range] -- not a sphere. OpenGothic used a sphere of radius senses_range,
  // so items in the cube corners (distance senses_range .. senses_range*sqrt(3)) were never
  // detected. Widen the broadphase and apply the axis-aligned box test to match the cube.
  world().detectItem(npc->position(), range*1.7320508f, [npc,range,&ret,&dist,flags](Item& it) {
    // NOTE: in original-game oCNpc::DetectItem (Gothic2.exe 0x0073fd40) tests the mask against
    // (main_flag | flags) -- category bits live in main_flag, weapon/wear subtype bits (SWD,
    // BOW, TORCH, RING, ...) live in flags -- and skips items carrying the 0x800000 no-detect
    // flag. OpenGothic checked only main_flag, so subtype-mask detections never matched.
    if((uint32_t(it.handle().flags) & 0x800000u)!=0)
      return;
    if(((uint32_t(it.handle().main_flag)|uint32_t(it.handle().flags)) & uint32_t(flags))==0)
      return;
    const auto d3 = npc->position()-it.position();
    if(std::abs(d3.x)>=range || std::abs(d3.y)>=range || std::abs(d3.z)>=range)
      return;
    float d = d3.quadLength();
    if(d<dist) {
      ret = &it;
      dist= d;
      }
    });
```

Grep-verified symbols: `npc->handle().senses_range` (already used at `gamescript.cpp:1891`),
`World::detectItem(const Vec3&, float, const std::function<void(Item&)>&)` (`world.h:75`),
`Item::position()`, `Vec3::x/.y/.z` (used at `world.cpp:991`), `Vec3::quadLength()`,
`std::abs` (already used at `gamescript.cpp:2418`/`2952`). The wider broadphase radius keeps the
existing sphere prefilter from discarding corner items before the exact box test runs; the only
caller of `World::detectItem` is this external, so behavior elsewhere is unchanged.
