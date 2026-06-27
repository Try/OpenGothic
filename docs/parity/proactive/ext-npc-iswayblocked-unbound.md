# Npc_IsWayBlocked is unbound in OpenGothic and always returns "not blocked"

**Confidence:** Medium (divergence is certain; gameplay payoff and a *surgical* 1:1 fix are not — see DEFERRED)

## Original function + address
The original `Npc_IsWayBlocked` external handler lives in
`oGameExternal.cpp` at `Gothic2.exe @0x006e9da0`. Behaviour (prose):

1. Pop the `self` instance (parameter slot 1) and resolve it to an `oCNpc`.
2. RTTI-cast the npc's movement controller to `zCAIPlayer`; if the cast
   succeeds it calls `zCAIPlayer::CheckEnoughSpaceMoveForward(aiPlayer, 0)`
   (`@0x00511700`), and the external returns `1` exactly when that helper
   returns `0` (i.e. "way is blocked" == "not enough forward space").
3. If the npc/controller is missing it returns `0` (not blocked).

`CheckEnoughSpaceMoveForward` pulls the npc's forward (AT) axis out of its
world transform and delegates to `zCAIPlayer::CheckEnoughSpaceMoveDir`
(`@0x00511320`). That routine ray-casts forward (and at a couple of
fanned-out angles) over a distance scaled by the collision radius
(`aiPlayer.radius * ~1.3`) using `zCWorld::TraceRayFirstHit` /
`TraceRayNearestHit`; it returns `0` (blocked) on a hit and `1` when the
path ahead is clear. So `Npc_IsWayBlocked` is a genuine forward-clearance
probe, not a constant.

## OpenGothic file:line
`game/game/gamescript.cpp` — the `bindExternal(...)` table (lines ~158-302).
There is **no** `bindExternal("npc_iswayblocked", ...)` entry, and no
`GameScript::npc_iswayblocked` member anywhere in the tree
(`grep -rin "wayblocked" game/` returns nothing outside this doc).

Unregistered externals fall through to the default handler installed in
`game/gothic.cpp:964`
(`vm.register_default_external([](std::string_view name){ notImplementedRoutine(...); })`),
which only logs `not implemented call [...]` (`game/gothic.cpp:1007`) and
leaves the return register at its default. For an `int`/`bool` query that
means `Npc_IsWayBlocked` **always evaluates to 0 / false** in OpenGothic.

## Divergence
- Original: returns `TRUE` whenever the npc lacks forward space (a wall /
  vob is within ~1.3 collision radii ahead).
- OpenGothic: the external is never bound, so it silently returns `false`
  ("way is clear") for every caller, every time. Any script / mod AI that
  gates on `Npc_IsWayBlocked` (e.g. to stop, sidestep, or pick an alternate
  move when boxed in) will behave as if the path is always open.

## Proposed patch — DEFERRED
Reasons:
1. **Not surgical / not 1:1.** A faithful reimplementation must reproduce
   `CheckEnoughSpaceMoveDir`'s multi-ray, radius-scaled forward probe
   (forward ray + angled rays, distance ≈ `radius*1.3`, "blocked on first
   hit"). OpenGothic exposes only coarser helpers
   (`Npc::testMove(pos)` / `Npc::tryMove(dp, CollisionTest&)` in
   `game/world/objects/npc.h:321-323`, backed by
   `DynamicWorld::testMove/tryMove` in `game/physics/dynamicworld.h:105-107`).
   A single forward `testMove` is only an approximation of the original's
   fan of rays and would diverge in the exact boxed-in cases the external
   is meant to detect — i.e. it risks a *different* wrong answer rather than
   parity. Per "empty beats false positives," an approximate binding is not
   warranted without a verified 1:1 mapping.
2. **Unverified vanilla payoff.** `Npc_IsWayBlocked` is a standard engine
   external but its use by *vanilla* G2 Daedalus content is not confirmed;
   it appears mainly in mod AI. Binding it has uncertain benefit for the
   stock game.

If pursued later, the binding should read (NOTE citation to carry):
`// NOTE: in original-game Npc_IsWayBlocked @0x006e9da0 returns TRUE iff
zCAIPlayer::CheckEnoughSpaceMoveForward(@0x00511700)==0, i.e. a forward
ray-fan (dist ~ radius*1.3) hits geometry; reimplement via the npc's
forward probe, not a constant false.` — and must be validated against the
multi-ray semantics before landing.
