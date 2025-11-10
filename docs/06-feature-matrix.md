# OpenGothic Engine - Feature Matrix

## Rendering Features

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Graphics API** |
| Vulkan | ✅ | ✅ | ✅ | ✅ | Tempest abstraction | High |
| DirectX 12 | ✅ | ✅ | ✅ | ❌ | Tempest abstraction | High |
| Metal | ✅ | ✅ | ⚠️ | ⚠️ | Tempest abstraction | High |
| OpenGL | ❌ | ✅ | ⚠️ | ✅ | Not needed | - |
| **Rendering Pipeline** |
| Deferred Rendering | ✅ | ✅ | ✅ | ⚠️ | G-buffer + lighting pass | Medium |
| Forward Rendering | ✅ | ✅ | ✅ | ✅ | For transparents | Medium |
| Clustered/Tiled | ✅ | ⚠️ | ✅ | ❌ | Meshlet-based | High |
| GPU-Driven | ✅ | ❌ | ⚠️ | ❌ | Compute culling | High |
| **Shadows** |
| Shadow Maps | ✅ | ✅ | ✅ | ✅ | CSM (2 cascades) | Medium |
| Virtual Shadow Maps | ✅ | ❌ | ✅ | ❌ | Clipmap paging | Complex |
| RT Shadows | ✅ | ✅ | ✅ | ❌ | Ray query | Complex |
| Software RT Shadows | ✅ | ❌ | ❌ | ❌ | Compute rasterizer | Complex |
| **Lighting** |
| Directional Lights | ✅ | ✅ | ✅ | ✅ | Sun/moon | Simple |
| Point/Spot Lights | ✅ | ✅ | ✅ | ✅ | Dynamic lights | Medium |
| Area Lights | ❌ | ✅ | ✅ | ⚠️ | Not needed | - |
| **Global Illumination** |
| Probe-Based GI | ✅ | ❌ | ⚠️ | ❌ | RT + SH | Complex |
| Lightmaps | ⚠️ | ✅ | ✅ | ✅ | Gothic has baked | Simple |
| Realtime GI | ✅ | ✅ | ✅ | ❌ | Via probes | Complex |
| **Advanced Features** |
| Ray Tracing | ✅ | ✅ | ✅ | ❌ | Vulkan ray query | Complex |
| Mesh Shaders | ✅ | ❌ | ⚠️ | ❌ | VK_EXT_mesh_shader | Complex |
| Bindless Textures | ✅ | ❌ | ⚠️ | ❌ | Descriptor indexing | Medium |
| HiZ Occlusion | ✅ | ❌ | ✅ | ❌ | Compute mipmap | Medium |
| **Post-Processing** |
| SSAO | ✅ | ✅ | ✅ | ✅ | Compute shader | Medium |
| SSR | ✅ | ✅ | ✅ | ✅ | Water only | Medium |
| Anti-Aliasing (CMAA2) | ✅ | ❌ | ❌ | ❌ | Compute shader | Medium |
| TAA | ❌ | ✅ | ✅ | ⚠️ | Not implemented | Medium |
| Bloom | ⚠️ | ✅ | ✅ | ✅ | Minimal | Simple |
| Tonemapping | ✅ | ✅ | ✅ | ✅ | HDR to LDR | Simple |
| **Sky/Atmosphere** |
| Physical Sky | ✅ | ⚠️ | ⚠️ | ❌ | Based on Hillaire | Complex |
| Volumetric Clouds | ✅ | ⚠️ | ✅ | ❌ | 3D noise | Complex |
| Volumetric Fog | ✅ | ✅ | ✅ | ⚠️ | Epipolar/3D | Complex |

## Scene Management

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Entity System** | OOP Hierarchy | GameObject/ECS | Actor/Component | Node Tree | Vob base class | Medium |
| **Spatial Partitioning** | K-d Tree + BSP | Octree | Various | Octree | SpaceIndex | Medium |
| **LOD System** | Distance-based | ✅ | ✅ | ✅ | NPC processing tiers | Medium |
| **Streaming** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |
| **Prefabs** | ⚠️ | ✅ | ✅ | ✅ | Gothic VOB trees | Medium |

## Physics

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Engine** | Bullet 2 | PhysX | Chaos/PhysX | Godot Physics | Wrapper | Medium |
| **Rigid Bodies** | ✅ | ✅ | ✅ | ✅ | Dynamic items | Medium |
| **Character Controller** | ✅ (custom) | ✅ | ✅ | ✅ | Capsule sweep | Medium |
| **Triggers** | ✅ | ✅ | ✅ | ✅ | Ghost objects | Simple |
| **Raycasting** | ✅ | ✅ | ✅ | ✅ | 5 types | Simple |
| **Cloth** | ❌ | ✅ | ✅ | ⚠️ | Not needed | - |
| **Destruction** | ❌ | ⚠️ | ✅ | ❌ | Not needed | - |

