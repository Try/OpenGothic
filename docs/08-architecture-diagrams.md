# OpenGothic Engine - Architecture Diagrams

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Application Layer                        │
│                      (main.cpp, mainwindow.cpp)                 │
└────────────┬──────────────────────────────────────┬─────────────┘
             │                                      │
             ↓                                      ↓
┌────────────────────────┐              ┌────────────────────────┐
│    Gothic (Game Core)   │              │   Tempest Framework    │
│   ├─ GameSession        │              │   ├─ Window           │
│   ├─ GameScript         │              │   ├─ Application      │
│   └─ Resources          │              │   └─ Device           │
└────────────┬────────────┘              └────────────┬───────────┘
             │                                        │
             ↓                                        ↓
┌────────────────────────────────────────────────────────────────┐
│                         World Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │ WorldObjects │  │ DynamicWorld │  │  WorldView   │         │
│  │ (Entities)   │  │  (Physics)   │  │ (Rendering)  │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  WayMatrix   │  │ WorldSound   │  │ GlobalEffects│         │
│  │(Pathfinding) │  │   (Audio)    │  │  (Screen FX) │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
└────────────────────────────────────────────────────────────────┘
             │
             ↓
┌────────────────────────────────────────────────────────────────┐
│                     Core Systems Layer                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Renderer │  │  Shaders │  │ Material │  │   Mesh   │       │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │  Physics │  │Animation │  │  Audio   │  │  Input   │       │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
└────────────────────────────────────────────────────────────────┘
             │
             ↓
┌────────────────────────────────────────────────────────────────┐
│                    Library Layer                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │  ZenKit  │  │  Bullet  │  │ Tempest  │  │ DirectMusic│      │
│  │ (Assets) │  │(Physics) │  │(Graphics)│  │  (Music)  │       │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
└────────────────────────────────────────────────────────────────┘
             │
             ↓
┌────────────────────────────────────────────────────────────────┐
│                    Platform Layer                               │
│         Vulkan  │  DirectX 12  │  Metal  │  OpenAL             │
└────────────────────────────────────────────────────────────────┘
```

---

## Rendering Pipeline Flow

```
Frame Start
    │
    ↓
┌────────────────────────────────────────┐
│  1. Game Update (CPU)                  │
│  ├─ tick(dt)                           │
│  ├─ Update animations                  │
│  ├─ Update physics                     │
│  └─ Update AI                          │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  2. Culling (CPU/GPU)                  │
│  ├─ Frustum culling                    │
│  ├─ Distance LOD                       │
│  └─ Occlusion culling (HiZ)            │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  3. Shadow Pass (GPU)                  │
│  ├─ Traditional shadow maps (2048²)    │
│  ├─ Virtual shadow maps (optional)     │
│  └─ Ray-traced shadows (optional)      │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  4. G-Buffer Pass (GPU)                │
│  ├─ Output: Diffuse (RGBA8)            │
│  ├─ Output: Normal (R32U)              │
│  └─ Output: Depth (D16/D24/D32)        │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  5. Lighting Pass (GPU)                │
│  ├─ Directional light (sun/moon)       │
│  ├─ Shadow resolution                  │
│  ├─ Probe-based GI (if RT enabled)     │
│  └─ Point/spot lights                  │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  6. Forward Pass (GPU)                 │
│  ├─ Water (tessellated)                │
│  ├─ Transparent objects                │
│  ├─ Particles                          │
│  └─ Emissive materials                 │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  7. Post-Processing (GPU)              │
│  ├─ Sky rendering                      │
│  ├─ Volumetric fog                     │
│  ├─ SSAO (optional)                    │
│  ├─ SSR (water only)                   │
│  ├─ Anti-aliasing (CMAA2)              │
│  └─ Tonemapping                        │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  8. UI Rendering (GPU)                 │
│  └─ UI quads (inventory, HUD, etc.)    │
└──────────┬─────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────┐
│  9. Present                            │
│  └─ Swap buffers                       │
└────────────────────────────────────────┘
    │
    ↓
Frame End
```

---

## Entity System Hierarchy

```
                    ┌─────────┐
                    │   Vob   │ (Base class)
                    │ (base)  │
                    └────┬────┘
                         │
        ┌────────────────┼────────────────┬────────────────────┐
        │                │                │                    │
   ┌────↓────┐      ┌────↓────┐     ┌────↓─────┐      ┌──────↓──────┐
   │StaticObj│      │  Item   │     │Interactive│      │     Npc     │
   │(decor)  │      │(pickup) │     │(door/chest)│      │(character)  │
   └─────────┘      └─────────┘     └─────┬──────┘      └─────────────┘
                                          │
                                    ┌─────↓──────┐
                                    │ FirePlace  │
                                    │ (torch)    │
                                    └────────────┘

   ┌──────────────────┐
   │AbstractTrigger   │
   │  (triggers)      │
   └────┬─────────────┘
        │
        ├─ Trigger
        ├─ TriggerScript
        ├─ MoveTrigger
        └─ ZoneTrigger
