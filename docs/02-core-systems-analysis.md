# OpenGothic Engine - Core Systems Analysis

## Table of Contents
1. [Rendering System](#rendering-system)
2. [Scene Management](#scene-management)
3. [Physics System](#physics-system)
4. [Game Loop Architecture](#game-loop-architecture)

---

# Rendering System

## 1.1 Graphics API and Abstraction

### Tempest Engine Integration

OpenGothic uses **Tempest**, a custom graphics abstraction layer that provides unified access to:
- **Vulkan** (Linux, Windows, Android)
- **DirectX 12** (Windows via `-dx12` flag)
- **Metal** (macOS, iOS)

**Key Abstraction Classes** (`lib/Tempest/Engine/include`):
```cpp
Device                  // Graphics device
CommandBuffer          // Command recording
RenderPipeline         // Graphics pipeline state
ComputePipeline        // Compute shader pipeline
Texture2d              // 2D textures
StorageBuffer          // SSBO/UAV
AccelerationStructure  // Ray tracing BLAS/TLAS
```

**Backend Selection** (`game/main.cpp:130-136`):
```cpp
#ifdef __WINDOWS__
  if(hasArg(argc, argv, "-dx12"))
    api.reset(new Tempest::DirectX12Api());
  else
#endif
  api.reset(new Tempest::VulkanApi());
```

**Device Initialization**:
- Prefers discrete GPUs over integrated
- Creates logical device with required extensions
- Manages swap chains per window

### Resource Management

**Double Buffering** (`game/resources.h:237-246`):
```cpp
static constexpr const uint8_t MaxFramesInFlight = 2;

struct DeleteQueue {
  std::vector<StorageBuffer>        ssbo;
  std::vector<StorageImage>         img;
  std::vector<ZBuffer>              zb;
  std::vector<DescriptorArray>      arr;
  std::vector<AccelerationStructure> rtas;
};
DeleteQueue recycled[MaxFramesInFlight];
```

**Lifecycle**:
1. Frame N: Resource used in GPU commands
2. Frame N+1: Resource still potentially in-flight
3. Frame N+2: Safe to delete, added to `recycled[(N+2)%2]`

## 1.2 Rendering Pipeline Architecture

### Hybrid Deferred/Forward Rendering

**Pipeline Type**: Deferred for opaque, forward for special materials

#### G-Buffer Structure
```cpp
// game/graphics/worldview.cpp
struct GBuffer {
  Texture2d diffuse;   // RGBA8 (RGB=albedo, A=hint)
  Texture2d normal;    // R32U (packed 2x16-bit normals)
  ZBuffer   depth;     // Depth16/24/32F
};
```

#### Full Pipeline Flow

```
┌─────────────────────────────────────────┐
│  1. VISIBILITY & CULLING (Compute)      │
│  ├─ HiZ Occlusion Culling               │
│  ├─ Cluster/Meshlet Frustum Culling     │
│  └─ Generate Indirect Draw Commands     │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│  2. SHADOW PASS                          │
│  ├─ Traditional Shadow Maps (2048²)     │
│  ├─ Virtual Shadow Maps (if enabled)    │
│  └─ Ray-Traced Shadows (experimental)   │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│  3. G-BUFFER PASS (Deferred)            │
│  ├─ Opaque Geometry                     │
│  ├─ Alpha-Tested Materials              │
│  └─ Output: Albedo, Normal, Depth       │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│  4. LIGHTING PASS (Deferred)            │
│  ├─ Directional Light (Sun/Moon)        │
│  ├─ Shadow Resolution                   │
│  ├─ Probe-based GI (if RT enabled)      │
│  └─ Point/Spot Lights                   │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│  5. FORWARD PASS                         │
│  ├─ Water (with tessellation)           │
│  ├─ Transparent Materials               │
│  ├─ Particles                           │
│  └─ Emissive Materials                  │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│  6. POST-PROCESSING                      │
│  ├─ Sky Rendering (physical atmosphere) │
│  ├─ Volumetric Fog (epipolar/3D)       │
│  ├─ Screen-Space Reflections (SSR)     │
│  ├─ SSAO (optional)                     │
│  ├─ Anti-Aliasing (CMAA2)              │
│  ├─ Tonemapping                         │
│  └─ UI Composition                      │
└─────────────────────────────────────────┘
```

### Material Classification

**File**: `game/graphics/material.cpp:220-236`

```cpp
bool Material::isForwardShading() const {
  switch(alpha) {
    case Water:         return true;  // Needs tessellation
    case Ghost:         return true;  // Special alpha
    case Transparent:   return true;
    case AdditiveLight: return true;
    case Multiply:      return true;
    default:            return false; // Deferred
  }
}
```

## 1.3 Shader Management

### Shader Organization

**Directory**: `/shader/`
```
shader/
├── antialiasing/    - CMAA2 implementation
├── epipolar/        - Volumetric fog along sun rays
├── hiz/             - Hierarchical Z-buffer
├── inventory/       - Item rendering
├── lighting/        - Deferred lighting, shadows, GI
├── materials/       - Vertex/fragment shaders per material
├── rtsm/            - Ray-traced shadow maps (software)
├── sky/             - Atmosphere and clouds
├── ssao/            - Screen-space ambient occlusion
├── swrt/            - Software ray tracing
├── upscale/         - Resolution upscaling
├── virtual_shadow/  - Virtual shadow maps
└── water/           - Water tessellation
```

### Compilation System

**File**: `game/graphics/shaders.cpp:1124-1204`

**Async Compilation**:
```cpp
void Shaders::compileShaders() {
  std::thread th([this]() {
    // Compile all shader permutations in background
    for(auto& shader : allShaders) {
      shader.compile(device);
    }
  });
  th.detach();
}
```

**Lazy Pipeline Creation**:
- Shaders compiled at startup
- Pipelines created on first use
- Material pipelines cached by: `(AlphaFunc, Scene, Variant)`

### Shader Features

**Bindless Textures** (if supported):
```glsl
// shader/materials/common.glsl
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 3) uniform sampler2D textures[];

vec4 sampleTexture(uint texId, vec2 uv) {
  return texture(textures[nonuniformEXT(texId)], uv);
}
```

**Mesh Shaders** (`shader/materials/main.mesh`):
```glsl
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 64) in;
layout(triangles, max_vertices=64, max_primitives=64) out;

taskPayloadSharedEXT Payload payload;

void main() {
  // Unpack meshlet data
  // Emit vertices and primitives
}
```

## 1.4 Shadow Techniques

### Technique #1: Traditional Cascaded Shadow Maps

**Configuration** (`game/graphics/sceneglobals.h:33-36`):
```cpp
static constexpr uint32_t V_SHADOW_WIDTH  = 2048;
static constexpr uint32_t V_SHADOW_HEIGHT = 2048;
static constexpr size_t   NUM_SHADOWMAP_LAYERS = 2; // Cascades
```

**Implementation**:
- 2 cascades for directional light
- Depth format: D16/D24S8/D32F (best available)
- PCF filtering in fragment shader
- Normal bias: 0.0015

### Technique #2: Virtual Shadow Maps (VSM)

**Location**: `shader/virtual_shadow/`

**Architecture**:
```
┌──────────────────┐
│  Page Table      │ 4096×4096 virtual pages
│  R32UI texture   │ Stores physical page indices
└──────────────────┘
         ↓
┌──────────────────┐
│  Page Pool       │ N×128×128 physical pages
│  Depth texture   │ Allocated on-demand
└──────────────────┘
```

**Pipeline** (`game/graphics/worldview.cpp:1069-1095`):
```cpp
void WorldView::drawVirtualShadow() {
  // 1. Clear and mark
  vsmClearPageTable();
  vsmMarkVisiblePages();  // From camera frustum

  // 2. Allocate
  vsmCullLights();        // Per-tile light culling
  vsmAllocatePages();     // GPU page allocator

  // 3. Render
  vsmRenderPages();       // Draw to virtual pages

  // 4. Apply
  vsmFogPass();           // Volumetric shadows
}
```

**Benefits**:
- Effectively infinite shadow resolution
- Page-level LOD
- Supports omni-directional lights
- GPU-driven allocation

### Technique #3: Ray-Traced Shadow Maps (Experimental)

**Location**: `shader/rtsm/`

**Software Rasterization**:
- Runs entirely in compute shaders
- Meshlet-based primitive processing
- Two-level tile binning (128² large, 32² small)

**Pipeline**:
1. **rtsmCulling**: Frustum cull meshlets
2. **rtsmMeshletCull**: Per-meshlet visibility
3. **rtsmPrimCull**: Triangle culling and clipping
4. **rtsmRaster**: Software triangle rasterization
5. **rtsmRendering**: Generate final shadow map

**Use Case**: Fallback for hardware without RT support, or high-quality omni shadows

## 1.5 Global Illumination

### Probe-Based Ray Tracing

**Location**: `shader/lighting/rt/`

**System Overview**:
```
World Space
     ↓
┌──────────────────────────────┐
│  Probe Grid (65k max)        │
│  Dynamic allocation          │
│  Spherical harmonics (SH)    │
└──────────────────────────────┘
     ↓ (256 rays/probe)
┌──────────────────────────────┐
│  Ray Tracing (RT cores)      │
│  G-buffer: diffuse, normal   │
└──────────────────────────────┘
     ↓
┌──────────────────────────────┐
│  Irradiance Computation      │
│  Temporal accumulation       │
└──────────────────────────────┘
     ↓
┌──────────────────────────────┐
│  Scene Lighting              │
│  Probe interpolation (4-8)   │
└──────────────────────────────┘
```

**Probe Allocation** (`shader/lighting/rt/probe_allocation.comp`):
- Allocates probes in camera-visible areas
- Grid spacing adapts to scene density
- Temporal stability via reuse

**Probe Trace** (`shader/lighting/rt/probe_trace.comp`):
```glsl
layout(local_size_x = 256) in;

void main() {
  uint probeId = gl_WorkGroupID.x;
  uint rayId   = gl_LocalInvocationID.x;

  vec3 probePos = fetchProbePosition(probeId);
  vec3 rayDir   = sphericalFibonacci(rayId, 256);

  // Trace ray
  rayQueryEXT rq;
  rayQueryInitializeEXT(rq, tlas, 0, 0xFF,
                        probePos, 0.01, rayDir, 1000.0);

  while(rayQueryProceedEXT(rq)) { /* traversal */ }

  // Write hit data
  writeProbeRay(probeId, rayId, hitData);
}
```

**Benefits**:
- Real-time GI with hardware RT
- Graceful degradation (disables if no RT)
- Dynamic lighting response

## 1.6 Advanced Rendering Features

### Mesh Shader Pipeline

**Location**: `shader/materials/main.mesh`

**Advantages over Vertex Shaders**:
1. **Reduced Vertex Processing**: Transform vertices once per meshlet
2. **Better Culling**: Per-meshlet backface/frustum culling
3. **Custom Primitive Output**: Flexible topology

**Meshlet Structure** (`game/graphics/mesh/submesh/packedmesh.h:20-25`):
```cpp
enum {
  MaxVert     = 64,  // Max vertices per meshlet
  MaxPrim     = 64,  // Max triangles per meshlet
  MaxInd      = 192, // MaxPrim * 3
  MaxMeshlets = 16,  // Max meshlets per submesh
};
```

**Packed Format**:
```
Meshlet Data (per meshlet):
├─ Vertex positions [MaxVert]
├─ Vertex attributes [MaxVert]
├─ Primitive indices [MaxInd]
└─ Metadata in last primitive:
   ├─ Vertex count (6 bits)
   └─ Primitive count (6 bits)
```

### Hierarchical Z-Buffer (HiZ)

**Purpose**: GPU occlusion culling

**Implementation** (`shader/hiz/`):
```cpp
// Build HiZ pyramid
void hiZPot() {
  // First pass: copy depth to mip 0
  imageStore(hiZ, ivec2(pixel), depth);
}

void hiZMip() {
  // Subsequent passes: max of 4 parent pixels
  float d0 = texelFetch(hiZPrev, parent + ivec2(0,0), 0).r;
  float d1 = texelFetch(hiZPrev, parent + ivec2(1,0), 0).r;
  float d2 = texelFetch(hiZPrev, parent + ivec2(0,1), 0).r;
  float d3 = texelFetch(hiZPrev, parent + ivec2(1,1), 0).r;
  float maxDepth = max(max(d0,d1), max(d2,d3));
  imageStore(hiZ, ivec2(pixel), maxDepth);
}
```

**Usage in Culling**:
- Test object AABB against HiZ pyramid
- If occluded, skip draw call
- Reduces overdraw and vertex processing

### Physically-Based Sky

**Based On**: ["Scalable and Production Ready Sky and Atmosphere" (Hillaire, 2020)](https://sebh.github.io/publications/egsr2020.pdf)

**LUT Precomputation**:
1. **Transmittance LUT**: Atmospheric density integration
2. **Multi-Scattering LUT**: Multiple light bounces
3. **View LUT**: Per-frame view-dependent sky appearance
4. **Cloud LUT**: 3D volumetric clouds

**Features**:
- Physically accurate Rayleigh/Mie scattering
- Aerial perspective
- Volumetric cloud shadows
- Day/night cycle support

### Anti-Aliasing: CMAA2

**Conservative Morphological Anti-Aliasing 2**

**Location**: `shader/antialiasing/cmaa2/`

**Algorithm**:
1. **Edge Detection**: Find geometric edges in final image
2. **Shape Classification**: Classify edge patterns (L, T, X, etc.)
3. **Color Blending**: Conservative blend along edges
4. **Deferred Output**: Apply AA to final framebuffer

**Benefits**:
- Post-process (works with any geometry)
- Lower cost than MSAA
- No temporal artifacts

---

# Scene Management

## 2.1 Entity System Architecture

### Base Class: Vob

**File**: `game/world/objects/vob.h`

**Design**: Traditional OOP hierarchy, not ECS

```cpp
class Vob {
protected:
  World*           world  = nullptr;
  Vob*             parent = nullptr;
  Tempest::Matrix4x4 localTransform;
  Tempest::Matrix4x4 transform;

  std::unique_ptr<AbstractVisual> visual;
  // ...
};
```

**Subclasses**:
```
Vob (base)
├── StaticObj          - Non-interactive decorative objects
├── Item               - Pickable items (weapons, potions, etc.)
├── Interactive        - Doors, chests, beds, switches
│   └── FirePlace      - Campfires and torches
├── Npc                - Characters (player, NPCs, monsters)
└── AbstractTrigger    - Invisible gameplay triggers
    ├── Trigger
    ├── TriggerScript
    ├── MoveTrigger
    └── ZoneTrigger
```

### World Container

**File**: `game/world/world.h`

**Major Subsystems**:
```cpp
class World {
  WorldObjects  wobj;      // Entity management
  DynamicWorld* wdynamic;  // Physics
  WorldView*    wview;     // Rendering
  WayMatrix*    wmatrix;   // Pathfinding/waypoints
  GlobalEffects* globFx;   // Screen effects
  WorldSound    wsound;    // 3D audio

  // BSP tree from Gothic
  struct {
    std::vector<zenkit::BspNode>   nodes;
    std::vector<zenkit::BspSector> sectors;
    std::vector<BspSector>         sectorsData;
  } bsp;
};
```

## 2.2 Spatial Partitioning

### K-d Tree: SpaceIndex

**File**: `game/world/spaceindex.h`

**Template Class**:
```cpp
template<class T>
class SpaceIndex final : public BaseSpaceIndex {
public:
  void add(T* value);
  void remove(T* value);

  template<class Func>
  void find(const Vec3& pos, float radius, const Func& callback);
};
```

**Implementation** (`game/world/spaceindex.cpp:159-204`):
- Alternates split axis (X, Y, Z)
- Separates dynamic vs static objects
- Dynamic objects always checked (not indexed)
- Binary space partitioning with AABB tests

**Usage Example**:
```cpp
SpaceIndex<Interactive> interactiveObj;
SpaceIndex<Item>        items;

// Find all interactive objects within 200 units of player
interactiveObj.find(playerPos, 200.0f, [](Interactive* obj) {
  // Process object
});
```

### BSP Tree Integration

**Purpose**: Room/sector identification for guilds and ambient zones

**Structure** (`game/world/world.h:200-209`):
```cpp
struct {
  std::vector<zenkit::BspNode>   nodes;
  std::vector<zenkit::BspSector> sectors;
  std::vector<BspSector>         sectorsData;
} bsp;
```

**Query** (`game/world/world.cpp:431-454`):
```cpp
std::string_view World::roomAt(const Vec3& pos) const {
  // Traverse BSP tree to find sector
  for(size_t i = 0; i < bsp.nodes.size(); ) {
    auto& n = bsp.nodes[i];
    auto  d = n.plane.distance(pos);

    if(d > 0)
      i = n.front_index;
    else if(d < 0)
      i = n.back_index;
    else
      break;  // On plane
  }

  return bsp.sectors[leafIndex].name;
}
```

## 2.3 Entity Management

### WorldObjects

**File**: `game/world/worldobjects.h`

**Storage**:
```cpp
class WorldObjects {
  // NPCs
  std::vector<std::unique_ptr<Npc>> npcArr;
  std::vector<std::unique_ptr<Npc>> npcInvalid;  // Dead
  std::vector<Npc*>                 npcNear;     // Cache

  // Items
  std::vector<std::unique_ptr<Item>> itemArr;
  SpaceIndex<Item>                   items;

  // Interactive
  SpaceIndex<Interactive> interactiveObj;

  // Vob hierarchy
  std::vector<std::unique_ptr<Vob>> rootVobs;
};
```

### NPC Distance-Based LOD

**File**: `game/world/worldobjects.cpp:210-227`

```cpp
void WorldObjects::updateAnimation() {
  const Vec3 plPos = player->position();

  for(auto& npc : npcArr) {
    float dist = (npc->position() - plPos).length();

    if(dist < 3000)
      npc->setProcessPolicy(Npc::AiNormal);   // Full update
    else if(dist < 6000)
      npc->setProcessPolicy(Npc::AiFar);      // Reduced
    else
      npc->setProcessPolicy(Npc::AiFar2);     // Minimal
  }
}
```

**Policies**:
- **AiNormal**: Full AI, animation, collision (< 30m)
- **AiFar**: Simplified AI, no per-frame animation (30-60m)
- **AiFar2**: Minimal processing, no AI updates (> 60m)

### ID-Based References

**System**: Numeric IDs for serialization

```cpp
class Npc {
  size_t id = 0;  // Unique ID
};

// Lookup by ID
Npc* WorldObjects::npcById(size_t id) {
  for(auto& npc : npcArr)
    if(npc->id == id)
      return npc.get();
  return nullptr;
}
```

**Benefits**:
- Stable references across save/load
- No pointer serialization issues
- Avoids dangling pointers

## 2.4 Scene Serialization

### Save File Format

**Format**: ZIP archive (miniz library)

**Structure**:
```
save_file.sav (ZIP)
├── worlds/[worldname]/
│   ├── version          - Save format version
│   ├── world            - World-level data
│   ├── npc/0            - NPC 0 state
│   ├── npc/1            - NPC 1 state
│   ├── npc/...
│   ├── items            - All item states
│   ├── mobsi            - Interactive object states
│   ├── triggerEvents    - Pending trigger events
│   └── routines         - Mob daily routines
├── game.sav             - Game session state
└── session.sav          - Script variables
```

### Serialization System

**File**: `game/game/serialize.h`

**Context-Aware Pointer Serialization**:
```cpp
class Serialize {
  World* world;  // Context for resolving IDs

  template<class T>
  void write(T* pointer) {
    if(!pointer) {
      write(uint32_t(0));
    } else {
      write(pointer->id());  // Convert to ID
    }
  }

  template<class T>
  T* read() {
    uint32_t id = read<uint32_t>();
    if(id == 0) return nullptr;
    return world->objectById<T>(id);  // Resolve ID
  }
};
```

### World Save/Load

**Save** (`game/world/world.cpp:188-198`):
```cpp
void World::save(Serialize& fout) {
  fout.setEntry("worlds/", wname, "/world");

  // Save BSP guild assignments
  fout.write(uint32_t(bsp.sectorsData.size()));
  for(size_t i = 0; i < bsp.sectorsData.size(); ++i) {
    fout.write(bsp.sectors[i].name, bsp.sectorsData[i].guild);
  }

  // Save all entities (delegates to WorldObjects)
  wobj.save(fout);
}
```

**Load** (`game/world/world.cpp:200-218`):
```cpp
void World::load(Serialize& fin) {
  fin.setEntry("worlds/", wname, "/world");

  // Load BSP guild assignments
  uint32_t size = 0;
  fin.read(size);
  bsp.sectorsData.resize(size);
  for(uint32_t i = 0; i < size; ++i) {
    std::string name;
    fin.read(name, bsp.sectorsData[i].guild);
  }

  // Load all entities
  wobj.load(fin);
}
```

---

# Physics System

## 3.1 Bullet Physics Integration

### Library Configuration

**File**: `CMakeLists.txt:131-163`

```cmake
set(BULLET2_MULTITHREADING ON)
set(BUILD_BULLET3 OFF)  # Use Bullet2 only
add_subdirectory(lib/bullet3)
target_link_libraries(Gothic2Notr
  BulletDynamics
  BulletCollision
  LinearMath)
```

### Wrapper Architecture

**File**: `game/physics/collisionworld.h`

```cpp
class CollisionWorld {
protected:
  std::unique_ptr<btCollisionConfiguration>  conf;
  std::unique_ptr<Broadphase>                broadphase;
  std::unique_ptr<btCollisionDispatcher>     dispatcher;
  std::unique_ptr<btConstraintSolver>        solver;
  std::unique_ptr<btDiscreteDynamicsWorld>   world;
};
```

**Unit Conversion**:
- Bullet: meters
- Game: centimeters
- `toMeters(x) = x * 0.01f`
- `toCentimeters(x) = x * 100.0f`

## 3.2 Collision System

### Broad Phase: DBVT

**Custom Broadphase** (`game/physics/collisionworld.cpp:32-76`):
```cpp
struct CollisionWorld::Broadphase : btDbvtBroadphase {
  // Optimized ray testing with pre-allocated stack
  btAlignedObjectArray<const btDbvtNode*> rayTestStk;

  void rayTest(const btVector3& rayFrom,
               const btVector3& rayTo,
               btBroadphaseRayCallback& callback) {
    // Traverse DBVT with stack
    btDbvt::rayTest(m_sets[0].m_root, rayFrom, rayTo,
                    callback, rayTestStk);
    btDbvt::rayTest(m_sets[1].m_root, rayFrom, rayTo,
                    callback, rayTestStk);
  }
};
```

**Features**:
- Dynamic Bounding Volume Tree (DBVT)
- Separate sets for static/dynamic objects
- Deferred collision updates for performance

### Collision Categories

**File**: `game/physics/dynamicworld.h:47-54`

```cpp
enum Category {
  C_Null      = 1,  // No collision
  C_Landscape = 2,  // Static world
  C_Water     = 3,  // Water volumes
  C_Object    = 4,  // Interactive objects
  C_Ghost     = 5,  // Character controllers
  C_Item      = 6,  // Dynamic items
};
```

**Filtering**:
```cpp
obj->getBroadphaseProxy()->m_collisionFilterGroup = C_Ghost;
obj->getBroadphaseProxy()->m_collisionFilterMask  = C_Landscape;
obj->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
```

### Collision Shapes

**1. Capsule (Characters)**:
```cpp
struct HumShape : btCapsuleShape {
  HumShape(btScalar radius, btScalar height)
    : btCapsuleShape(toMeters(radius), toMeters(height)) {}
};
```

**2. Box (Items)**:
```cpp
btBoxShape* box = new btBoxShape(btVector3(
  toMeters(halfExtX),
  toMeters(halfExtY),
  toMeters(halfExtZ)
));
```

**3. BVH Triangle Mesh (World)**:
```cpp
btBvhTriangleMeshShape* mesh = new btBvhTriangleMeshShape(
  triangleArray,
  true,   // Use quantization
  true    // Build BVH
);
```

## 3.3 Rigid Body Dynamics

### Dynamic World

**File**: `game/physics/dynamicworld.h`

**Gravity**:
```cpp
static constexpr float gravityMS = 9.8f;  // m/s²
static constexpr float gravity   = gravityMS * 100.f / (1000.f * 1000.f);  // cm/ms²
```

### Item Physics

**Types** (`game/physics/dynamicworld.h:255-259`):
```cpp
enum ItemType : uint8_t {
  IT_Static,   // Collision only (mass = 0)
  IT_Movable,  // Can be pushed (mass = 0, kinematic)
  IT_Dynamic,  // Full physics (mass > 0)
};
```

**Mass Calculation** (`game/physics/dynamicworld.cpp:703-714`):
```cpp
float density = materialDensity(mat);  // kg/m³
float volume  = (hX * 0.01f) * (hY * 0.01f) * (hZ * 0.01f);  // m³
float mass    = density * volume;  // kg

btVector3 inertia;
shape->calculateLocalInertia(mass, inertia);
```

### Material Properties

**Friction** (`game/physics/dynamicworld.cpp:835-856`):
| Material | Friction |
|----------|----------|
| METAL    | 1.1      |
| STONE    | 0.65     |
| WOOD     | 0.4      |
| EARTH    | 0.4      |
| SNOW     | 0.2      |
| WATER    | 0.01     |

**Density** (`game/physics/dynamicworld.cpp:858-877`):
| Material | Density (kg/m³) |
|----------|-----------------|
| METAL    | 7800            |
| STONE    | 2200            |
| WOOD     | 700             |
| EARTH    | 1500            |
| WATER    | 1000            |

## 3.4 Character Physics

### NPC Body

**File**: `game/physics/dynamicworld.cpp:38-62`

```cpp
struct NpcBody : btRigidBody {
  Tempest::Vec3 pos;      // Centimeters
  float r, h;             // Radius, height
  float rX, rZ;           // Collision extents
  bool enable = true;     // Can be disabled
  size_t frozen = 0;      // Optimization flag
  uint64_t lastMove = 0;  // Last movement tick
};
```

### Movement System

**Swept Collision** (`game/physics/dynamicworld.cpp:980-1014`):
```cpp
enum MoveResult {
  MC_OK,       // Successful move
  MC_Fail,     // Blocked
  MC_Partial,  // Partial move (slide)
  MC_Skip,     // No collision test needed
};

MoveResult DynamicWorld::moveNpc(NpcBody& npc, Vec3& pos, Vec3& dpos) {
  // Subdivide large movements
  int steps = int(std::ceil(dpos.length() / 20.0f));  // 20cm substeps

  for(int i = 0; i < steps; ++i) {
    Vec3 step = dpos / float(steps);

    // Test capsule sweep
    if(testCollision(npc, pos, pos + step)) {
      // Try to slide along surface
      // ...
    }
  }
}
```

### NPC-NPC Collision Optimization

**File**: `game/physics/dynamicworld.cpp:64-329`

**Frozen List System**:
```cpp
struct NpcBodyList {
  std::vector<Record> body;    // Active NPCs
  std::vector<Record> frozen;  // Static NPCs (sorted by X)
  float maxR = 0;               // Largest radius
};

void updateNpc() {
  // If NPC hasn't moved in 1 second
  if(npc.lastMove + 1000 < currentTime) {
    moveTo(frozen, npc);  // Move to frozen list
    sortFrozen();         // Sort by X for binary search
  }
}
```

**Collision Test**:
```cpp
bool testNpcNpc(NpcBody& a, NpcBody& b) {
  float dX = a.pos.x - b.pos.x;
  float dZ = a.pos.z - b.pos.z;
  float r  = a.r + b.r;

  if(dX*dX + dZ*dZ > r*r)
    return false;  // No collision

  float dY = std::abs(a.pos.y - b.pos.y);
  float h  = (a.h + b.h) * 0.5f;

  return dY < h;  // Vertical overlap?
}
```

## 3.5 Raycasting

### Ray Types

**1. Land Ray** (ground detection):
```cpp
RayLandResult landRay(const Vec3& from, const Vec3& to) {
  // Returns: position, normal, material, sector
}
```

**2. Water Ray** (water surface):
```cpp
bool waterRay(const Vec3& from, const Vec3& to, Vec3& out) {
  // Tests against water volumes only
}
```

**3. Generic Ray** (line-of-sight):
```cpp
RayLandResult ray(const Vec3& from, const Vec3& to) {
  // Filters: C_Landscape | C_Object
  // Back-face culling enabled
}
```

**4. NPC Ray** (character detection):
```cpp
RayQueryResult rayNpc(const Vec3& from, const Vec3& to) {
  // Two-phase: landscape ray + NPC cylinder test
  return { landHit, npcHit };
}
```

**5. Sound Occlusion Ray** (audio):
```cpp
float soundOcclusion(const Vec3& from, const Vec3& to) {
  // Counts wall thickness for attenuation
  // occlusion = thickness / 1.5m
}
```

### Ray Callback Example

```cpp
struct ClosestRayResultCallback : btCollisionWorld::ClosestRayResultCallback {
  float addSingleResult(btCollisionWorld::LocalRayResult& rayResult,
                        bool normalInWorldSpace) override {
    // Extract material from triangle mesh
    const btCollisionObject* obj = rayResult.m_collisionObject;
    int partId = rayResult.m_localShapeInfo->m_shapePart;
    int triId  = rayResult.m_localShapeInfo->m_triangleIndex;

    // Get material from PhysicVbo
    material = physicVbo->getMaterial(partId, triId);

    return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
  }
};
```

---

# Game Loop Architecture

## 4.1 Entry Point and Initialization

**File**: `game/main.cpp:70`

```cpp
int main(int argc, const char** argv) {
  // 1. Logging setup
  setupLogging("log.txt");

  // 2. Crash handler
  CrashLog::setup();

  // 3. Graphics API creation
  auto api    = createGraphicsApi();  // Vulkan/DX12/Metal
  auto device = api->createDevice(selectGPU());

  // 4. Core systems
  Resources resources(device);
  Gothic gothic;
  GameMusic music;

  // 5. Setup scripts
  gothic.setupGlobalScripts();

  // 6. Window and application
  MainWindow window(device);
  Tempest::Application app;

  // 7. Enter main loop
  return app.exec();  // Tempest event loop
}
```

## 4.2 Frame Loop

**File**: `game/mainwindow.cpp:1151-1225`

```cpp
void MainWindow::render() {
  // 1. Update game logic
  const uint64_t dt = tick();
  updateAnimation(dt);
  tickCamera(dt);

  // 2. Wait for GPU
  auto& sync = fence[cmdId];
  if(!sync.wait(0)) {
    std::this_thread::yield();
    return;  // GPU still busy
  }
  Resources::resetRecycled(cmdId);

  // 3. Update UI
  dispatchPaintEvent(uiLayer, atlas);
  uiMesh[cmdId].update(device, uiLayer);

  // 4. Submit GPU commands
  CommandBuffer& cmd = commands[cmdId];
  auto enc = cmd.startEncoding(device);
  renderer.draw(enc, cmdId, swapchain.currentImage(),
                uiMesh[cmdId], numMesh[cmdId], inventory, video);
  device.submit(cmd, sync);
  device.present(swapchain);

  cmdId = (cmdId + 1) % MaxFramesInFlight;

  // 5. Frame rate limiting
  auto t = Application::tickCount();
  if(isInMenu && t - time < 16ms)
    Application::sleep(16ms - (t - time));  // 60 FPS cap for menu

  fps.push(t - time);
}
```

## 4.3 Game Tick Hierarchy

```
MainWindow::tick(dt)
├─ video.tick()                // Video playback
├─ dialogs.tick(dt)            // Dialog system
├─ inventory.tick(dt)          // Inventory UI
├─ Gothic::tick(dt)            // Game session
│   └─ GameSession::tick(dt)
│       ├─ GameScript::tick(dt)    // Daedalus VM
│       └─ World::tick(dt)
│           ├─ WorldObjects::tick(dt)   // NPCs, items
│           ├─ DynamicWorld::tick(dt)   // Physics
│           ├─ WorldView::tick(dt)      // Graphics
│           ├─ WorldSound::tick(dt)     // Audio
│           └─ GlobalEffects::tick(dt)  // Screen FX
├─ player.tickFocus()          // Interaction target
├─ tickMouse()                 // Mouse input
└─ player.tickMove(dt)         // Player movement
```

## 4.4 Frame Timing

**Double Buffering**:
```cpp
static constexpr uint8_t MaxFramesInFlight = 2;
uint8_t cmdId = 0;  // Cycles 0, 1, 0, 1, ...
```

**Delta Time Clamping**:
```cpp
uint64_t tick() {
  auto dt = currentTime - lastTick;

  if(dt < 5)   return 0;   // Min 5ms (200 FPS logic cap)
  if(dt > 50)  dt = 50;    // Max 50ms (prevent physics explosion)

  lastTick = currentTime;
  return dt;
}
```

**Time Scaling**:
```cpp
// Gothic time flows faster than real time
constexpr float timeScale = 14.5f;
worldTime += dt * timeScale;
```

---

## Key Takeaways

### Rendering
- ✅ Modern hybrid pipeline with RT/mesh shader support
- ✅ Multiple shadow techniques with graceful degradation
- ✅ GPU-driven architecture minimizes CPU overhead
- ⚠️ Complex shader system with many permutations

### Scene Management
- ✅ Clean OOP hierarchy, easy to understand
- ✅ Spatial optimization with k-d tree and BSP
- ✅ Robust save/load with context-aware serialization
- ⚠️ No streaming, all entities loaded at once

### Physics
- ✅ Well-integrated Bullet with custom optimizations
- ✅ Material-based properties (friction, density)
- ✅ Efficient NPC collision via spatial partitioning
- ⚠️ Character physics separate from Bullet (complexity)

### Game Loop
- ✅ Clean initialization sequence
- ✅ Proper frame pacing and GPU synchronization
- ✅ Hierarchical tick system with clear responsibilities
- ✅ Delta time clamping prevents physics issues

---

**Next**: See `03-asset-animation-systems.md` for deep dive into asset pipeline and animation architecture.