## Animation

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Skeletal** | ✅ (96 bones) | ✅ | ✅ | ✅ | Pose class | Medium |
| **Blending** | ✅ (multi-layer) | ✅ | ✅ | ✅ | SLERP | Medium |
| **State Machine** | ✅ (pattern-based) | ✅ | ✅ | ✅ | AnimationSolver | Medium |
| **IK** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |
| **Root Motion** | ✅ | ✅ | ✅ | ✅ | Extracted | Medium |
| **Events** | ✅ (extensive) | ✅ | ✅ | ✅ | 20+ event types | Medium |
| **Compression** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |

## Audio

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **3D Audio** | ✅ | ✅ | ✅ | ✅ | Custom | Medium |
| **Occlusion** | ✅ | ⚠️ | ✅ | ⚠️ | Physics raycast | Medium |
| **Adaptive Music** | ✅ | ⚠️ | ⚠️ | ⚠️ | DirectMusic | Complex |
| **Middleware** | Custom DMusic | FMOD/Wwise | Wwise | Custom | 5000 lines | Complex |

## Input

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Action Mapping** | ✅ (dual) | ✅ | ✅ | ✅ | KeyCodec | Simple |
| **Gamepad** | ⚠️ | ✅ | ✅ | ✅ | Partial | Simple |
| **Touch** | ✅ | ✅ | ✅ | ✅ | TouchInput | Simple |
| **Rebinding** | ✅ | ✅ | ✅ | ✅ | INI persistence | Simple |

## Scripting

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Language** | Daedalus | C#/Visual | C++/Blueprint | GDScript/C# | ZenKit VM | N/A |
| **Hot Reload** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |
| **Debugging** | ⚠️ | ✅ | ✅ | ✅ | Log-based | - |
| **External Functions** | 200+ | Full API | Full API | Full API | Manual binding | Medium |

## Asset Pipeline

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Formats** | Gothic (ZenKit) | Extensive | Extensive | Extensive | VFS | Medium |
| **Import** | Automatic | Editor | Editor | Editor | VFS mount | Simple |
| **Hot Reload** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |
| **Compression** | VDF + DXT | Various | Various | Various | ZenKit | Simple |
| **Streaming** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |

## Editor

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Visual Editor** | ❌ | ✅ | ✅ | ✅ | Uses Gothic tools | N/A |
| **Scene Editor** | ❌ | ✅ | ✅ | ✅ | Uses Gothic tools | N/A |
| **Asset Browser** | ❌ | ✅ | ✅ | ✅ | File system | N/A |
| **Profiler** | ⚠️ | ✅ | ✅ | ✅ | FPS counter only | - |
| **Debugger** | Marvin mode | ✅ | ✅ | ✅ | F-key commands | Simple |

## Networking

| Feature | OpenGothic | Unity | Unreal | Godot | Implementation | Complexity |
|---------|------------|-------|--------|-------|----------------|------------|
| **Multiplayer** | ❌ | ✅ | ✅ | ✅ | Not implemented | - |
| **Replication** | ❌ | ✅ | ✅ | ✅ | Not needed | - |

## Platforms

| Platform | OpenGothic | Unity | Unreal | Godot |
|----------|------------|-------|--------|-------|
| **Windows** | ✅ | ✅ | ✅ | ✅ |
| **Linux** | ✅ | ✅ | ✅ | ✅ |
| **macOS** | ✅ | ✅ | ✅ | ✅ |
| **iOS** | ⚠️ | ✅ | ✅ | ✅ |
| **Android** | ❌ | ✅ | ✅ | ✅ |
| **Consoles** | ❌ | ✅ | ✅ | ⚠️ |

## Legend

- ✅ Fully supported
- ⚠️ Partially supported / Limited
- ❌ Not supported / Not applicable

## Priority Rating for Custom Engine

Based on typical game needs:

| Feature | Priority | Reason |
|---------|----------|--------|
| **Core Rendering** | ⭐⭐⭐⭐⭐ | Essential |
| **Physics** | ⭐⭐⭐⭐⭐ | Essential |
| **Animation** | ⭐⭐⭐⭐⭐ | Essential |
| **Audio** | ⭐⭐⭐⭐ | Very important |
| **Input** | ⭐⭐⭐⭐⭐ | Essential |
| **Scripting** | ⭐⭐⭐⭐ | Very important |
| **Editor** | ⭐⭐⭐⭐⭐ | Critical for productivity |
| **Ray Tracing** | ⭐⭐ | Nice to have |
| **Mesh Shaders** | ⭐⭐ | Nice to have |
| **Networking** | ⭐⭐⭐ | Game-dependent |
| **Hot Reload** | ⭐⭐⭐⭐ | Productivity boost |

---

**Next**: See `07-implementation-roadmap.md` for suggested development order.
