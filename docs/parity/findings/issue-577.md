# Issue #577 — Crash unless meshlets disabled

**Category:** stability/hardware · **Disposition:** OUT-OF-SCOPE for
CLI/config (driver bug; possible shader-side workaround noted below)

## Symptom
On Windows 11, the game hard-crashes on New Game / load unless mesh shaders are
disabled. Reported at commit 26b816e (Feb 2024).

## Root cause (from issue thread)
AMD's **commercial** Vulkan driver (`amdvlk64.dll`) crashes during mesh-shader
pipeline creation: the shader compiler mishandles direct `vec3` struct member
assignments (pos/normal) in mesh-shader output. Does NOT reproduce on Intel,
NVIDIA, or AMD's open-source MESA/RADV driver. This is a driver compiler bug,
not an OpenGothic logic error — it hard-crashes with no Vulkan error code.

## OpenGothic — current state
- Mesh shading is gated by capability + CLI flag:
  - `hasMeshShader()` at `game/gothic.cpp:33-37` (needs `meshShader` +
    `taskShader`).
  - `opts.doMeshShading = CommandLine::inst().isMeshShading()`
    (`game/gothic.cpp:90-92`); toggled by `-ms 0/1` (`game/commandline.cpp:130-134`).
  - consumed in `game/graphics/drawcommands.cpp` (e.g. l.42, 278).
- A user workaround already exists: launch with `-ms 0` to disable mesh shaders.

## Why out-of-scope for this assignment
This is a hardware/driver defect, not CLI/config/sound plumbing. Per clean-room
rules and the FIX-only-when-surgical rule, the appropriate engine fix is a GLSL
change, not config: the thread reports that rewriting the mesh-shader `vec3`
struct assignments component-wise (assign `.x/.y/.z` individually for pos and
normal, as already works for color/uv) avoids the AMD crash on all vendors with
no vendor detection. That belongs in the mesh-shader source under
`shader/`, outside the CLI/config/sound scope of this triage, and the maintainer
was reluctant to carry an indefinite driver workaround.

## Recommendation
- Short term: document `-ms 0` as the supported workaround for affected
  AMDVLK setups.
- Engine fix (separate, shader-side): component-wise `vec3` writes in the
  mesh-shader output path. Track upstream AMD driver bug. Not actioned here.
