# OpenGothic Engine - Implementation Roadmap

## Suggested Order for Building a Custom Engine

Based on OpenGothic's architecture and common game engine development practices, here's a recommended implementation order:

---

## Phase 1: Foundation (Month 1-2)

### 1.1 Core Infrastructure

**Goal**: Basic application framework

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Application Loop** | ⭐⭐⭐⭐⭐ | 1 week | None |
| **Window Management** | ⭐⭐⭐⭐⭐ | 1 week | OS APIs |
| **Input System** | ⭐⭐⭐⭐⭐ | 1 week | Window |
| **File I/O** | ⭐⭐⭐⭐⭐ | 1 week | None |
| **Logging** | ⭐⭐⭐⭐⭐ | 2 days | None |
| **Math Library** | ⭐⭐⭐⭐⭐ | 1 week | None (or use GLM) |

**Reference in OpenGothic**:
- `game/main.cpp` - Application entry
- `game/mainwindow.h/cpp` - Window/loop
- `game/utils/keycodec.h` - Input
- `lib/Tempest` - Math and utilities

**Milestones**:
- ✅ Window opens and responds to input
- ✅ Basic game loop running at 60 FPS
- ✅ File loading and logging working

### 1.2 Graphics Abstraction

**Goal**: Render API abstraction layer

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Device/Context** | ⭐⭐⭐⭐⭐ | 2 weeks | Graphics API |
| **Command Buffers** | ⭐⭐⭐⭐⭐ | 1 week | Device |
| **Resources (Buffers/Textures)** | ⭐⭐⭐⭐⭐ | 2 weeks | Device |
| **Shaders** | ⭐⭐⭐⭐⭐ | 1 week | Device |
| **Pipeline State** | ⭐⭐⭐⭐⭐ | 1 week | Shaders |

**Reference in OpenGothic**:
- `lib/Tempest/Engine/` - Complete abstraction
- Choose: Build own or use existing (bgfx, sokol_gfx, Diligent)

**Recommendation**: **Use existing abstraction** (saves 2-3 months)

**Milestones**:
- ✅ Clear screen to color
- ✅ Render single triangle
- ✅ Load and display texture

---

## Phase 2: Basic Rendering (Month 3-4)

### 2.1 Mesh Rendering

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Vertex/Index Buffers** | ⭐⭐⭐⭐⭐ | 1 week | Resources |
| **Basic Vertex Shader** | ⭐⭐⭐⭐⭐ | 3 days | Shaders |
| **MVP Matrices** | ⭐⭐⭐⭐⭐ | 3 days | Math |
| **Mesh Loader (OBJ/glTF)** | ⭐⭐⭐⭐ | 1 week | File I/O |
| **Texture Loading (PNG/JPG)** | ⭐⭐⭐⭐⭐ | 1 week | File I/O |

**Reference in OpenGothic**:
- `game/graphics/mesh/protomesh.h` - Mesh structure
- `game/resources.cpp` - Asset loading
- Use: stb_image, tinyobjloader, cgltf

**Milestones**:
- ✅ Render 3D models
- ✅ Camera movement (WASD + mouse look)
- ✅ Textured meshes

### 2.2 Scene Management

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Scene Graph/Entity System** | ⭐⭐⭐⭐⭐ | 2 weeks | None |
| **Transform Hierarchy** | ⭐⭐⭐⭐⭐ | 1 week | Math |
| **Frustum Culling** | ⭐⭐⭐⭐ | 1 week | Math |
| **Spatial Partitioning** | ⭐⭐⭐⭐ | 1 week | Scene Graph |

**Reference in OpenGothic**:
- `game/world/worldobjects.h` - Entity management
- `game/world/spaceindex.h` - K-d tree
- `game/graphics/dynamic/frustrum.h` - Culling

**Decision Point**: **ECS vs OOP Hierarchy?**
- OpenGothic: OOP (simpler, easier to understand)
- Modern engines: ECS (better performance, more flexible)
- Recommendation: Start with OOP, migrate to ECS later if needed

**Milestones**:
- ✅ Multiple objects in scene
- ✅ Culling improves performance
- ✅ Scene hierarchy (parent-child)

---

## Phase 3: Materials & Lighting (Month 5-6)

