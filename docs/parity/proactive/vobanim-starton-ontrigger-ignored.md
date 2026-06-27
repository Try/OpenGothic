# zCVobAnimate: `startOn` flag and OnTrigger/OnUntrigger animation toggle are ignored

**Confidence:** High (divergence); fix DEFERRED (no surgical, regression-safe path)

## Original function + address (prose only)

In `Gothic2.exe` the `zCVobAnimate` vob carries two per-object boolean fields:
a configured "start on world load" flag (object offset `0x120`) and a live
"is currently animating / running" flag (object offset `0x121`).

- `zCVobAnimate::zCVobAnimate` (constructor, @`0x00617070`) initializes the
  start-on flag to **1** (true) and the running flag to **0**.
- `zCVobAnimate::Unarchive` (@`0x006172a0`) reads the start-on bool from the
  archive into `0x120`, optionally reads a saved running bool into `0x121`,
  then **only** sets the running flag to 1 and kicks the visual's animation
  (`zCVisualAnimate::StartAni`, reached by RTTI-casting the visual to
  `zCVisualAnimate` and calling vtable slot +0x58 when slot +0x64 yields a
  model) when `(savedRunning) || startOn` is true. So a level-placed
  `zCVobAnimate` with `startOn = FALSE` loads **frozen**.
- `zCVobAnimate::OnTrigger` (@`0x006171f0`) and the private `StartAni`
  (@`0x006171a0`) set running = 1 and start the `zCVisualAnimate` animation.
- `zCVobAnimate::OnUntrigger` (@`0x00617240`) and the private `StopAni`
  (@`0x006171e0`) set running = 0, halting the animation.
- `zCVobAnimate::SetVisual` (@`0x00617130`) re-applies: if `startOn || running`
  it (re)starts the animation, otherwise leaves it stopped.

Net original behavior: a `zCVobAnimate`'s animation is **gated** by the running
flag. It animates on world load only when `startOn` is true (the common default),
and a designer-placed instance with `startOn = false` stays still until it
receives an `OnTrigger` event; an `OnUntrigger` event stops it again.

## OpenGothic file:line

- `game/world/objects/vob.cpp:134-136` — `zCVobAnimate` is loaded as a plain
  `StaticObj` with the comment `// NOTE: engine animates all objects anyway`.
- `game/graphics/meshobjects.cpp:173-174` — every morph-mesh `Mesh`
  unconditionally auto-starts its first morph animation with infinite duration:
  `if(mesh.morph.size()>0) startMMAnim(mesh.morph[0].name,1,uint64_t(-1));`.
- `game/world/worldobjects.cpp` `WorldObjects::execTriggerEvent` — trigger
  events are routed **only** to the `triggers` vector (objects derived from
  `AbstractTrigger`). A `StaticObj` is not in that list, so trigger events whose
  `target` names an animated vob reach nothing.
- `lib/ZenKit/include/zenkit/vobs/Misc.hh:78-99` — `zenkit::VAnimate` already
  exposes the data: `bool start_on{false};` and the save-only
  `bool s_is_running{false};`. OpenGothic never reads either.

## Divergence

OpenGothic treats `zCVobAnimate` as an ordinary static decoration:

1. **`startOn` ignored.** A morph-mesh `zCVobAnimate` always begins animating on
   world load (auto `startMMAnim` in the `Mesh` ctor). The original keeps it
   frozen when `startOn == false`. (Conversely, a skeletal-MODEL
   `zCVobAnimate` gets no animation started in OG at all, since the MODEL branch
   in `ObjVisual::setVisual` never calls `startAnim` — but morph-mesh anim
   vobs are the common case.)
2. **OnTrigger / OnUntrigger ignored.** The start/stop-on-trigger toggle does not
   exist. A `zCVobAnimate` placed with `startOn=false` that is meant to come
   alive only on a quest/mechanism trigger never starts; one that is meant to be
   halted on `OnUntrigger` never stops.
3. **Per-vob running state not persisted.** `s_is_running` is neither read on
   load nor written on save, so animation state is not restored across saves.

## Proposed patch

**DEFERRED.**

Reason: there is no surgical, regression-safe edit. A correct fix requires
cross-subsystem plumbing that the codebase does not currently have hooks for:

- Honoring `start_on` means suppressing the unconditional auto-`startMMAnim` in
  `MeshObjects::Mesh`'s constructor (`game/graphics/meshobjects.cpp:174`), which
  has no per-instance "do not auto-start" knob; a flag would have to be threaded
  `VAnimate.start_on` → a new `VobAnimate` vob class → `ObjVisual` →
  `MeshObjects::Mesh`.
- Honoring `OnTrigger`/`OnUntrigger` requires routing trigger events to a
  non-`AbstractTrigger` vob. `WorldObjects::execTriggerEvent` only iterates the
  `triggers` (AbstractTrigger) list, so a name→vob lookup or a new dispatch path
  is needed to deliver start/stop to the animated vob and call `startMMAnim` /
  stop it.
- A partial fix that only gates `start_on` (without the trigger un-gate) is
  *more* dangerous than the current behavior: any `startOn=false` vob would be
  frozen permanently, because OG cannot yet deliver the `OnTrigger` that would
  start it. That is a behavior regression, not a parity gain.

Recommended scope for a future targeted change: add a dedicated `VobAnimate`
vob class that (a) skips the morph auto-start when `start_on==false`, (b) is
registered so trigger events targeting its name call start/stop, and (c)
serializes `s_is_running`. Until that plumbing exists, leave the current
"animate everything" behavior in place rather than introduce a half-toggle.

`// NOTE: in original-game zCVobAnimate::Unarchive @0x006172a0 / OnTrigger @0x006171f0 /`
`// OnUntrigger @0x00617240 gate animation behind the per-vob running flag (startOn-default`
`// + trigger toggle); OpenGothic auto-animates all morph meshes and ignores the toggle.`
