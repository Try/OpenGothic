# Agent Instructions

OpenGothic is a from-scratch C++20 reimplementation of the original Gothic /
Gothic II engine (ZenGin by Piranha Bytes). The goal is faithful, 1:1 behavioral
parity with the original game.

## Parity: prefer the original binary over guessing

When implementing or fixing engine behavior, **do not guess at original
semantics** — consult the original `Gothic2.exe` via the Ghidra toolchain in
`~/gothic-re/`. 1:1 is the ideal.

Warm decompiler (recommended, sub-second after a one-time ~25s boot):

```sh
~/gothic-re/wde start                # boot once
~/gothic-re/wde strings oCNpc        # find a subsystem by its RTTI class name
~/gothic-re/wde xrefs FUN_00601000   # who references a function
~/gothic-re/wde dec FUN_00601000     # decompile by name or address
~/gothic-re/wde callers <import>     # callers of an imported symbol
~/gothic-re/wde stop                 # frees the project lock
```

ZenGin keeps `oC*` / `zC*` class names in the binary, so a `strings` query for the
class (e.g. `oCGame`, `zCWorld`, `oCNpc`, `oCMobInter`) is the fast entry point;
follow with `xrefs` / `dec`. Full setup and the optional live GhidraMCP loop (the
`ghidra` MCP server, GUI on :8080) are documented in `~/gothic-re/SETUP.md`.

### Parity map

`docs/parity/PARITY_MAP.md` maps original ZenGin classes ↔ OpenGothic classes ↔ open
GitHub issues (bucketed by subsystem). Use it to find which OG file/class owns a bug
and which original class to decompile. It is generated from `docs/parity/name_map.json`
(the editable seed) — see `docs/parity/README.md` for the loop and regeneration
commands. Per-issue analyses live in `docs/parity/findings/`. When you fix a parity
bug, add a `// NOTE: in original-game <fn> @<addr> ...` citation and, ideally, a
findings note.

When you derive behavior from the original, record where it came from with the
codebase's existing parity-comment convention:

```cpp
// NOTE: in original-game ...   (matches the address/behavior in Gothic2.exe)
```

This convention is already used across the source — see `game/camera.cpp`,
`game/world/objects/npc.cpp`, `game/world/collisionzone.cpp`.

## Coding style

Match the existing OpenGothic C++20 conventions in the file you are editing — no
new style regime is imposed by this document. Keep changes minimal and local;
reuse existing engine utilities and the ZenKit (`lib/ZenKit`) format/Daedalus-VM
layer rather than re-parsing game data by hand.

## Build

```sh
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build ./build --target Gothic2Notr -j$(sysctl -n hw.ncpu)
```

Binary: `build/opengothic/Gothic2Notr`. Run with `-g <path-to-Gothic2-data>`.