```

---

## World Systems Integration

```
┌──────────────────────────────────────────────────────────────┐
│                          World                                │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                    WorldObjects                         │ │
│  │  ┌────────┐  ┌────────┐  ┌──────────┐                  │ │
│  │  │  NPCs  │  │ Items  │  │Interactive│                  │ │
│  │  └───┬────┘  └───┬────┘  └────┬─────┘                  │ │
│  │      │           │            │                         │ │
│  │      └───────────┴────────────┘                         │ │
│  │                  │                                       │ │
│  │            ┌─────↓──────┐                               │ │
│  │            │SpaceIndex  │ (K-d tree spatial queries)    │ │
│  │            │(K-d tree)  │                               │ │
│  │            └────────────┘                               │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                   DynamicWorld                          │ │
│  │  ┌──────────────────────────────────────────────┐       │ │
│  │  │         Bullet Physics World                 │       │ │
│  │  │  ├─ NPC capsules (custom collision)          │       │ │
│  │  │  ├─ Item rigid bodies                        │       │ │
│  │  │  ├─ World static mesh                        │       │ │
│  │  │  └─ Raycasting                               │       │ │
│  │  └──────────────────────────────────────────────┘       │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                     WorldView                           │ │
│  │  ┌──────────────────────────────────────────────┐       │ │
│  │  │            Renderer                          │       │ │
│  │  │  ├─ VisualObjects (meshes)                   │       │ │
│  │  │  ├─ LightGroup (dynamic lights)              │       │ │
│  │  │  ├─ PfxObjects (particles)                   │       │ │
│  │  │  └─ Sky                                       │       │ │
│  │  └──────────────────────────────────────────────┘       │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                    WorldSound                           │ │
│  │  ├─ 3D positioned sounds                                │ │
│  │  ├─ Ambient sounds (time-scheduled)                     │ │
│  │  └─ Occlusion calculations                              │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## Asset Loading Pipeline

```
User Requests Asset (e.g., "SWORD.3DS")
        │
        ↓
┌────────────────────┐
│ Resources Manager  │
│ Check Cache        │
└────────┬───────────┘
         │
    ┌────↓──────┐
    │ In Cache? │
    └────┬──────┘
         │
    ┌────↓───────┬────────┐
    │ Yes        │   No   │
    │            │        │
    ↓            ↓        │
Return       Query VFS   │
Cached       (ZenKit)    │
Asset            │        │
    ↑            ↓        │
    │    ┌───────────┐   │
    │    │ File      │   │
    │    │ Found?    │   │
    │    └───┬───────┘   │
    │        │           │
    │   ┌────↓─────┬─────↓──┐
    │   │ Yes      │   No   │
    │   │          │        │
    │   ↓          ↓        │
    │ Open    Return null  │
    │ File                 │
    │   │                  │
    │   ↓                  │
    │ ┌──────────────┐    │
    │ │Parse with    │    │
    │ │ZenKit        │    │
    │ │(.3DS→.MRM)   │    │
    │ └──────┬───────┘    │
    │        │             │
    │        ↓             │
    │ ┌──────────────┐    │
    │ │ Convert to   │    │
    │ │ Engine Format│    │
    │ │ (PackedMesh) │    │
    │ └──────┬───────┘    │
    │        │             │
    │        ↓             │
    │ ┌──────────────┐    │
    │ │Upload to GPU │    │
    │ │(VBO, IBO,etc)│    │
    │ └──────┬───────┘    │
    │        │             │
    │        ↓             │
    │ ┌──────────────┐    │
    │ │ Cache Asset  │    │
    │ └──────┬───────┘    │
    │        │             │
    └────────┴─────────────┘
             │
             ↓
       Return Asset
```

---

## Animation System Flow