### 3.1 Material System

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Material Structure** | ⭐⭐⭐⭐⭐ | 1 week | None |
| **Shader Variants** | ⭐⭐⭐⭐ | 1 week | Shaders |
| **Texture Binding** | ⭐⭐⭐⭐⭐ | 3 days | Textures |
| **Alpha Blending** | ⭐⭐⭐⭐ | 3 days | Pipeline |

**Reference in OpenGothic**:
- `game/graphics/material.h/cpp` - Material classification

**Milestones**:
- ✅ Multiple materials per mesh
- ✅ Transparent objects
- ✅ Alpha testing (cutouts)

### 3.2 Forward Rendering

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Directional Light** | ⭐⭐⭐⭐⭐ | 1 week | Shaders |
| **Point/Spot Lights** | ⭐⭐⭐⭐ | 1 week | Shaders |
| **Basic Shadows** | ⭐⭐⭐⭐ | 2 weeks | Render to texture |
| **Phong/Blinn-Phong** | ⭐⭐⭐⭐⭐ | 3 days | Shaders |

**Reference in OpenGothic**:
- `shader/lighting/direct_light.frag` - Lighting math
- `shader/materials/` - Material shaders

**Milestones**:
- ✅ Lit scenes
- ✅ Basic shadows
- ✅ Multiple light types

---

## Phase 4: Advanced Rendering (Month 7-8)

### 4.1 Deferred Rendering (Optional)

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **G-Buffer Setup** | ⭐⭐⭐ | 1 week | Multiple render targets |
| **Deferred Lighting Pass** | ⭐⭐⭐ | 1 week | G-buffer |
| **Forward+ (hybrid)** | ⭐⭐⭐ | 2 weeks | Compute shaders |

**Reference in OpenGothic**:
- `game/graphics/worldview.cpp:drawGBuffer()` - G-buffer pass
- `shader/lighting/` - Deferred lighting

**Decision**: Skip if not needed (forward is simpler)

### 4.2 Post-Processing

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Framebuffer Setup** | ⭐⭐⭐⭐ | 1 week | Render to texture |
| **Tonemapping** | ⭐⭐⭐⭐ | 2 days | Shaders |
| **FXAA/SMAA** | ⭐⭐⭐ | 1 week | Shaders |
| **Bloom** | ⭐⭐⭐ | 1 week | Gaussian blur |

**Reference in OpenGothic**:
- `shader/antialiasing/` - CMAA2
- Use: Existing FXAA/SMAA implementations

**Milestones**:
- ✅ HDR rendering
- ✅ Anti-aliasing
- ✅ Bloom effect

---

## Phase 5: Physics (Month 9)

### 5.1 Physics Integration

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Physics Library Setup** | ⭐⭐⭐⭐⭐ | 1 week | Bullet/PhysX/Jolt |
| **Rigid Bodies** | ⭐⭐⭐⭐⭐ | 1 week | Physics lib |
| **Character Controller** | ⭐⭐⭐⭐⭐ | 2 weeks | Physics lib |
| **Raycasting** | ⭐⭐⭐⭐⭐ | 3 days | Physics lib |
| **Triggers** | ⭐⭐⭐⭐ | 3 days | Physics lib |

**Reference in OpenGothic**:
- `game/physics/dynamicworld.h/cpp` - Bullet wrapper
- Recommendation: **Use Jolt Physics** (modern, fast, MIT license)

**Milestones**:
- ✅ Objects fall with gravity
- ✅ Collision detection
- ✅ Character can walk/jump

---

## Phase 6: Animation (Month 10)

### 6.1 Skeletal Animation

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Skeleton Structure** | ⭐⭐⭐⭐⭐ | 1 week | None |
| **Animation Data** | ⭐⭐⭐⭐⭐ | 1 week | File I/O |
| **GPU Skinning** | ⭐⭐⭐⭐⭐ | 2 weeks | Shaders |
| **Animation Blending** | ⭐⭐⭐⭐ | 2 weeks | Math |
| **Animation State Machine** | ⭐⭐⭐⭐ | 2 weeks | Logic |

**Reference in OpenGothic**:
- `game/graphics/mesh/skeleton.h` - Bone hierarchy
- `game/graphics/mesh/pose.h` - Pose evaluation
- `game/graphics/mesh/animationsolver.h` - State machine
- Use: Ozz-animation library for data structures

