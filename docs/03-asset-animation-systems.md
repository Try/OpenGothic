# OpenGothic Engine - Asset Pipeline and Animation Systems

## Table of Contents
1. [Asset Pipeline](#asset-pipeline)
2. [Animation System](#animation-system)

---

# Asset Pipeline

## 1.1 Overview

OpenGothic's asset pipeline is built around **ZenKit**, a modern C++ library for parsing Gothic's proprietary formats. The system provides:
- VFS (Virtual File System) for archive mounting
- Multi-format support (meshes, textures, audio, scripts)
- Caching system with per-type storage
- GPU resource lifecycle management
- Mod overlay system with priority

**Central Manager**: `Resources` class (`game/resources.h/cpp`)

## 1.2 Supported Asset Types

### Meshes

| Format | Extension | Description | Use Case |
|--------|-----------|-------------|----------|
| Multi-Resolution Mesh | `.MRM` | Static meshes with LODs | World objects, items |
| Morph Mesh | `.MMB` | Blend shape animation | Facial expressions |
| Model Mesh | `.MDM` | Skeletal mesh geometry | Characters, creatures |
| Model Hierarchy | `.MDH` | Bone structure | Character skeletons |
| Model (combined) | `.MDL` | Mesh + Hierarchy | Characters (Gothic 1) |
| ASCII Model | `.ASC` | Text-based mesh | Dev tools, exports |
| World Mesh | `.ZEN` (embedded) | Terrain and buildings | Level geometry |

**Optimization**: All meshes processed into `PackedMesh` format with:
- Meshlets (64 vertices, 64 primitives)
- BVH acceleration structure
- Material bucket assignment

### Textures

| Format | Extension | Description | Compression |
|--------|-----------|-------------|-------------|
| Targa | `.TGA` | Uncompressed RGBA | None |
| Gothic Texture | `.TEX` | Compressed Gothic format | DXT1-5 |
| Derived | None | Procedural from color | None |

**Features**:
- Animated textures: `texture_A0.tga`, `texture_A1.tga`, ...
- UV scrolling for waterfalls, etc.
- Automatic mipmap generation
- DXT → DDS conversion via ZenKit

### Audio

| Type | Extensions | Description |
|------|------------|-------------|
| Sound Effects | `.WAV` | PCM audio samples |
| DirectMusic Segments | `.SGT`, `.STY` | Dynamic music segments |
| DLS Collections | `.DLS` | Instrument samples |

### Scripts

| Type | Extensions | Description | Priority |
|------|------------|-------------|----------|
| Daedalus Compiled | `.DAT` | Bytecode | Fallback |
| Daedalus Binary | `.BIN` | Binary bytecode | Primary |
| Animation Scripts | `.MDS` | ASCII animation definitions | G1 |
| Animation Binary | `.MSB` | Binary animation definitions | G2 (preferred) |

**Load Order**: `.BIN` → `.DAT` (`.BIN` takes precedence)

### World Data

| Format | Extension | Contents |
|--------|-----------|----------|
| ZEN World | `.ZEN` | Complete level: BSP, VOBs, waypoints, mesh |

**Components Loaded**:
- BSP tree (spatial partitioning)
- VOB tree (game objects)
- Way network (pathfinding)
- World mesh (terrain)
- World bounding box

### Fonts

| Type | Files | Description |
|------|-------|-------------|
| Font Definition | `.FNT` | Character metrics, kerning |
| Font Texture | `.TGA` | Glyph atlas |

**Variants**: Normal, Hi-res, Disabled, Yellow, Red

## 1.3 VDF Archive System

### Virtual File System (VFS)

**Library**: ZenKit's `zenkit::Vfs`

**Architecture**:
```
┌────────────────────────────────────────┐
│  Application Layer                     │
│  Resources::loadTexture()              │
└────────────────────────────────────────┘
                ↓
┌────────────────────────────────────────┐
│  VFS Layer (zenkit::Vfs)               │
│  ├─ _work/ (loose files)               │
│  ├─ Textures.mod (mod archive)         │
│  ├─ Addon.vdf (expansion)              │
│  └─ Data.vdf (base game)               │
└────────────────────────────────────────┘
                ↓
┌────────────────────────────────────────┐
│  Physical Storage                      │
│  ├─ C:/Gothic2/Data/*.vdf              │
│  └─ C:/Gothic2/_work/**/* (loose)      │
└────────────────────────────────────────┘
```

### Archive Mounting

**File**: `game/resources.cpp:129-173`

```cpp
void Resources::loadVdfs(const std::vector<std::u16string>& modvdfs,
                         bool modFilter) {
  std::vector<Archive> archives;

  // 1. Scan for VDF files
  detectVdf(archives, Gothic::inst().path({u"Data"}, Dir::FT_Dir));

  // 2. Sort by priority: mods first, then timestamp
  std::stable_sort(archives.begin(), archives.end(),
    [](const Archive& a, const Archive& b) {
      return std::make_tuple(a.isMod ? 1 : -1, a.time, int(a.ord)) >=
             std::make_tuple(b.isMod ? 1 : -1, b.time, int(b.ord));
    });

  // 3. Mount each archive
  for(auto& archive : archives) {
    gothicAssets.mount_disk(archive.name,
                           zenkit::VfsOverwriteBehavior::OLDER);
  }
}
```

**Archive Metadata**:
```cpp
struct Archive {
  std::u16string name;     // File path
  int64_t        time;     // VDF timestamp (internal, not file system)
  uint16_t       ord;      // Load order
  bool           isMod;    // .MOD file?
};
```

**Priority System**:
1. `.MOD` files (mods) override all
2. Within mods: newer timestamp overrides older
3. Base `.VDF` files loaded last

### Work Directory Override

**Purpose**: Allow loose files to override VDF contents

**File**: `game/resources.cpp:124-127`

```cpp
void Resources::mountWork(const std::filesystem::path& path) {
  inst->gothicAssets.mkdir("/_work");
  inst->gothicAssets.mount_host(path, "/_work",
                               zenkit::VfsOverwriteBehavior::ALL);
}
```

**Use Cases**:
- Mod development (test without repackaging)
- Texture replacements
- Script debugging

**Load Priority**:
```
Highest: _work/ loose files
         .BIN scripts
         .DAT scripts
         .MOD archives
Lowest:  Base .VDF archives
```

## 1.4 Asset Loading Flow

### Generic Loading Pattern

```cpp
template<typename T>
T* Resources::load(std::string_view name) {
  // 1. CHECK CACHE
  auto it = cache.find(name);
  if(it != cache.end())
    return it->second.get();

  // 2. QUERY VFS
  const auto* entry = vdfsIndex().find(name);
  if(!entry)
    return nullptr;  // Not found

  // 3. PARSE WITH ZENKIT
  auto reader = entry->open_read();
  zenkit::AssetType asset;
  asset.load(reader.get());

  // 4. CONVERT TO ENGINE FORMAT
  auto engineAsset = std::make_unique<T>(asset);

  // 5. UPLOAD TO GPU (if needed)
  engineAsset->createGpuResources(device);

  // 6. CACHE
  T* ptr = engineAsset.get();
  cache[name] = std::move(engineAsset);

  return ptr;
}
```

### Texture Loading Example

**File**: `game/resources.cpp:341-424`

**Steps**:
1. **Normalize name**: Convert to uppercase, handle `-C.TEX` compressed variant
2. **Check cache**: `texCache.find(name)`
3. **Attempt compressed**: Try `.TEX` format first (smaller, faster)
4. **Fallback uncompressed**: Load `.TGA` if `.TEX` not found
5. **ZenKit parse**: `zenkit::Texture::load()` or TGA decode
6. **DXT conversion**: `zenkit::to_dds()` for compressed textures
7. **Mipmap generation**: If `forceMips` and no mipmaps exist
8. **GPU upload**: `Tempest::Texture2d` creation
9. **Cache store**: `texCache[name] = std::move(texture)`

**Code Snippet**:
```cpp
Tempest::Texture2d* Resources::implLoadTexture(std::string_view name,
                                                bool forceMips) {
  // Check cache
  auto it = texCache.find(name);
  if(it != texCache.end())
    return it->second.get();

  // Load uncached
  auto tex = implLoadTextureUncached(name, forceMips);
  if(!tex)
    return nullptr;

  // Store in cache
  auto ptr = tex.get();
  texCache[name] = std::move(tex);
  return ptr;
}
```

### Mesh Loading Example

**File**: `game/resources.cpp:444-460`

**Static Mesh (.MRM)**:
```cpp
ProtoMesh* Resources::loadMesh(std::string_view name) {
  // 1. Convert extension: .3DS → .MRM
  FileExt::exchangeExt(name, "3DS", "MRM");

  // 2. Find in VFS
  const auto* entry = vdfsIndex().find(name);
  if(!entry) return fallbackMesh();

  // 3. Parse with ZenKit
  zenkit::MultiResolutionMesh zmsh;
  auto reader = entry->open_read();
  zmsh.load(reader.get());

  // 4. Optimize for rendering
  PackedMesh packed(zmsh, PackedMesh::PK_Visual);

  // 5. Create ProtoMesh (submeshes, materials, attachments)
  return new ProtoMesh(std::move(packed), name);
}
```

**Skeletal Mesh (.MDL/.MDH/.MDM)**:
```cpp
// Gothic 2: separate files
loadMesh("HUMANS.MDH");  // Hierarchy (skeleton)
loadMesh("HUM_BODY_NAKED0.MDM");  // Mesh geometry

// Gothic 1: combined
loadMesh("HUMAN.MDL");  // Both hierarchy and mesh
```

## 1.5 Asset Caching

### Cache Structure

**File**: `game/resources.h:248-259`

```cpp
class Resources {
  // Textures
  TextureCache texCache;  // unordered_map<string, unique_ptr<Texture2d>>
  std::map<Color, unique_ptr<Texture2d>, Less> pixCache;  // Solid colors

  // Meshes
  std::unordered_map<std::string, std::unique_ptr<ProtoMesh>> aniMeshCache;
  std::unordered_map<DecalK, std::unique_ptr<ProtoMesh>, Hash> decalMeshCache;
  std::unordered_map<std::string, std::unique_ptr<Skeleton>> skeletonCache;
  std::unordered_map<std::string, std::unique_ptr<Animation>> animCache;
  std::unordered_map<BindK, std::unique_ptr<AttachBinder>, Hash> bindCache;
  std::unordered_map<std::string, std::unique_ptr<PfxEmitterMesh>> emiMeshCache;

  // World data
  std::unordered_map<std::string, std::unique_ptr<VobTree>> zenCache;

  // Fonts
  std::unordered_map<FontK, std::unique_ptr<GthFont>, Hash> gothicFnt;

  // Thread safety
  std::recursive_mutex sync;
};
```

**Cache Characteristics**:
- ✅ Thread-safe via recursive mutex
- ✅ Stores `unique_ptr` for automatic cleanup
- ✅ Case-insensitive keys (normalized to uppercase)
- ❌ Never evicts entries (grows indefinitely)
- ❌ No LRU or reference counting

### GPU Resource Recycling

**Problem**: Can't delete GPU resources immediately (may be in-flight)

**Solution**: Deferred deletion with frame delay

**File**: `game/resources.h:238-246`

```cpp
struct DeleteQueue {
  std::vector<Tempest::StorageBuffer>        ssbo;
  std::vector<Tempest::StorageImage>         img;
  std::vector<Tempest::ZBuffer>              zb;
  std::vector<Tempest::DescriptorArray>      arr;
  std::vector<Tempest::AccelerationStructure> rtas;
};

static constexpr uint8_t MaxFramesInFlight = 2;
DeleteQueue recycled[MaxFramesInFlight];
uint8_t recycledId = 0;
```

**Lifecycle**:
```cpp
// Frame N: User wants to delete resource
Resources::recycle(std::move(resource));  // Add to current frame's queue

// Frame N+1: Resource still potentially in-flight on GPU
// (Queue not touched)

// Frame N+2: Safe to delete
Resources::resetRecycled((N+2) % MaxFramesInFlight);  // Clear old queue
// vector destructor deletes resources
```

**Code**:
```cpp
void Resources::recycle(StorageBuffer&& buf) {
  std::lock_guard<std::mutex> guard(syncRecycle);
  recycled[recycledId].ssbo.emplace_back(std::move(buf));
}

void Resources::resetRecycled(uint8_t fId) {
  std::lock_guard<std::mutex> guard(syncRecycle);
  auto& r = recycled[fId];
  r.ssbo.clear();
  r.img.clear();
  r.zb.clear();
  r.arr.clear();
  r.rtas.clear();
}
```

## 1.6 Mesh Optimization: PackedMesh

### Meshlet System

**Goal**: Optimize for mesh shaders and GPU culling

**File**: `game/graphics/mesh/submesh/packedmesh.h:20-25`

```cpp
enum {
  MaxVert     = 64,   // Vertices per meshlet
  MaxPrim     = 64,   // Triangles per meshlet
  MaxInd      = 192,  // Indices (MaxPrim * 3)
  MaxMeshlets = 16,   // Meshlets per submesh
};
```

### Packed Data Format

**Per-Meshlet Storage**:
```cpp
struct Meshlet {
  // Vertices (MaxVert)
  struct Vertex {
    uint8_t pos[3];       // Position (quantized to bbox)
    uint8_t normal[3];    // Normal (8-bit octahedral)
    uint8_t uv[2];        // UV (normalized to 0-255)
    uint32_t color;       // Vertex color
    // For skeletal: bone IDs + weights
  } vertices[MaxVert];

  // Indices (MaxInd)
  uint8_t indices[MaxInd];  // Local indices (0-63)

  // Metadata (stored in last primitive)
  uint8_t vertexCount;      // Actual vertex count
  uint8_t primitiveCount;   // Actual primitive count
};
```

### Packing Process

**File**: `game/graphics/mesh/submesh/packedmesh.cpp`

**Algorithm**:
```cpp
void PackedMesh::packMeshletsObj(const zenkit::Mesh& mesh) {
  for(auto& submesh : mesh.submeshes) {
    // 1. Split into 64-vertex clusters
    std::vector<Cluster> clusters = clusterize(submesh, MaxVert);

    // 2. For each cluster
    for(auto& cluster : clusters) {
      Meshlet meshlet;

      // 3. Quantize vertices to bounding box
      AABB bbox = computeBounds(cluster.vertices);
      for(auto& v : cluster.vertices) {
        meshlet.vertices[i].pos = quantize(v.position, bbox, 8);  // 8-bit
        meshlet.vertices[i].normal = octEncode(v.normal);         // 8-bit
        meshlet.vertices[i].uv = quantizeUV(v.uv);                // 8-bit
      }

      // 4. Generate local indices
      for(auto& tri : cluster.triangles) {
        meshlet.indices[j++] = tri.i0;  // 0-63
        meshlet.indices[j++] = tri.i1;
        meshlet.indices[j++] = tri.i2;
      }

      // 5. Store metadata in last primitive
      meshlet.indices[MaxInd-2] = cluster.vertexCount;
      meshlet.indices[MaxInd-1] = cluster.triangleCount;

      submesh.meshlets.push_back(meshlet);
    }
  }
}
```

**Benefits**:
- **Memory**: 64 vertices × 12 bytes = 768 bytes/meshlet (compact)
- **Cache-Friendly**: Small working set fits in GPU cache
- **Culling Granularity**: Per-meshlet instead of per-object
- **Mesh Shader Ready**: Direct input to mesh shader pipeline

### BVH for Ray Tracing

**Purpose**: Acceleration structure for software/hardware RT

**File**: `game/graphics/mesh/submesh/packedmesh.cpp:packBVH()`

**Algorithm**:
```cpp
void PackedMesh::packBVH(const zenkit::Mesh& mesh) {
  // 1. Build list of triangle AABBs
  std::vector<AABB> triBoxes;
  for(auto& submesh : mesh.submeshes) {
    for(auto& tri : submesh.triangles) {
      AABB box;
      box.expand(tri.v0);
      box.expand(tri.v1);
      box.expand(tri.v2);
      triBoxes.push_back(box);
    }
  }

  // 2. Build BVH (binary tree)
  bvh = buildBVH(triBoxes, 0, triBoxes.size());
  //   - Recursively split on longest axis
  //   - Leaf nodes store triangle indices
  //   - Internal nodes store bounding box
}
```

**BVH Format**:
```cpp
struct BvhNode {
  AABB   bbox;
  uint32_t left;   // Left child index (or first triangle)
  uint32_t right;  // Right child index (or triangle count)
  bool   isLeaf;
};
```

**Usage**:
- Ray tracing in compute shaders (software RT)
- Acceleration structure building for hardware RT
- Collision detection (optional)

## 1.7 Material System

### Material Class

**File**: `game/graphics/material.h:10-61`

```cpp
class Material {
public:
  enum AlphaFunc {
    Solid,          // Opaque
    AlphaTest,      // Cutout (alpha < 0.5 discarded)
    Water,          // Tessellated water surface
    Ghost,          // Special transparency
    Multiply,       // Multiplicative blend
    Transparent,    // Alpha blend
    AdditiveLight,  // Additive blend
  };

private:
  const Tempest::Texture2d* tex = nullptr;
  std::vector<const Tempest::Texture2d*> frames;  // Animated textures

  AlphaFunc alpha = AlphaTest;
  float     alphaWeight = 1.0f;

  // Animation
  Tempest::Point texAniMapDirPeriod;  // UV scroll direction and period
  uint64_t       texAniFPSInv = 1;    // Frame animation rate

  // Special properties
  bool  isGhost = false;
  float waveMaxAmplitude = 0.0f;  // Water waves
  float envMapping = 0.0f;        // Environment map strength
};
```

### Material Loading

**File**: `game/graphics/material.cpp:17-104`

**From ZenKit Material**:
```cpp
Material::Material(const zenkit::Material& mat, bool vert) {
  // 1. Load texture
  tex = Resources::loadTexture(mat.texture);

  // 2. Alpha function
  switch(mat.alpha_func) {
    case zenkit::AlphaFunction::BLEND:
      alpha = Transparent;
      break;
    case zenkit::AlphaFunction::ADD:
      alpha = AdditiveLight;
      break;
    case zenkit::AlphaFunction::MULTIPLY:
      alpha = Multiply;
      break;
    default:
      alpha = (mat.group == zenkit::MaterialGroup::WATER) ? Water : Solid;
  }

  // 3. Texture animation
  if(mat.texture_anim_fps > 0) {
    texAniFPSInv = uint64_t(1000.0 / mat.texture_anim_fps);
    // Load animation frames: texture_A0, texture_A1, ...
    for(int i = 0; i < mat.texture_anim_mapping_count; ++i) {
      frames.push_back(Resources::loadTexture(animName));
    }
  }

  // 4. UV animation
  texAniMapDirPeriod.x = int32_t(mat.texture_anim_mapping_dir.x * 100);
  texAniMapDirPeriod.y = int32_t(mat.texture_anim_mapping_dir.y * 100);

  // 5. Water waves
  if(mat.wave_mode != zenkit::WaveMode::NONE) {
    waveMaxAmplitude = mat.wave_max_amplitude / 100.0f;  // cm to meters
  }

  // 6. Environment mapping
  envMapping = mat.environment_mapping;
}
```

### Rendering Path Selection

**File**: `game/graphics/material.cpp:220-236`

```cpp
bool Material::isForwardShading() const {
  switch(alpha) {
    case Water:
    case Ghost:
    case Transparent:
    case AdditiveLight:
    case Multiply:
      return true;  // Forward pass
    default:
      return false; // Deferred pass
  }
}

bool Material::isTesselated() const {
  return alpha == Water && waveMaxAmplitude > 0;
}

bool Material::isShadowmapRequired() const {
  return alpha != Ghost && alpha != AdditiveLight;
}
```

---

# Animation System

## 2.1 Skeletal Animation Architecture

### Skeleton

**File**: `game/graphics/mesh/skeleton.h`

**Structure**:
```cpp
class Skeleton {
public:
  static constexpr size_t MAX_NUM_SKELETAL_NODES = 96;

  struct Node {
    size_t             parent = size_t(-1);
    Tempest::Matrix4x4 tr;       // Local transform
    std::string        name;
  };

  std::vector<Node>               nodes;
  std::vector<Tempest::Matrix4x4> tr;  // World-space transforms (computed)

  // Special node indices
  size_t headNode  = size_t(-1);  // BIP01_HEAD
  size_t rootNode  = 0;           // BIP01

  // Bounding box (for physics)
  AABB bbox;
};
```

**Loading** (`game/graphics/mesh/skeleton.cpp:20-95`):
```cpp
Skeleton::Skeleton(const zenkit::ModelHierarchy& h) {
  nodes.resize(h.nodes.size());

  for(size_t i = 0; i < h.nodes.size(); ++i) {
    auto& src = h.nodes[i];
    auto& dst = nodes[i];

    dst.parent = src.parent_index;
    dst.tr     = toMatrix4x4(src.transform);
    dst.name   = src.name;

    // Find special nodes
    if(src.name == "BIP01 HEAD")
      headNode = i;
  }

  // Compute hierarchy order for efficient traversal
  ordered.resize(nodes.size());
  // ... topological sort ...
}
```

### Animation Data

**File**: `game/graphics/mesh/animation.h`

**Structure**:
```cpp
struct AnimData {
  // Keyframe data
  std::vector<zenkit::AnimationSample> samples;
  std::vector<uint32_t>                nodeIndex;  // Which bone

  // Frame range
  uint32_t firstFrame = 0;
  uint32_t lastFrame = 0;
  uint32_t numFrames = 0;

  float fpsRate = 60.0f;  // Frames per second

  // Movement delta (for locomotion)
  std::vector<Tempest::Vec3> tr;

  // Events (combat, items, effects)
  std::vector<zenkit::MdsSoundEffectGround> gfx;  // Footsteps
  std::vector<zenkit::MdsSoundEffect>       sfx;  // Sound effects
  std::vector<zenkit::MdsParticleEffect>    pfx;  // Particle effects
  std::vector<zenkit::MdsEventTag>          events;  // Gameplay events
};
```

**Sample Format**:
```cpp
struct AnimationSample {
  zenkit::Quat rotation;  // Quaternion (x, y, z, w)
  zenkit::Vec3 position;  // Translation (x, y, z)
};
```

## 2.2 Animation Blending

### Pose Class

**File**: `game/graphics/mesh/pose.h`

**Layer System**:
```cpp
class Pose {
  struct Layer {
    const Animation* seq = nullptr;
    uint64_t         sTime = 0;      // Start time
    BodyState        bs = BS_NONE;   // Body state
    uint8_t          comb = 0;       // Combo index
  };

  std::vector<Layer> lay;  // Multiple simultaneous animations

  // Sampling state
  enum SampleState {
    S_None,
    S_Old,    // Previous frame
    S_Valid,  // Current frame
  };
  SampleState                 state = S_None;
  std::vector<zenkit::AnimationSample> tr;      // Current transforms
  std::vector<zenkit::AnimationSample> trOld;   // Previous transforms
};
```

### Blending Algorithm

**File**: `game/graphics/mesh/pose.cpp:460-526`

**Update Flow**:
```cpp
void Pose::update(uint64_t now, Npc* npc) {
  // 1. Rotate sample states: Valid → Old → None
  state = (state == S_Valid) ? S_Old : S_None;
  std::swap(tr, trOld);

  // 2. Sample all layers
  for(auto& layer : lay) {
    uint64_t t = now - layer.sTime;  // Time in animation
    float alpha = layer.seq->computeAlpha(t);

    // Sample animation at time t
    layer.seq->sample(tr, t, alpha);
  }

  // 3. Blend between old and new samples
  if(state == S_Old) {
    float blendAlpha = computeBlendAlpha();
    for(size_t i = 0; i < tr.size(); ++i) {
      tr[i] = AnimMath::mix(trOld[i], tr[i], blendAlpha);
    }
  }

  state = S_Valid;

  // 4. Compute world-space transforms
  skeletonToWorldSpace();
}
```

**Interpolation** (`game/graphics/mesh/animmath.cpp:33-47`):
```glsl
// Quaternion SLERP
Quat slerp(const Quat& x, const Quat& y, float a) {
  float cosTheta = dot(x, y);

  // Shortest path
  Quat y2 = (cosTheta < 0) ? -y : y;
  cosTheta = abs(cosTheta);

  // Lerp for close quaternions
  if(cosTheta > 0.9995f)
    return normalize(lerp(x, y2, a));

  // True slerp
  float theta = acos(cosTheta);
  return (sin((1-a)*theta)*x + sin(a*theta)*y2) / sin(theta);
}

// Mix samples
AnimationSample mix(const AnimationSample& x,
                   const AnimationSample& y,
                   float a) {
  return {
    slerp(x.rotation, y.rotation, a),
    lerp(x.position, y.position, a)
  };
}
```

## 2.3 Animation Events

### Event System

**File**: `game/graphics/mesh/animation.h:55-104`

**Event Types**:
```cpp
enum EvType {
  // Combat
  EV_Undefined,
  EV_Create_Item,
  EV_Insert_Item,
  EV_Remove_Item,
  EV_Destroy_Item,
  EV_Place_Munition,
  EV_Remove_Munition,
  EV_Draw_Sound,
  EV_Undraw_Sound,
  EV_Swap_Mesh,
  EV_Draw_Torch,
  EV_Inv_Torch,
  EV_Drop_Torch,
  EV_Hit_Limb,
  EV_Hit_Direction,
  EV_Dam_Multiply,
  EV_Par_Frame,
  EV_Opt_Frame,
  EV_Hit_End,
  EV_Window,
  EV_Combo,
  EV_SetFightMode,
  EV_CreateQuadMark,
};
```

**Event Storage**:
```cpp
struct EvCount {
  uint8_t def_opt_frame = 0;       // Hit frame number
  uint8_t groundSounds = 0;        // Number of footsteps
  zenkit::MdsFightMode weaponCh = INVALID;  // Weapon change

  std::vector<EvTimed> timed;      // Frame-based events
  std::vector<EvMorph> morph;      // Facial morphs
};
```

### Event Processing

**File**: `game/graphics/mesh/pose.cpp:528-624`

**Barrier System**:
```cpp
void Pose::processEvents(uint64_t barrier, Animation::EvCount& ev) {
  for(auto& layer : lay) {
    uint64_t t = now - layer.sTime;
    uint32_t frame = layer.seq->frameAt(t);

    // Check each event
    for(auto& event : layer.seq->events) {
      if(event.frame == frame && event.frame > barrier) {
        // Trigger event
        ev.timed.push_back(event);
      }
    }
  }
}
```

**NPC Integration** (`game/world/objects/npc.cpp:1814-1892`):
```cpp
void Npc::tickAnimationTags(uint64_t& barrier) {
  Animation::EvCount ev = visual.processEvents(barrier);

  for(auto& event : ev.timed) {
    switch(event.type) {
      case Animation::EV_Create_Item:
        createItemSlot(event.slot, event.item);
        break;
      case Animation::EV_Swap_Mesh:
        visual.setBody(event.slot, event.item);
        break;
      case Animation::EV_Opt_Frame:
        hitFrameReached = true;  // Attack hit detection
        break;
      // ... handle all event types
    }
  }

  barrier = currentFrame;  // Update barrier
}
```

## 2.4 Animation State Machines

### AnimationSolver

**File**: `game/graphics/mesh/animationsolver.h`

**Purpose**: Select appropriate animation based on:
- Current weapon
- Body state (idle, walk, run, attack, etc.)
- Transition type
- Walking mode (sneak, normal, run)

**Pattern-Based Resolution**:
```cpp
Anim AnimationSolver::solveAnim(WeaponState ws,
                                WeaponState wsNext,
                                BodyState bs) {
  // Build animation name from pattern
  // Example: "1H_SWORD_RUN" = "1H_" + weaponName(ws) + "_" + stateName(bs)

  std::string animName;

  if(bs == BS_RUN)
    animName = format("{}RUN", weaponPrefix(ws));
  else if(bs == BS_ATTACK)
    animName = format("{}ATTACK", weaponPrefix(ws));
  // ... many more patterns ...

  // Lookup animation
  return findAnim(animName);
}
```

**Overlay System**:
```cpp
struct Overlay {
  std::string                   name;
  std::shared_ptr<Skeleton>     skeleton;
  std::vector<Animation>        anims;
  uint64_t                      time = 0;
};

std::vector<Overlay> overlay;  // Armor, helmet overlays
```

### Transition System

**Automatic Transition Detection**:
```cpp
// Example: "T_1HRUN_2_STAND" → transition from run to stand
if(animName.starts_with("T_")) {
  // Extract source and dest states
  auto src = extractState(animName, 1);
  auto dst = extractState(animName, 2);

  // Create transition
  return Transition {
    .from = src,
    .to = dst,
    .anim = findAnim(animName)
  };
}
```

**Combo System**:
```cpp
struct Combo {
  uint64_t window = 0;  // Time window to continue combo
  uint8_t  current = 0; // Current combo index
  uint8_t  max = 0;     // Max combo length
};

// Extend combo if within window
if(now - lastAttack < combo.window && combo.current < combo.max) {
  combo.current++;
  return findAnim(format("{}ATTACK_{}", weaponPrefix, combo.current));
}
```

## 2.5 GPU Skinning

### Vertex Format

**File**: `game/resources.h:63-70`

```cpp
struct VertexA {  // Animated vertex
  float    norm[3];      // Normal (3 floats)
  float    uv[2];        // UV (2 floats)
  uint32_t color;        // Color (packed RGBA)

  // Skinning data (4 bone influences)
  float    pos[4][3];    // 4 bone-local positions
  uint8_t  boneId[4];    // 4 bone indices
  float    weights[4];   // 4 weights (sum = 1.0)
};
```

**Pre-transformed Positions**:
- Instead of storing inverse bind matrices
- Each vertex stores 4 pre-transformed positions (already in bone space)
- Shader only needs to transform and blend

### Shader Implementation

**File**: `shader/materials/vertex_process.glsl:124-136`

```glsl
// Pull bone matrices from instance buffer
const uvec4 boneId = v.boneId + uvec4(obj.animPtr);
const vec3 t0 = (pullMatrix(boneId.x) * vec4(v.pos0, 1.0)).xyz;
const vec3 t1 = (pullMatrix(boneId.y) * vec4(v.pos1, 1.0)).xyz;
const vec3 t2 = (pullMatrix(boneId.z) * vec4(v.pos2, 1.0)).xyz;
const vec3 t3 = (pullMatrix(boneId.w) * vec4(v.pos3, 1.0)).xyz;

// Blend positions
pos = t0 * v.weight.x +
      t1 * v.weight.y +
      t2 * v.weight.z +
      t3 * v.weight.w;

// Similar for normals
```

**Matrix Upload** (`game/graphics/sceneglobals.cpp`):
```cpp
void SceneGlobals::updateAnimation(const Pose& pose) {
  // Get world-space bone matrices
  auto matrices = pose.transform();

  // Upload to GPU buffer
  for(size_t i = 0; i < matrices.size(); ++i) {
    instanceMem[animPtr + i] = matrices[i];
  }
}
```

## 2.6 Animation Compression

### Current Status: ❌ No Compression

**Observations**:
- Animation data stored uncompressed in memory
- Full 32-bit float precision for all transforms
- No quantization of quaternions or positions
- No keyframe reduction or curve fitting
- No delta encoding between frames

**Why?**:
- Original Gothic animations are already efficient
- Memory not a bottleneck for this game
- Simplicity over optimization
- Lazy loading via VFS reduces memory usage anyway

**Potential Optimizations** (not implemented):
- Quaternion quantization to 16-bit per component
- Position quantization relative to bone rest pose
- Keyframe reduction via curve fitting
- Delta encoding for minimal changes
- Compression per-track (some bones change less)

## 2.7 Root Motion

### Extraction

**File**: `game/graphics/mesh/pose.cpp:232-266`

```cpp
Matrix4x4 Pose::rootTransform() const {
  if(tr.empty())
    return Matrix4x4::identity();

  // Get root bone transform (usually BIP01)
  auto& sample = tr[0];

  Matrix4x4 m;
  m.translate(sample.position);
  m.rotate(sample.rotation);

  return m;
}

Vec3 Pose::rootTranslation() const {
  auto& anim = lay[0].seq;
  uint64_t t = now - lay[0].sTime;

  float frame = t * anim->fpsRate / 1000.0f;
  int f0 = int(frame);
  int f1 = f0 + 1;

  // Interpolate movement delta
  Vec3 d0 = anim->tr[f0];
  Vec3 d1 = anim->tr[f1];
  float alpha = frame - f0;

  return lerp(d0, d1, alpha);
}
```

**NPC Integration** (`game/world/objects/npc.cpp:1203-1245`):
```cpp
void Npc::updatePos() {
  // Get root motion from animation
  Vec3 dpos = visual.pose.rootTranslation();

  // Apply to NPC position
  position += dpos.rotate(rotation);

  // Sync with physics
  physic.setPosition(position);
}
```

---

## Key Takeaways

### Asset Pipeline
- ✅ **Robust**: ZenKit handles all Gothic format complexity
- ✅ **Flexible**: VFS overlay system for mods
- ✅ **Cached**: Per-type caches with thread safety
- ✅ **Optimized**: Meshlets and BVH for modern GPUs
- ⚠️ **No Hot Reload**: Changes require restart
- ⚠️ **No Eviction**: Caches grow indefinitely

### Animation System
- ✅ **Sophisticated**: Multi-layer blending with SLERP
- ✅ **Event-Rich**: Extensive gameplay integration
- ✅ **GPU-Accelerated**: Fast skinning with 4 bones/vertex
- ✅ **State Machine**: Pattern-based animation resolution
- ✅ **Root Motion**: Proper locomotion extraction
- ❌ **No Compression**: Uncompressed storage

---

**Next**: See `04-audio-input-scripting.md` for analysis of audio, input, and Daedalus VM integration.