```
NPC wants to play animation "S_RUN"
        │
        ↓
┌─────────────────────────┐
│   AnimationSolver       │
│   ├─ Get weapon state   │
│   ├─ Get body state     │
│   └─ Resolve pattern    │
│      "1H_SWORD_RUN"     │
└────────┬────────────────┘
         │
         ↓
┌─────────────────────────┐
│    Animation Load       │
│    (from MDS/MSB)       │
│  ┌────────────────────┐ │
│  │ KeyFrames (samples)│ │
│  │ Events (hit/sfx)   │ │
│  │ Movement delta     │ │
│  └────────────────────┘ │
└────────┬────────────────┘
         │
         ↓
┌─────────────────────────┐
│       Pose              │
│  ┌──────────────────┐   │
│  │ Multi-layer      │   │
│  │ blending         │   │
│  │ (SLERP)          │   │
│  └────────┬─────────┘   │
│           ↓             │
│  ┌──────────────────┐   │
│  │ Sample at time t │   │
│  │ for each bone    │   │
│  └────────┬─────────┘   │
│           ↓             │
│  ┌──────────────────┐   │
│  │ Compute world    │   │
│  │ transforms       │   │
│  │ (hierarchy)      │   │
│  └────────┬─────────┘   │
└───────────┼─────────────┘
            │
            ↓
┌───────────────────────────┐
│   Upload to GPU           │
│   (bone matrices buffer)  │
└───────────┬───────────────┘
            │
            ↓
┌───────────────────────────┐
│   Vertex Shader           │
│   ├─ Fetch 4 bone IDs     │
│   ├─ Fetch 4 positions    │
│   ├─ Transform each       │
│   └─ Blend with weights   │
└───────────────────────────┘
            │
            ↓
       Final vertex position
```

---

## Script Execution Flow

```
Game Event (e.g., NPC sees player)
        │
        ↓
┌─────────────────────────────┐
│   NPC Perception System     │
│   ├─ Check perception range │
│   ├─ Line-of-sight check    │
│   └─ Trigger event          │
└────────┬────────────────────┘
         │
         ↓
┌─────────────────────────────┐
│    GameScript               │
│    ├─ Set global_self()     │
│    ├─ Set global_other()    │
│    └─ Get AI state function │
└────────┬────────────────────┘
         │
         ↓
┌─────────────────────────────┐
│    DaedalusVm (ZenKit)      │
│    ├─ Execute bytecode      │
│    ├─ Call external funcs   │
│    │   (ai_output, etc.)   │
│    └─ Return result         │
└────────┬────────────────────┘
         │
         ↓
┌─────────────────────────────┐
│  External Function Handler  │
│  (C++ game code)            │
│  ├─ ai_output()             │
│  │   → Start dialogue       │
│  ├─ ai_gotowp()             │
│  │   → Pathfinding          │
│  └─ npc_setattribute()      │
│      → Modify NPC stats     │
└────────┬────────────────────┘
         │
         ↓
     Game State Updated
```

---

## Memory Management

```
┌─────────────────────────────────────────────────────┐
│              Resources Cache Layer                  │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐               │
│  │ Texture Cache│  │  Mesh Cache  │               │
│  │ (never freed)│  │ (never freed)│               │
│  └──────────────┘  └──────────────┘               │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐               │
│  │  Anim Cache  │  │  Font Cache  │               │
│  │ (never freed)│  │ (never freed)│               │
│  └──────────────┘  └──────────────┘               │
└─────────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────┐
│           GPU Resource Recycling                    │
│                                                     │
│  Frame N:   Resource in use                        │
│             ↓                                       │
│  User calls recycle()                              │
│             ↓                                       │
│  Added to recycled[N % 2]                          │
│                                                     │
│  Frame N+1: Resource potentially in-flight         │
│             (Do nothing)                            │
│                                                     │
│  Frame N+2: GPU finished with resource             │
│             ↓                                       │
│  resetRecycled((N+2) % 2)                          │
│             ↓                                       │
│  vector.clear() → destructors called               │
│             ↓                                       │
│  GPU resources freed                               │
└─────────────────────────────────────────────────────┘
```

---

## Physics Integration