**Milestones**:
- ✅ Character walks/runs
- ✅ Animation blending works
- ✅ State transitions smooth

---

## Phase 7: Audio (Month 11)

### 7.1 Audio System

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Audio Backend** | ⭐⭐⭐⭐⭐ | 1 week | FMOD/Wwise/OpenAL |
| **3D Audio** | ⭐⭐⭐⭐ | 1 week | Audio backend |
| **Music System** | ⭐⭐⭐ | 1 week | Audio backend |
| **Occlusion** | ⭐⭐ | 1 week | Physics raycasts |

**Reference in OpenGothic**:
- `game/world/worldsound.h/cpp` - 3D audio manager
- `game/dmusic/` - Music system (very complex!)
- Recommendation: **Use FMOD or Wwise** (don't build from scratch)

**Milestones**:
- ✅ Sound effects play
- ✅ 3D positional audio
- ✅ Background music

---

## Phase 8: Scripting (Month 12)

### 8.1 Scripting Integration

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Scripting Language** | ⭐⭐⭐⭐ | 2 weeks | Lua/Wren/ChaiScript |
| **Binding Layer** | ⭐⭐⭐⭐ | 2 weeks | Script VM |
| **Hot Reload** | ⭐⭐⭐ | 1 week | File watching |

**Reference in OpenGothic**:
- `game/game/gamescript.h/cpp` - Daedalus VM integration
- Recommendation: **Lua** (widely used, easy to integrate)
- Alternative: **C#** via Mono (more powerful but heavier)

**Milestones**:
- ✅ Scripts can spawn entities
- ✅ Scripts can respond to events
- ✅ Hot reload works

---

## Phase 9: Advanced Features (Month 13-15)

### 9.1 Modern Rendering

| Component | Priority | Effort | Prerequisites |
|-----------|----------|--------|---------------|
| **PBR Materials** | ⭐⭐⭐⭐ | 2 weeks | Advanced shaders |
| **IBL (Image-Based Lighting)** | ⭐⭐⭐ | 1 week | PBR |
| **SSAO** | ⭐⭐⭐ | 1 week | G-buffer/depth |
| **SSR** | ⭐⭐ | 2 weeks | G-buffer |

**Reference in OpenGothic**:
- `shader/ssao/` - SSAO implementation
- Use: LearnOpenGL tutorials for PBR

### 9.2 Ray Tracing (Optional)

| Component | Priority | Effort | Prerequisites |
|-----------|----------|--------|---------------|
| **Acceleration Structures** | ⭐⭐ | 2 weeks | Vulkan RT / DXR |
| **RT Shadows** | ⭐⭐ | 1 week | AS |
| **RT Reflections** | ⭐⭐ | 1 week | AS |
| **RT GI** | ⭐ | 2 weeks | AS |

**Reference in OpenGothic**:
- `game/graphics/rtscene.h` - Acceleration structure
- `shader/lighting/rt/` - RT shaders
- **Recommendation**: Skip unless targeting high-end only

---

## Phase 10: Editor (Month 16-18)

### 10.1 Visual Editor

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **ImGui Integration** | ⭐⭐⭐⭐⭐ | 1 week | GUI lib |
| **Scene View** | ⭐⭐⭐⭐⭐ | 2 weeks | Rendering |
| **Gizmos (Move/Rotate/Scale)** | ⭐⭐⭐⭐⭐ | 2 weeks | Input + rendering |
| **Inspector Panel** | ⭐⭐⭐⭐⭐ | 2 weeks | Reflection |
| **Asset Browser** | ⭐⭐⭐⭐ | 1 week | File I/O |
| **Undo/Redo** | ⭐⭐⭐⭐ | 2 weeks | Command pattern |

**Reference**: Unity, Unreal, Godot editors

**OpenGothic**: No editor (uses original Gothic tools)

**Milestones**:
- ✅ Place objects in scene visually
- ✅ Edit properties in inspector
- ✅ Save/load scenes

---

## Phase 11: Optimization (Month 19-20)

### 11.1 Performance

| Component | Priority | Effort | Dependencies |
|-----------|----------|--------|--------------|
| **Profiler** | ⭐⭐⭐⭐⭐ | 1 week | Tracy/Optick |
| **Mesh LOD** | ⭐⭐⭐⭐ | 1 week | Mesh processing |
| **Occlusion Culling** | ⭐⭐⭐ | 2 weeks | HiZ or software |
| **Multithreading** | ⭐⭐⭐ | 2 weeks | Job system |
| **Memory Pooling** | ⭐⭐⭐ | 1 week | Custom allocators |

**Reference in OpenGothic**:
- Distance-based NPC LOD
- HiZ occlusion culling
- GPU-driven rendering

---

## Timeline Summary

| Phase | Duration | Focus | Key Deliverables |
|-------|----------|-------|------------------|
| **1. Foundation** | 2 months | Core systems | Window, input, file I/O, loop |
| **2. Basic Rendering** | 2 months | Meshes & scenes | 3D models, camera, scene graph |
| **3. Materials & Lighting** | 2 months | Visuals | Lights, shadows, materials |
| **4. Advanced Rendering** | 2 months | Quality | Deferred, post-processing |
| **5. Physics** | 1 month | Interaction | Collision, character controller |
| **6. Animation** | 1 month | Movement | Skeletal, blending, state machine |
| **7. Audio** | 1 month | Sound | 3D audio, music |
| **8. Scripting** | 1 month | Logic | Lua/C# integration |
| **9. Advanced Features** | 3 months | Polish | PBR, SSAO, RT (optional) |
| **10. Editor** | 3 months | Tools | Visual editor |
| **11. Optimization** | 2 months | Performance | Profiling, culling, threading |

**Total: ~19 months** for full-featured engine (with shortcuts)

**Minimum Viable Engine: ~8 months** (Phases 1-6 + basic audio)

---

## Key Lessons from OpenGothic

### What to Do

1. **Use Existing Libraries**:
   - Graphics: Tempest/bgfx/Diligent
   - Physics: Jolt/Bullet/PhysX
   - Audio: FMOD/Wwise
   - Scripting: Lua/C#
   - **Saves 6+ months**

2. **Start Simple**:
   - Forward rendering before deferred
   - OOP before ECS
   - Basic shadows before advanced techniques

3. **Build Incrementally**:
   - Each phase should produce a working demo
   - Test features as you build them
   - Don't move on until current phase works

4. **Prioritize Developer Experience**:
   - Hot reload (critical!)
   - Good logging
   - Profiler integration early
   - Editor as soon as possible

### What to Avoid

1. **Don't Build Everything**:
   - OpenGothic has no editor (uses Gothic's)
   - OpenGothic has no networking
   - OpenGothic has no advanced UI system
   - Focus on your game's needs

2. **Don't Over-Engineer**:
   - OpenGothic uses simple OOP
   - Start with simple solutions
   - Optimize later when needed

3. **Don't Reinvent the Wheel**:
   - OpenGothic uses ZenKit, Bullet, Tempest
   - Standing on shoulders of giants
   - Focus on what makes your engine unique

---

## Recommended Tech Stack

Based on OpenGothic and modern practices:

| Component | OpenGothic | Recommendation |
|-----------|------------|----------------|
| **Graphics** | Tempest | bgfx or Diligent |
| **Physics** | Bullet 2 | Jolt Physics |
| **Audio** | Custom DirectMusic | FMOD Core |
| **Scripting** | Daedalus VM | Lua or C# (Mono) |
| **Asset Loading** | ZenKit | Assimp + stb_image |
| **GUI** | Tempest (custom) | ImGui (editor) + custom (game) |
| **Math** | Tempest | GLM |
| **Profiler** | None | Tracy Profiler |

---

## Conclusion

OpenGothic demonstrates that a well-architected engine can:
- Leverage modern graphics features
- Maintain compatibility with legacy data
- Provide excellent performance
- Be built by a small team

The key is **smart use of libraries** and **clear architecture**.

For your custom engine:
- Start with phases 1-6 (foundation → animation)
- Add editor early (phase 10 after basic features)
- Optimize last (phase 11)
- Skip what you don't need (networking, advanced RT, etc.)

**Good luck building your engine!**

---

**See also**: `06-feature-matrix.md` for feature comparison and priority ratings.