```
┌──────────────────────────────────────────────────────┐
│                 Bullet Physics World                 │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │            Broadphase (DBVT)                   │ │
│  │  ├─ Static set (world mesh, objects)           │ │
│  │  └─ Dynamic set (items)                        │ │
│  └────────────┬───────────────────────────────────┘ │
│               │                                      │
│               ↓                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │         Narrow Phase (Dispatcher)              │ │
│  │  ├─ Capsule vs Mesh (NPCs)                     │ │
│  │  ├─ Box vs Mesh (Items)                        │ │
│  │  └─ Mesh vs Mesh (World collision)             │ │
│  └────────────┬───────────────────────────────────┘ │
│               │                                      │
│               ↓                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │          Solver (Sequential Impulse)           │ │
│  │  ├─ Resolve collisions                         │ │
│  │  ├─ Apply forces                               │ │
│  │  └─ Integrate velocities                       │ │
│  └────────────┬───────────────────────────────────┘ │
└───────────────┼──────────────────────────────────────┘
                │
                ↓
┌───────────────────────────────────────────────────────┐
│              Custom NPC Collision                     │
│  (Separate from Bullet for performance)               │
│                                                       │
│  ┌────────────────────────────────────────────────┐  │
│  │   NpcBodyList                                  │  │
│  │   ├─ Active NPCs (moving)                      │  │
│  │   └─ Frozen NPCs (static, sorted by X)        │  │
│  └────────────┬───────────────────────────────────┘  │
│               │                                       │
│               ↓                                       │
│  ┌────────────────────────────────────────────────┐  │
│  │   Swept Capsule Test                           │  │
│  │   ├─ Check vs world (Bullet)                   │  │
│  │   ├─ Check vs nearby NPCs (custom)             │  │
│  │   └─ Slide on collision                        │  │
│  └────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────┘
```

---

## Data Flow Summary

```
User Input
   │
   ↓
┌──────────────┐
│ Input System │
└──────┬───────┘
       │
       ↓
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Game Logic   │───→│  Scripting   │───→│   Physics    │
│   (tick)     │    │  (Daedalus)  │    │   (Bullet)   │
└──────┬───────┘    └──────────────┘    └──────┬───────┘
       │                                        │
       ↓                                        │
┌──────────────┐                               │
│  Animation   │←──────────────────────────────┘
│   (Pose)     │
└──────┬───────┘
       │
       ↓
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Rendering   │───→│    Audio     │───→│     UI       │
│  (Renderer)  │    │  (3D Sound)  │    │  (ImGui)     │
└──────┬───────┘    └──────────────┘    └──────────────┘
       │
       ↓
   GPU Output
       │
       ↓
    Display
```

---

## Class Dependency Graph

```
Gothic (singleton)
  │
  ├─→ Resources (singleton)
  │     ├─→ ZenKit::Vfs
  │     ├─→ TextureCache
  │     ├─→ MeshCache
  │     └─→ Tempest::Device
  │
  ├─→ GameSession
  │     ├─→ GameScript
  │     │     └─→ ZenKit::DaedalusVm
  │     │
  │     └─→ World
  │           ├─→ WorldObjects
  │           │     ├─→ std::vector<Npc>
  │           │     ├─→ std::vector<Item>
  │           │     └─→ SpaceIndex
  │           │
  │           ├─→ DynamicWorld
  │           │     └─→ btDiscreteDynamicsWorld
  │           │
  │           ├─→ WorldView
  │           │     ├─→ Renderer
  │           │     ├─→ VisualObjects
  │           │     └─→ SceneGlobals
  │           │
  │           ├─→ WorldSound
  │           │     └─→ std::vector<Sound>
  │           │
  │           └─→ WayMatrix
  │                 └─→ Pathfinding graph
  │
  └─→ GameMusic
        └─→ DirectMusic
              ├─→ Segment
              ├─→ Style
              └─→ Mixer
```

---

## Thread Model

```
┌─────────────────────────────────────────────────────┐
│                  Main Thread                        │
│                                                     │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │  Input  │→ │  Logic  │→ │Rendering│            │
│  │ (poll)  │  │ (tick)  │  │ (submit)│            │
│  └─────────┘  └─────────┘  └─────────┘            │
│                                                     │
│  Note: Mostly single-threaded!                     │
└─────────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────┐
│               Background Threads                    │
│                                                     │
│  ┌──────────────────────┐                          │
│  │  Shader Compilation  │ (detached thread)        │
│  │  (async at startup)  │                          │
│  └──────────────────────┘                          │
│                                                     │
│  ┌──────────────────────┐                          │
│  │   Asset Streaming    │ (minimal, mostly sync)   │
│  └──────────────────────┘                          │
└─────────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────┐
│                 GPU Thread                          │
│  (Managed by driver)                               │
│                                                     │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │Culling  │→ │Rendering│→ │ Present │            │
│  │(compute)│  │(graphics)│  │         │            │
│  └─────────┘  └─────────┘  └─────────┘            │
└─────────────────────────────────────────────────────┘
```

Note: OpenGothic is **mostly single-threaded** on CPU, with GPU-heavy workloads.

---

These diagrams provide a visual understanding of OpenGothic's architecture. For implementation details, refer to the corresponding markdown files.
